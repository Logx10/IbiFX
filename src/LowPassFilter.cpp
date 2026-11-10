#include "LowPassFilter.h"

namespace
{
// 200 Hz é "quase só o corpo da nota" (um filtro fechado de RAT); 12 kHz já
// é praticamente aberto — acima disso um passa-baixa de um polo quase não
// faz nada audível numa guitarra.
constexpr float kMinFrequency = 200.0f;
constexpr float kMaxFrequency = 12000.0f;
constexpr float kDefaultFrequency = 5000.0f;
}

LowPassFilter::LowPassFilter()
{
    m_parameters.emplace_back("frequency", "Frequency",
                              kMinFrequency, kMaxFrequency, kDefaultFrequency);
}

void LowPassFilter::setFrequency(float hz)
{
    m_parameters[0].setValue(hz);
}

float LowPassFilter::frequency() const
{
    return m_parameters[0].value();
}

const char* LowPassFilter::name() const
{
    return "LowPass";
}

void LowPassFilter::prepare(double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
}

void LowPassFilter::reset()
{
    // Zerar a memória evita um degrau na primeira amostra depois de religar.
    m_state = 0.0f;
}

void LowPassFilter::process(std::vector<float>& buffer)
{
    const double cutoff = static_cast<double>(m_parameters[0].value());

    // Mesma conta RC do HighPassFilter — ver o comentário no .h. Uma vez
    // por bloco, porque envolve divisão e o corte não muda no meio.
    const double rc = 1.0 / (6.283185307179586 * cutoff);
    const double dt = 1.0 / m_sampleRate;
    const float a = static_cast<float>(dt / (rc + dt));

    for (float& sample : buffer)
    {
        m_state += a * (sample - m_state);
        sample = m_state;
    }
}
