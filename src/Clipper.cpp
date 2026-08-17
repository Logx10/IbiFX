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

void Clipper::process(std::vector<float>& buffer)
{
    const float thresholdValue = m_parameters[0].value();

    for (float& sample : buffer)
    {
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
