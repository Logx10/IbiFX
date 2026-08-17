#include "Delay.h"

#include <algorithm>
#include <cmath>

namespace
{
// Índices do buffer circular são calculados com sinal e só depois convertidos.
// Com std::size_t, a subtração writePosition - delaySamples não fica negativa:
// ela dá a volta e vira um número gigantesco, o teste `< 0` nunca dispara e o
// acesso estoura o buffer. É a armadilha clássica do circular buffer.
using Index = std::ptrdiff_t;
}

Delay::Delay()
{
    m_parameters.emplace_back("time", "Time", 0.0f, 2.0f, 0.25f);
    m_parameters.emplace_back("feedback", "Feedback", 0.0f, 0.95f, 0.3f);
    m_parameters.emplace_back("mix", "Mix", 0.0f, 1.0f, 0.5f);
}

const char* Delay::name() const
{
    return "Delay";
}

void Delay::prepare(double sampleRate, int /*blockSize*/)
{
    // Sample rate inválido cairia num buffer de tamanho zero, deixando o
    // módulo mudo sem explicação. 44100 é o palpite menos surpreendente.
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    // O buffer precisa comportar o MAIOR atraso que o parâmetro permite —
    // não o atraso atual, que pode mudar a qualquer momento sem passar por
    // aqui. A folga de uma amostra evita que a leitura no atraso máximo caia
    // exatamente sobre a posição de escrita.
    const double maxSeconds = static_cast<double>(m_parameters[0].maxValue());
    const std::size_t samples = static_cast<std::size_t>(std::ceil(maxSeconds * m_sampleRate)) + 1;

    // assign redimensiona E zera. Alocar aqui, e nunca no process(), é a
    // regra crítica do domínio de tempo real.
    m_circular.assign(samples, 0.0f);
    m_writePosition = 0;

    m_smoothedFeedback.prepare(m_sampleRate, kDefaultRampSeconds, m_parameters[1].value());
    m_smoothedMix.prepare(m_sampleRate, kDefaultRampSeconds, m_parameters[2].value());
}

void Delay::reset()
{
    // Zerar só a posição não bastaria: o áudio antigo continuaria no buffer e
    // voltaria a tocar assim que a escrita desse a volta.
    std::fill(m_circular.begin(), m_circular.end(), 0.0f);
    m_writePosition = 0;

    m_smoothedFeedback.snapTo(m_parameters[1].value());
    m_smoothedMix.snapTo(m_parameters[2].value());
}

void Delay::process(std::vector<float>& buffer)
{
    if (m_circular.empty())
    {
        return;
    }

    const float timeSeconds = m_parameters[0].value();

    m_smoothedFeedback.setTarget(m_parameters[1].value());
    m_smoothedMix.setTarget(m_parameters[2].value());

    const Index size = static_cast<Index>(m_circular.size());

    // Segundos viram amostras aqui, e não no setter: o sample rate pode mudar
    // depois que o tempo foi definido.
    Index delaySamples = static_cast<Index>(std::lround(static_cast<double>(timeSeconds) * m_sampleRate));
    delaySamples = std::clamp<Index>(delaySamples, 0, size - 1);

    for (float& sample : buffer)
    {
        Index readPosition = static_cast<Index>(m_writePosition) - delaySamples;

        if (readPosition < 0)
        {
            readPosition += size;
        }

        const float delayed = m_circular[static_cast<std::size_t>(readPosition)];

        const float feedbackAmount = m_smoothedFeedback.nextValue();
        const float mixAmount = m_smoothedMix.nextValue();

        // A leitura vem ANTES da escrita. Invertida, um atraso curto leria o
        // valor recém-gravado e a realimentação viraria instantânea.
        m_circular[m_writePosition] = sample + delayed * feedbackAmount;

        // Interpolação linear entre seco e molhado.
        sample = sample * (1.0f - mixAmount) + delayed * mixAmount;

        ++m_writePosition;

        if (m_writePosition >= m_circular.size())
        {
            m_writePosition = 0;
        }
    }
}

void Delay::setTime(float seconds)
{
    m_parameters[0].setValue(seconds);
}

float Delay::time() const
{
    return m_parameters[0].value();
}

void Delay::setFeedback(float amount)
{
    m_parameters[1].setValue(amount);
}

float Delay::feedback() const
{
    return m_parameters[1].value();
}

void Delay::setMix(float amount)
{
    m_parameters[2].setValue(amount);
}

float Delay::mix() const
{
    return m_parameters[2].value();
}

std::size_t Delay::bufferSize() const
{
    return m_circular.size();
}
