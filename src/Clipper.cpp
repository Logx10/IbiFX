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
    m_oversampler.reset();
}

void Clipper::process(std::vector<float>& buffer)
{
    m_smoothedThreshold.setTarget(m_parameters[0].value());

    for (float& sample : buffer)
    {
        const float thresholdValue = m_smoothedThreshold.nextValue();

        // > e não >=: uma amostra exatamente no teto já está dentro da faixa.
        const auto curve = [thresholdValue](float x)
        {
            if (x > thresholdValue)
                return thresholdValue;
            if (x < -thresholdValue)
                return -thresholdValue;
            return x;
        };

        // O corte seco é o pior caso de aliasing (a quina gera harmônicos
        // sem fim); sobreamostrar não o elimina, mas o reduz muito.
        sample = m_oversampling ? m_oversampler.process(sample, curve) : curve(sample);
    }
}

void Clipper::setOversampling(bool enabled)
{
    m_oversampling = enabled;
    m_oversampler.reset();
}

bool Clipper::oversampling() const
{
    return m_oversampling;
}
