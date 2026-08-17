#include "GainProcessor.h"

GainProcessor::GainProcessor()
{
    m_parameters.emplace_back("gain", "Gain", -8.0f, 8.0f, 1.0f);
}

void GainProcessor::setGain(float newGain)
{
    m_parameters[0].setValue(newGain);
}

float GainProcessor::gain() const
{
    return m_parameters[0].value();
}

const char* GainProcessor::name() const
{
    return "Gain";
}

void GainProcessor::prepare(double sampleRate, int /*blockSize*/)
{
    m_smoothedGain.prepare(sampleRate, kDefaultRampSeconds, m_parameters[0].value());
}

void GainProcessor::reset()
{
    m_smoothedGain.snapTo(m_parameters[0].value());
}

void GainProcessor::process(std::vector<float>& buffer)
{
    // O alvo é lido uma vez por bloco, não por amostra: o parâmetro só muda
    // entre blocos. A suavização acontece dentro do laço.
    m_smoothedGain.setTarget(m_parameters[0].value());

    for (float& sample : buffer)
    {
        sample *= m_smoothedGain.nextValue();
    }
}
