#include "Chorus.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kTwoPi = 6.283185307179586;

// Atraso mínimo e quanto o LFO pode somar a ele, em segundos — ver o
// comentário no .h. Abaixo de ~5 ms a soma vira filtro pente (flanger);
// acima de ~15 ms começa a soar como eco curto (slapback).
constexpr double kBaseDelay = 0.005;
constexpr double kMaxSweep = 0.007;

// Vizinhas que a interpolação de Hermite lê além da amostra de base: uma
// antes e duas depois. É a folga que o buffer precisa ter.
constexpr std::size_t kInterpolationMargin = 4;

// Interpolação de Hermite de 4 pontos (forma de Catmull-Rom): o valor entre
// y0 e y1, a uma fração t de y0, usando ym1 e y2 para estimar a inclinação
// em cada ponta.
float hermite(float ym1, float y0, float y1, float y2, float t)
{
    const float c0 = y0;
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * t + c2) * t + c1) * t + c0;
}
}

Chorus::Chorus()
{
    m_parameters.emplace_back("rate", "Rate", 0.1f, 5.0f, 0.8f);
    m_parameters.emplace_back("depth", "Depth", 0.0f, 1.0f, 0.5f);
    m_parameters.emplace_back("mix", "Mix", 0.0f, 1.0f, 0.5f);
}

const char* Chorus::name() const
{
    return "Chorus";
}

void Chorus::prepare(double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    const std::size_t samples =
        static_cast<std::size_t>(std::ceil((kBaseDelay + kMaxSweep) * m_sampleRate)) + kInterpolationMargin;

    m_circular.assign(samples, 0.0f);
    m_writePosition = 0;
    m_phase = 0.0;

    m_smoothedDepth.prepare(m_sampleRate, kDefaultRampSeconds, m_parameters[1].value());
    m_smoothedMix.prepare(m_sampleRate, kDefaultRampSeconds, m_parameters[2].value());
}

void Chorus::reset()
{
    std::fill(m_circular.begin(), m_circular.end(), 0.0f);
    m_writePosition = 0;
    m_phase = 0.0;

    m_smoothedDepth.snapTo(m_parameters[1].value());
    m_smoothedMix.snapTo(m_parameters[2].value());
}

void Chorus::process(std::vector<float>& buffer)
{
    // Sem prepare(), não há buffer: passa o sinal intocado em vez de ler
    // fora da memória — mesma política do Delay.
    if (m_circular.empty())
        return;

    m_smoothedDepth.setTarget(m_parameters[1].value());
    m_smoothedMix.setTarget(m_parameters[2].value());

    const double phaseIncrement = static_cast<double>(m_parameters[0].value()) / m_sampleRate;
    const std::size_t size = m_circular.size();

    for (float& sample : buffer)
    {
        const float depthAmount = m_smoothedDepth.nextValue();
        const float mixAmount = m_smoothedMix.nextValue();

        // Escreve ANTES de ler: o atraso mínimo (5 ms) é muito maior que as
        // duas amostras à frente que a interpolação lê, então a leitura
        // nunca alcança a posição recém-escrita.
        m_circular[m_writePosition] = sample;

        const double lfo = 0.5 + 0.5 * std::sin(kTwoPi * m_phase);
        const double delaySamples = (kBaseDelay + static_cast<double>(depthAmount) * kMaxSweep * lfo) * m_sampleRate;

        // Posição de leitura com fração, recuada a partir da escrita.
        double readPosition = static_cast<double>(m_writePosition) - delaySamples;
        if (readPosition < 0.0)
            readPosition += static_cast<double>(size);

        const auto base = static_cast<std::size_t>(readPosition);
        const float fraction = static_cast<float>(readPosition - static_cast<double>(base));

        const float ym1 = m_circular[(base + size - 1) % size];
        const float y0 = m_circular[base % size];
        const float y1 = m_circular[(base + 1) % size];
        const float y2 = m_circular[(base + 2) % size];

        const float wet = hermite(ym1, y0, y1, y2, fraction);

        sample = sample * (1.0f - mixAmount) + wet * mixAmount;

        m_writePosition = (m_writePosition + 1) % size;

        m_phase += phaseIncrement;
        if (m_phase >= 1.0)
            m_phase -= 1.0;
    }
}

void Chorus::setRate(float hertz)
{
    m_parameters[0].setValue(hertz);
}

float Chorus::rate() const
{
    return m_parameters[0].value();
}

void Chorus::setDepth(float amount)
{
    m_parameters[1].setValue(amount);
}

float Chorus::depth() const
{
    return m_parameters[1].value();
}

void Chorus::setMix(float amount)
{
    m_parameters[2].setValue(amount);
}

float Chorus::mix() const
{
    return m_parameters[2].value();
}

std::size_t Chorus::bufferSize() const
{
    return m_circular.size();
}
