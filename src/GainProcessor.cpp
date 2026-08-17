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

void GainProcessor::process(std::vector<float>& buffer)
{
    // Lido uma vez, fora do laço: o valor não muda durante o bloco, e evita
    // uma indireção por amostra.
    const float gainValue = m_parameters[0].value();

    for (float& sample : buffer)
    {
        sample *= gainValue;
    }
}
