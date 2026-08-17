#include "Clipper.h"

Clipper::Clipper()
{
    m_parameters.emplace_back("threshold", "Threshold", 0.0f, 2.0f, 1.0f);
}

void Clipper::setThreshold(float newThreshold)
{
    m_parameters[0].setValue(newThreshold);
}

float Clipper::threshold() const
{
    return m_parameters[0].value();
}

const char* Clipper::name() const
{
    return "Clipper";
}

void Clipper::prepare(double sampleRate, int /*blockSize*/)
{
    m_smoothedThreshold.prepare(sampleRate, kDefaultRampSeconds, m_parameters[0].value());
}

void Clipper::reset()
{
    m_smoothedThreshold.snapTo(m_parameters[0].value());
}

void Clipper::process(std::vector<float>& buffer)
{
    m_smoothedThreshold.setTarget(m_parameters[0].value());

    for (float& sample : buffer)
    {
        const float thresholdValue = m_smoothedThreshold.nextValue();

        // > e não >=: uma amostra exatamente no teto já está dentro da faixa.
        if (sample > thresholdValue)
        {
            sample = thresholdValue;
        }
        else if (sample < -thresholdValue)
        {
            sample = -thresholdValue;
        }
    }
}
