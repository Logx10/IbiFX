#include "LiveEngine.h"

#include <algorithm>
#include <cmath>

ModuleChain& LiveEngine::chain()
{
    return m_chain;
}

const ModuleChain& LiveEngine::chain() const
{
    return m_chain;
}

bool LiveEngine::start(AudioDevice::Mode mode, double sampleRate, int blockSize)
{
    // O buffer é dimensionado com folga porque o driver pode entregar blocos
    // maiores que o pedido. Reservar mais do que o esperado custa alguns
    // kilobytes e evita a única coisa que não podemos fazer no callback.
    const std::size_t reserve = static_cast<std::size_t>(blockSize) * 8;
    m_monoBuffer.reserve(reserve);
    m_monoBuffer.assign(reserve, 0.0f);

    const bool started = m_device.start(
        [this](float* output, const float* input, std::size_t frames, std::size_t channels)
        {
            processBlock(output, input, frames, channels);
        },
        mode, sampleRate, blockSize);

    if (!started)
    {
        return false;
    }

    // O sample rate que vale é o negociado com o driver, não o pedido. Os
    // módulos precisam dele para converter segundos em amostras.
    m_chain.prepare(m_device.sampleRate(), blockSize);
    m_chain.reset();

    return true;
}

void LiveEngine::stop()
{
    m_device.stop();
}

bool LiveEngine::isRunning() const
{
    return m_device.isRunning();
}

bool LiveEngine::setParameter(std::size_t moduleIndex, std::size_t parameterIndex, float value)
{
    Command command;
    command.type = Command::Type::SetParameter;
    command.moduleIndex = moduleIndex;
    command.parameterIndex = parameterIndex;
    command.value = value;

    return m_chain.pushCommand(command);
}

bool LiveEngine::setBypassed(std::size_t moduleIndex, bool bypassed)
{
    Command command;
    command.type = Command::Type::SetBypass;
    command.moduleIndex = moduleIndex;
    command.value = bypassed ? 1.0f : 0.0f;

    return m_chain.pushCommand(command);
}

bool LiveEngine::resetModule(std::size_t moduleIndex)
{
    Command command;
    command.type = Command::Type::Reset;
    command.moduleIndex = moduleIndex;

    return m_chain.pushCommand(command);
}

float LiveEngine::inputPeak() const
{
    return m_inputPeak.load(std::memory_order_relaxed);
}

float LiveEngine::outputPeak() const
{
    return m_outputPeak.load(std::memory_order_relaxed);
}

double LiveEngine::sampleRate() const
{
    return m_device.sampleRate();
}

std::size_t LiveEngine::channelCount() const
{
    return m_device.channelCount();
}

std::string LiveEngine::deviceName() const
{
    return m_device.deviceName();
}

std::string LiveEngine::lastError() const
{
    return m_device.lastError();
}

std::size_t LiveEngine::processedBlocks() const
{
    return m_device.processedBlocks();
}

void LiveEngine::processBlock(float* output,
                              const float* input,
                              std::size_t frameCount,
                              std::size_t channelCount)
{
    if (output == nullptr || channelCount == 0)
    {
        return;
    }

    // Se o driver pedir mais do que reservamos, processamos o que cabe e
    // silenciamos o resto. Redimensionar aqui seria alocar na thread de
    // áudio — o remédio seria pior que a doença.
    const std::size_t frames = std::min(frameCount, m_monoBuffer.capacity());

    m_monoBuffer.resize(frames);

    // Desintercala: pega o primeiro canal de entrada. Uma guitarra entrega um
    // sinal só, e é dele que a cadeia mono precisa.
    float entrada = 0.0f;

    if (input != nullptr)
    {
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            const float sample = input[frame * channelCount];
            m_monoBuffer[frame] = sample;
            entrada = std::max(entrada, std::fabs(sample));
        }
    }
    else
    {
        // Sem entrada, alimentamos silêncio. Um delay com eco pendente ainda
        // devolve som, e é assim que se ouve a cauda de um efeito.
        std::fill(m_monoBuffer.begin(), m_monoBuffer.end(), 0.0f);
    }

    m_chain.process(m_monoBuffer);

    float saida = 0.0f;
    for (float sample : m_monoBuffer)
    {
        saida = std::max(saida, std::fabs(sample));
    }

    // Decaimento: o pico antigo cai um pouco a cada bloco, e o novo só o
    // substitui se for maior. Sem isso o medidor travaria no maior pico de
    // sempre; com decaimento rápido demais, ele piscaria e não daria para ler.
    constexpr float kDecay = 0.85f;

    const float picoEntrada = std::max(entrada, m_inputPeak.load(std::memory_order_relaxed) * kDecay);
    const float picoSaida = std::max(saida, m_outputPeak.load(std::memory_order_relaxed) * kDecay);

    m_inputPeak.store(picoEntrada, std::memory_order_relaxed);
    m_outputPeak.store(picoSaida, std::memory_order_relaxed);

    // Reintercala: o mesmo sinal em todos os canais de saída.
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        const float sample = m_monoBuffer[frame];

        for (std::size_t channel = 0; channel < channelCount; ++channel)
        {
            output[frame * channelCount + channel] = sample;
        }
    }

    // Zera o que sobrou, se o driver pediu mais do que conseguimos processar.
    for (std::size_t frame = frames; frame < frameCount; ++frame)
    {
        for (std::size_t channel = 0; channel < channelCount; ++channel)
        {
            output[frame * channelCount + channel] = 0.0f;
        }
    }
}
