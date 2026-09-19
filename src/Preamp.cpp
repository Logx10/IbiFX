#include "Preamp.h"

#include <cmath>

namespace
{
constexpr int kStageCount = 3;

// Ganho de reexpansão entre estágios — ver o comentário no .h sobre por que
// ele existe. 2.0 é moderado: o suficiente para o próximo estágio ter sinal
// de verdade pra trabalhar, sem já entrar saturado antes mesmo de chegar
// nele.
constexpr float kInterStageBoost = 2.0f;
}

Preamp::Preamp()
{
    // A faixa é bem menor que a do SoftClipper (0-100): aqui o drive é
    // aplicado 3 vezes em cascata, com reexpansão entre elas, então o mesmo
    // número satura muito mais rápido que num estágio só.
    m_parameters.emplace_back("drive", "Drive", 0.1f, 5.0f, 1.0f);
}

void Preamp::setDrive(float newDrive)
{
    m_parameters[0].setValue(newDrive);
}

float Preamp::drive() const
{
    return m_parameters[0].value();
}

const char* Preamp::name() const
{
    return "Preamp";
}

void Preamp::prepare(double sampleRate, int /*blockSize*/)
{
    m_smoothedDrive.prepare(sampleRate, kDefaultRampSeconds, m_parameters[0].value());
}

void Preamp::reset()
{
    m_smoothedDrive.snapTo(m_parameters[0].value());
}

void Preamp::process(std::vector<float>& buffer)
{
    m_smoothedDrive.setTarget(m_parameters[0].value());

    for (float& sample : buffer)
    {
        const float driveValue = m_smoothedDrive.nextValue();

        float stageSignal = sample;

        for (int stage = 0; stage < kStageCount; ++stage)
        {
            stageSignal = std::tanh(driveValue * stageSignal);

            // Reexpande antes do PRÓXIMO estágio, exceto depois do último —
            // é o que garante que a saída final seja sempre uma tanh pura,
            // sem nunca escapar de [-1, +1].
            if (stage < kStageCount - 1)
            {
                stageSignal *= kInterStageBoost;
            }
        }

        sample = stageSignal;
    }
}
