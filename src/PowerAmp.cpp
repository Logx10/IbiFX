#include "PowerAmp.h"

#include <cmath>

#include "OnePole.h"

namespace
{
// Bem mais lentos que os tempos do NoiseGate ou do Compressor — ver o
// comentário no .h sobre por que o sag reage à música, não à nota.
constexpr float kSagAttackSeconds = 0.05f;
constexpr float kSagReleaseSeconds = 0.3f;
}

PowerAmp::PowerAmp()
{
    m_parameters.emplace_back("drive", "Drive", 0.1f, 5.0f, 1.0f);
    m_parameters.emplace_back("sag", "Sag", 0.0f, 1.0f, 0.3f);
}

void PowerAmp::setDrive(float newDrive)
{
    m_parameters[0].setValue(newDrive);
}

float PowerAmp::drive() const
{
    return m_parameters[0].value();
}

void PowerAmp::setSag(float amount)
{
    m_parameters[1].setValue(amount);
}

float PowerAmp::sag() const
{
    return m_parameters[1].value();
}

const char* PowerAmp::name() const
{
    return "PowerAmp";
}

void PowerAmp::prepare(double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

void PowerAmp::reset()
{
    m_envelope = 0.0f;
}

void PowerAmp::process(std::vector<float>& buffer)
{
    const float driveValue = m_parameters[0].value();
    const float sagValue = m_parameters[1].value();

    const float attackCoeff = onePoleCoefficient(kSagAttackSeconds, m_sampleRate);
    const float releaseCoeff = onePoleCoefficient(kSagReleaseSeconds, m_sampleRate);

    for (float& sample : buffer)
    {
        const float inputMagnitude = std::fabs(sample);
        const float coeff = inputMagnitude > m_envelope ? attackCoeff : releaseCoeff;

        m_envelope += coeff * (inputMagnitude - m_envelope);

        const float effectiveDrive = driveValue * (1.0f + sagValue * m_envelope);

        sample = std::tanh(effectiveDrive * sample);
    }
}
