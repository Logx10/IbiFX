#include "Cabinet.h"

#include "IRLoader.h"

Cabinet::Cabinet()
{
    m_parameters.emplace_back("mix", "Mix", 0.0f, 1.0f, 1.0f);
}

void Cabinet::loadImpulseResponseFile(const std::string& path)
{
    m_engine.setImpulseResponse(loadImpulseResponse(path));
    m_irPath = path;
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

void Cabinet::prepare(double /*sampleRate*/, int /*blockSize*/)
{
    // A convolução não depende de sample rate nem de block size — só do
    // conteúdo da IR já carregada. Nada a preparar aqui além do que
    // loadImpulseResponseFile() já fez.
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
