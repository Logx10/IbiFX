#include "Rotovibe.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.141592653589793;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kMinRate = 0.3;
constexpr double kMaxRate = 7.0;
constexpr double kCenterFrequency = 700.0;
constexpr std::array<double, 4> kStageRatios{0.55, 0.9, 1.45, 2.3};
constexpr int kControlInterval = 32;

double rateForPosition(float position)
{
    return kMinRate * std::pow(kMaxRate / kMinRate, static_cast<double>(position));
}

float coefficientFor(double center, double ratio, double sampleRate)
{
    const double frequency = std::min(center * ratio, sampleRate * 0.45);
    const double tangent = std::tan(kPi * frequency / sampleRate);
    return static_cast<float>((tangent - 1.0) / (tangent + 1.0));
}
}

Rotovibe::Rotovibe()
{
    m_parameters.emplace_back("position", "Treadle", 0.0f, 1.0f, 0.35f);
    m_parameters.emplace_back("intensity", "Intensity", 0.0f, 1.0f, 0.7f);
    m_parameters.emplace_back("mode", "Mode", 0.0f, 1.0f, 0.0f);
}

const char* Rotovibe::name() const
{
    return "Rotovibe";
}

void Rotovibe::prepare(double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    m_rate.prepare(m_sampleRate, kDefaultRampSeconds,
                   static_cast<float>(rateForPosition(m_parameters[0].value())));
    m_intensity.prepare(m_sampleRate, kDefaultRampSeconds, m_parameters[1].value());
    m_wetMix.prepare(m_sampleRate, kDefaultRampSeconds,
                     m_parameters[2].value() >= 0.5f ? 1.0f : 0.5f);
    reset();
}

void Rotovibe::reset()
{
    m_stages = {};
    m_phase = 0.0;
    m_samplesUntilUpdate = 0;
    m_coefficientSteps = {};
    for (std::size_t i = 0; i < m_coefficients.size(); ++i)
        m_coefficients[i] = coefficientFor(kCenterFrequency, kStageRatios[i], m_sampleRate);
    m_rate.snapTo(static_cast<float>(rateForPosition(m_parameters[0].value())));
    m_intensity.snapTo(m_parameters[1].value());
    m_wetMix.snapTo(m_parameters[2].value() >= 0.5f ? 1.0f : 0.5f);
}

void Rotovibe::process(std::vector<float>& buffer)
{
    m_rate.setTarget(static_cast<float>(rateForPosition(m_parameters[0].value())));
    m_intensity.setTarget(m_parameters[1].value());
    m_wetMix.setTarget(m_parameters[2].value() >= 0.5f ? 1.0f : 0.5f);

    for (float& sample : buffer)
    {
        const double rate = m_rate.nextValue();
        const double intensity = m_intensity.nextValue();

        if (m_samplesUntilUpdate == 0)
        {
            // Atualiza o alvo a cada 32 amostras, mas percorre todos os
            // coeficientes linearmente entre alvos. Evita tan() por amostra
            // e degraus audíveis na frequência dos filtros.
            const double futurePhase = m_phase + rate * kControlInterval / m_sampleRate;
            const double sweep = std::sin(kTwoPi * futurePhase);
            const double center = kCenterFrequency * std::pow(2.0, 1.8 * intensity * sweep);
            for (std::size_t i = 0; i < m_stages.size(); ++i)
            {
                const float target = coefficientFor(center, kStageRatios[i], m_sampleRate);
                m_coefficientSteps[i] = (target - m_coefficients[i]) / kControlInterval;
            }
            m_samplesUntilUpdate = kControlInterval;
        }

        float wet = sample;
        for (std::size_t i = 0; i < m_stages.size(); ++i)
        {
            // Bilinear transform de um all-pass de primeira ordem. Cada
            // estágio tem uma frequência diferente, como os filtros de um
            // circuito de vibe, produzindo vales móveis ao misturar com dry.
            m_coefficients[i] += m_coefficientSteps[i];
            const float coefficient = m_coefficients[i];

            Stage& stage = m_stages[i];
            const float output = coefficient * wet + stage.previousInput
                                 - coefficient * stage.previousOutput;
            stage.previousInput = wet;
            stage.previousOutput = output;
            wet = output;
        }

        const float mix = m_wetMix.nextValue();
        sample = sample * (1.0f - mix) + wet * mix;

        --m_samplesUntilUpdate;
        m_phase += rate / m_sampleRate;
        if (m_phase >= 1.0)
            m_phase -= 1.0;
    }
}
