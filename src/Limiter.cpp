#include "Limiter.h"

#include <cmath>

Limiter::Limiter()
{
    // Teto de 0.99, não 1.0: com threshold em 1.0 não sobraria headroom
    // nenhum para a curva suavizar o excesso, e a divisão abaixo (excesso /
    // headroom) explodiria. 0.99 garante pelo menos 1% de faixa para a tanh
    // trabalhar, mesmo no ajuste mais agressivo permitido.
    m_parameters.emplace_back("threshold", "Threshold", 0.1f, 0.99f, 0.9f);
}

void Limiter::setThreshold(float newThreshold)
{
    m_parameters[0].setValue(newThreshold);
}

float Limiter::threshold() const
{
    return m_parameters[0].value();
}

const char* Limiter::name() const
{
    return "Limiter";
}

void Limiter::prepare(double sampleRate, int /*blockSize*/)
{
    m_smoothedThreshold.prepare(sampleRate, kDefaultRampSeconds, m_parameters[0].value());
}

void Limiter::reset()
{
    m_smoothedThreshold.snapTo(m_parameters[0].value());
}

void Limiter::process(std::vector<float>& buffer)
{
    m_smoothedThreshold.setTarget(m_parameters[0].value());

    for (float& sample : buffer)
    {
        const float thresholdValue = m_smoothedThreshold.nextValue();
        const float magnitude = std::fabs(sample);

        // Dentro da faixa segura, sem nenhuma alteração — um limitador não é
        // um efeito, e não deve colorir o que já está bem.
        if (magnitude <= thresholdValue)
        {
            continue;
        }

        const float headroom = 1.0f - thresholdValue;
        const float excess = magnitude - thresholdValue;
        const float sign = sample < 0.0f ? -1.0f : 1.0f;

        sample = sign * (thresholdValue + headroom * std::tanh(excess / headroom));
    }
}
