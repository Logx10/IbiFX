#include "Flanger.h"

#include <algorithm>
#include <cmath>

#include "Interpolation.h"

namespace
{
// Atraso mínimo e máximo, em segundos — os medidos no Electric Mistress
// (ver o header).
constexpr double kMinDelay = 0.0008;
constexpr double kMaxDelay = 0.006;

// Vizinhas que a interpolação de Hermite lê além da amostra de base.
constexpr std::size_t kInterpolationMargin = 4;

// LFO triangular de 0 a 1: sobe em linha reta na primeira metade do ciclo,
// desce na segunda. O Electric Mistress usa triângulo, não seno — o pente
// passa o mesmo tempo em cada atraso, sem "parar" nas pontas.
double triangle(double phase)
{
    return phase < 0.5 ? 2.0 * phase : 2.0 - 2.0 * phase;
}
}

Flanger::Flanger()
{
    m_parameters.emplace_back("rate", "Rate", 0.07f, 10.0f, 0.3f);
    m_parameters.emplace_back("range", "Range", 0.0f, 1.0f, 0.7f);
    m_parameters.emplace_back("color", "Color", 0.0f, 0.9f, 0.5f);
}

void Flanger::setRate(float hertz)
{
    m_parameters[0].setValue(hertz);
}

float Flanger::rate() const
{
    return m_parameters[0].value();
}

void Flanger::setRange(float amount)
{
    m_parameters[1].setValue(amount);
}

float Flanger::range() const
{
    return m_parameters[1].value();
}

void Flanger::setColor(float amount)
{
    m_parameters[2].setValue(amount);
}

float Flanger::color() const
{
    return m_parameters[2].value();
}

const char* Flanger::name() const
{
    return "Flanger";
}

void Flanger::prepare(double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    const std::size_t samples = static_cast<std::size_t>(std::ceil(kMaxDelay * m_sampleRate)) + kInterpolationMargin;
    m_circular.assign(samples, 0.0f);

    m_smoothedRange.prepare(m_sampleRate, kDefaultRampSeconds, m_parameters[1].value());
    m_smoothedColor.prepare(m_sampleRate, kDefaultRampSeconds, m_parameters[2].value());
    reset();
}

void Flanger::reset()
{
    std::fill(m_circular.begin(), m_circular.end(), 0.0f);
    m_writePosition = 0;
    m_phase = 0.0;

    m_smoothedRange.snapTo(m_parameters[1].value());
    m_smoothedColor.snapTo(m_parameters[2].value());
}

void Flanger::process(std::vector<float>& buffer)
{
    // Sem prepare(), não há buffer: passa o sinal intocado — mesma política
    // do Chorus e do Delay.
    if (m_circular.empty())
        return;

    m_smoothedRange.setTarget(m_parameters[1].value());
    m_smoothedColor.setTarget(m_parameters[2].value());

    const double phaseIncrement = static_cast<double>(m_parameters[0].value()) / m_sampleRate;
    const std::size_t size = m_circular.size();

    for (float& sample : buffer)
    {
        const double rangeAmount = m_smoothedRange.nextValue();
        const float colorAmount = m_smoothedColor.nextValue();

        // O atraso vai do mínimo até onde o Range deixa.
        const double delay = kMinDelay + rangeAmount * (kMaxDelay - kMinDelay) * triangle(m_phase);
        double readPosition = static_cast<double>(m_writePosition) - delay * m_sampleRate;
        if (readPosition < 0.0)
            readPosition += static_cast<double>(size);

        const auto base = static_cast<std::size_t>(readPosition);
        const float fraction = static_cast<float>(readPosition - static_cast<double>(base));

        // Lê ANTES de escrever: a amostra atual ainda não está na linha, e
        // o atraso mínimo (~38 amostras a 48 kHz) é muito maior que as duas
        // vizinhas à frente que a interpolação usa.
        const float wet = hermite(m_circular[(base + size - 1) % size], m_circular[base % size],
                                  m_circular[(base + 1) % size], m_circular[(base + 2) % size], fraction);

        m_circular[m_writePosition] = sample + colorAmount * wet;

        sample = 0.5f * (sample + wet);

        m_writePosition = (m_writePosition + 1) % size;
        m_phase += phaseIncrement;
        if (m_phase >= 1.0)
            m_phase -= 1.0;
    }
}
