#include "SoftClipper.h"

#include <cmath>

SoftClipper::SoftClipper()
{
    m_parameters.emplace_back("drive", "Drive", 0.0f, 100.0f, 1.0f);
}

void SoftClipper::setDrive(float newDrive)
{
    m_parameters[0].setValue(newDrive);
}

float SoftClipper::drive() const
{
    return m_parameters[0].value();
}

const char* SoftClipper::name() const
{
    return "SoftClipper";
}

void SoftClipper::prepare(double sampleRate, int /*blockSize*/)
{
    m_smoothedDrive.prepare(sampleRate, kDefaultRampSeconds, m_parameters[0].value());
}

void SoftClipper::reset()
{
    m_smoothedDrive.snapTo(m_parameters[0].value());
    m_oversampler.reset();
}

void SoftClipper::process(std::vector<float>& buffer)
{
    m_smoothedDrive.setTarget(m_parameters[0].value());

    for (float& sample : buffer)
    {
        // O teto em ±1 não é imposto por código: é propriedade da tanh.
        const float driveValue = m_smoothedDrive.nextValue();
        const auto curve = [driveValue](float x) { return std::tanh(driveValue * x); };

        sample = m_oversampling ? m_oversampler.process(sample, curve) : curve(sample);
    }
}

void SoftClipper::setOversampling(bool enabled)
{
    m_oversampling = enabled;
    m_oversampler.reset();
}

bool SoftClipper::oversampling() const
{
    return m_oversampling;
}
