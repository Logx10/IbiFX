#include "Cabinet.h"

#include <utility>

#include "IRLoader.h"
#include "Resampler.h"

Cabinet::Cabinet()
{
    m_parameters.emplace_back("mix", "Mix", 0.0f, 1.0f, 1.0f);
}

void Cabinet::loadImpulseResponseFile(const std::string& path)
{
    ImpulseResponse ir = readImpulseResponse(path);

    m_originalIr = std::move(ir.samples);
    m_irSampleRate = ir.sampleRate;
    m_irPath = path;

    applyImpulseResponse();
}

void Cabinet::applyImpulseResponse()
{
    if (m_originalIr.empty())
        return;

    const bool needsConversion = m_sampleRate > 0.0 && m_irSampleRate > 0.0 && m_sampleRate != m_irSampleRate;

    if (!needsConversion)
    {
        m_engine.setImpulseResponse(m_originalIr);
        return;
    }

    std::vector<float> converted = resample(m_originalIr, m_irSampleRate, m_sampleRate);

    // resample() preserva a AMPLITUDE de cada amostra, mas uma IR com mais
    // amostras (subindo a taxa) somaria mais contribuições por amostra de
    // saída na convolução — o cabinet ficaria toRate/fromRate mais alto
    // (+0,7 dB de 44,1 para 48 kHz). Escalar por fromRate/toRate mantém a
    // mesma resposta em frequência, volume incluído.
    const float gain = static_cast<float>(m_irSampleRate / m_sampleRate);
    for (float& tap : converted)
        tap *= gain;

    m_engine.setImpulseResponse(std::move(converted));
}

const std::string& Cabinet::irPath() const
{
    return m_irPath;
}

void Cabinet::setMix(float amount)
{
    m_parameters[0].setValue(amount);
}

float Cabinet::mix() const
{
    return m_parameters[0].value();
}

const char* Cabinet::name() const
{
    return "Cabinet";
}

void Cabinet::prepare(double sampleRate, int /*blockSize*/)
{
    // Só reconverte se a taxa mudou — prepare() é chamado a cada religada
    // do motor (troca de preset, de equipamento), e refazer a conversão e a
    // alocação toda vez à toa seria desperdício.
    if (sampleRate == m_sampleRate)
        return;

    m_sampleRate = sampleRate;
    applyImpulseResponse();
}

void Cabinet::reset()
{
    m_engine.reset();
}

void Cabinet::process(std::vector<float>& buffer)
{
    const float mixValue = m_parameters[0].value();

    for (float& sample : buffer)
    {
        const float dry = sample;
        const float wet = m_engine.processSample(dry);

        sample = dry * (1.0f - mixValue) + wet * mixValue;
    }
}
