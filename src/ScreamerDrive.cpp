#include "ScreamerDrive.h"

#include <cmath>

namespace
{
// Yeh, Abel e Smith, DAFx-07, seção 4.2. O capacitor de 47 nF dá
// um polo de ~720 Hz com R1. O potenciômetro altera R2, não esse polo.
constexpr double kR1 = 4700.0;
constexpr double kCapacitor = 47e-9;
constexpr float kFixedFeedback = 51000.0f;
constexpr float kDrivePot = 500000.0f;
// Aproximação suave dos diodos de silício, com 1 unidade digital por volt.
constexpr float kDiodeLimit = 0.55f;
}

ScreamerDrive::ScreamerDrive()
{
    m_parameters.emplace_back("drive", "Drive", 0.0f, 1.0f, 0.25f);
    prepare(48000.0, 0);
}

const char* ScreamerDrive::name() const
{
    return "ScreamerDrive";
}

void ScreamerDrive::prepare(double sampleRate, int /*blockSize*/)
{
    const double rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    const double oversampledRate = rate * Oversampler::kFactor;
    m_feedbackCoefficient = static_cast<float>(-std::expm1(-1.0 / (oversampledRate * kR1 * kCapacitor)));
    m_drive.prepare(rate, kDefaultRampSeconds, m_parameters[0].value());
    reset();
}

void ScreamerDrive::reset()
{
    m_drive.snapTo(m_parameters[0].value());
    m_feedbackLow = 0.0f;
    m_oversampler.reset();
}

void ScreamerDrive::process(std::vector<float>& buffer)
{
    m_drive.setTarget(m_parameters[0].value());
    for (float& sample : buffer)
    {
        const float gain = (kFixedFeedback + kDrivePot * m_drive.nextValue()) / static_cast<float>(kR1);
        sample = m_oversampler.process(sample, [this, gain](float input) {
            m_feedbackLow += m_feedbackCoefficient * (input - m_feedbackLow);
            const float high = input - m_feedbackLow;
            return input + kDiodeLimit * std::tanh(high * gain / kDiodeLimit);
        });
    }
}
