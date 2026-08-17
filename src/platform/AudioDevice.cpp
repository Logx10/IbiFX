#include "AudioDevice.h"

#include <atomic>

#include "miniaudio.h"

// Estrutura escondida do header, para que miniaudio.h não vaze para o core.
struct AudioDevice::Impl
{
    ma_context context{};
    ma_device device{};

    bool contextReady = false;
    bool deviceReady = false;
    bool running = false;

    ProcessCallback callback;

    double sampleRate = 0.0;
    std::size_t channelCount = 0;
    std::string deviceName;
    std::string lastError;

    // Escrito pela thread de áudio, lido pela de controle. Atômico pelo mesmo
    // motivo do Parameter: sem isso seria corrida de dados.
    std::atomic<std::size_t> processedBlocks{0};
};

namespace
{
// Ponte entre o miniaudio e o nosso callback.
//
// Roda na thread de áudio. Tudo aqui precisa ser previsível: nenhuma
// alocação, nenhuma exceção, nenhuma espera.
void dataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount)
{
    auto* impl = static_cast<AudioDevice::Impl*>(device->pUserData);

    if (impl == nullptr || !impl->callback)
    {
        return;
    }

    impl->callback(static_cast<float*>(output),
                   static_cast<const float*>(input),
                   static_cast<std::size_t>(frameCount),
                   impl->channelCount);

    impl->processedBlocks.fetch_add(1, std::memory_order_relaxed);
}
}

AudioDevice::AudioDevice()
    : m_impl(std::make_unique<Impl>())
{
}

AudioDevice::~AudioDevice()
{
    stop();
}

bool AudioDevice::start(ProcessCallback callback, Mode mode, double sampleRate, int blockSize)
{
    if (m_impl->running)
    {
        m_impl->lastError = "o dispositivo ja esta rodando";
        return false;
    }

    if (!callback)
    {
        m_impl->lastError = "callback vazio";
        return false;
    }

    if (sampleRate <= 0.0 || blockSize <= 0)
    {
        m_impl->lastError = "sample rate ou tamanho de bloco invalido";
        return false;
    }

    m_impl->callback = std::move(callback);
    m_impl->lastError.clear();
    m_impl->processedBlocks.store(0, std::memory_order_relaxed);

    // O backend nulo é escolhido explicitamente; nos outros modos deixamos o
    // miniaudio decidir, para que ele use CoreAudio, WASAPI ou ALSA conforme
    // o sistema.
    ma_context_config contextConfig = ma_context_config_init();

    ma_result result = MA_SUCCESS;

    if (mode == Mode::Null)
    {
        ma_backend backends[] = {ma_backend_null};
        result = ma_context_init(backends, 1, &contextConfig, &m_impl->context);
    }
    else
    {
        result = ma_context_init(nullptr, 0, &contextConfig, &m_impl->context);
    }

    if (result != MA_SUCCESS)
    {
        m_impl->lastError = std::string("falha ao iniciar o contexto de audio: ")
                          + ma_result_description(result);
        return false;
    }

    m_impl->contextReady = true;

    const ma_device_type deviceType = (mode == Mode::Duplex)
                                    ? ma_device_type_duplex
                                    : ma_device_type_playback;

    ma_device_config config = ma_device_config_init(deviceType);

    // f32 é o formato em que o DSP já trabalha. Pedir qualquer outro faria o
    // miniaudio converter a cada bloco, sem ganho nenhum.
    config.playback.format = ma_format_f32;
    config.playback.channels = 0;   // 0 = aceitar o padrão do dispositivo

    if (deviceType == ma_device_type_duplex)
    {
        config.capture.format = ma_format_f32;
        config.capture.channels = 0;
    }

    config.sampleRate = static_cast<ma_uint32>(sampleRate);
    config.periodSizeInFrames = static_cast<ma_uint32>(blockSize);
    config.dataCallback = dataCallback;
    config.pUserData = m_impl.get();

    result = ma_device_init(&m_impl->context, &config, &m_impl->device);

    if (result != MA_SUCCESS)
    {
        m_impl->lastError = std::string("falha ao abrir o dispositivo: ")
                          + ma_result_description(result);
        ma_context_uninit(&m_impl->context);
        m_impl->contextReady = false;
        return false;
    }

    m_impl->deviceReady = true;

    // O driver pode ter negociado valores diferentes dos pedidos. É o que
    // vale de verdade, e é o que precisa chegar ao prepare() dos módulos.
    m_impl->sampleRate = static_cast<double>(m_impl->device.sampleRate);
    m_impl->channelCount = static_cast<std::size_t>(m_impl->device.playback.channels);
    m_impl->deviceName = m_impl->device.playback.name;

    result = ma_device_start(&m_impl->device);

    if (result != MA_SUCCESS)
    {
        m_impl->lastError = std::string("falha ao iniciar o dispositivo: ")
                          + ma_result_description(result);
        stop();
        return false;
    }

    m_impl->running = true;
    return true;
}

void AudioDevice::stop()
{
    if (m_impl->deviceReady)
    {
        // ma_device_uninit espera o callback em andamento terminar antes de
        // liberar, então não há risco de a thread de áudio usar memória já
        // destruída.
        ma_device_uninit(&m_impl->device);
        m_impl->deviceReady = false;
    }

    if (m_impl->contextReady)
    {
        ma_context_uninit(&m_impl->context);
        m_impl->contextReady = false;
    }

    m_impl->running = false;
}

bool AudioDevice::isRunning() const
{
    return m_impl->running;
}

double AudioDevice::sampleRate() const
{
    return m_impl->sampleRate;
}

std::size_t AudioDevice::channelCount() const
{
    return m_impl->channelCount;
}

std::string AudioDevice::deviceName() const
{
    return m_impl->deviceName;
}

std::string AudioDevice::lastError() const
{
    return m_impl->lastError;
}

std::size_t AudioDevice::processedBlocks() const
{
    return m_impl->processedBlocks.load(std::memory_order_relaxed);
}
