#include "Wah.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;

// Faixa do Ibanez WH-10 no modo guitarra — ver o header.
constexpr float kLowestHz = 350.0f;
constexpr float kHighestHz = 2200.0f;

// Quanto o pico fica acima do resto, e quanto do grave vaza: 2x (+6 dB) no
// centro e um quarto do passa-baixa. Ajustados de ouvido contra o volume
// do pedal desligado — ver o teste de nível em test_wah.cpp.
constexpr float kPeakGain = 2.0f;
constexpr float kLowpassLeak = 0.25f;
}

Wah::Wah()
{
    m_parameters.emplace_back("position", "Pedal", 0.0f, 1.0f, 0.5f);
    m_parameters.emplace_back("resonance", "Resonance", 1.5f, 10.0f, 3.0f);
}

void Wah::setPosition(float position)
{
    m_parameters[0].setValue(position);
}

float Wah::position() const
{
    return m_parameters[0].value();
}

void Wah::setResonance(float q)
{
    m_parameters[1].setValue(q);
}

float Wah::resonance() const
{
    return m_parameters[1].value();
}

float Wah::centerFrequency(float position)
{
    return kLowestHz * std::pow(kHighestHz / kLowestHz, std::clamp(position, 0.0f, 1.0f));
}

const char* Wah::name() const
{
    return "Wah";
}

void Wah::prepare(double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    m_smoothedPosition.prepare(m_sampleRate, kDefaultRampSeconds, m_parameters[0].value());
    reset();
}

void Wah::reset()
{
    m_ic1 = 0.0f;
    m_ic2 = 0.0f;
    m_smoothedPosition.snapTo(m_parameters[0].value());
}

void Wah::process(std::vector<float>& buffer)
{
    m_smoothedPosition.setTarget(m_parameters[0].value());

    const float k = 1.0f / m_parameters[1].value();

    for (float& sample : buffer)
    {
        // O centro é recalculado a cada amostra: a posição suavizada muda
        // um pouquinho em cada uma, e a forma TPT aguenta isso sem estalo.
        const double center = centerFrequency(m_smoothedPosition.nextValue());
        const float g = static_cast<float>(std::tan(kPi * center / m_sampleRate));

        const float v3 = sample - m_ic2;
        const float v1 = (m_ic1 + g * v3) / (1.0f + g * (g + k));
        const float v2 = m_ic2 + g * v1;

        m_ic1 = 2.0f * v1 - m_ic1;
        m_ic2 = 2.0f * v2 - m_ic2;

        sample = k * v1 * kPeakGain + v2 * kLowpassLeak;
    }
}
