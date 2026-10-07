#include "Preamp.h"

#include <cmath>
#include <cstddef>

#include "OnePole.h"

namespace
{
constexpr int kStageCount = 3;

constexpr float kPi = 3.14159265f;

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

    // 8 kHz é o meio da faixa típica do efeito Miller num pré valvulado —
    // ver o comentário no .h. Cada ampli da GearLibrary escolhe o seu.
    m_parameters.emplace_back("interstage", "Interstage", 2000.0f, 16000.0f, 8000.0f);
}

void Preamp::setInterstageCutoff(float hertz)
{
    m_parameters[1].setValue(hertz);
}

float Preamp::interstageCutoff() const
{
    return m_parameters[1].value();
}

void Preamp::setInterstageFilter(bool enabled)
{
    m_interstageFilter = enabled;
    m_interstageState.fill(0.0f);
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
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

void Preamp::reset()
{
    m_smoothedDrive.snapTo(m_parameters[0].value());
    m_oversampler.reset();
    m_interstageState.fill(0.0f);
}

void Preamp::process(std::vector<float>& buffer)
{
    m_smoothedDrive.setTarget(m_parameters[0].value());

    // O filtro roda na taxa em que a cascata roda — 4× a do motor, com
    // oversampling. Uma conta por bloco, não por amostra: o corte só muda
    // quando alguém gira o knob.
    const double cascadeRate = m_sampleRate * (m_oversampling ? Oversampler::kFactor : 1);
    const float interstageCoefficient = m_interstageFilter
        ? onePoleCoefficient(1.0f / (2.0f * kPi * m_parameters[1].value()), cascadeRate)
        : 1.0f;

    for (float& sample : buffer)
    {
        const float driveValue = m_smoothedDrive.nextValue();

        const auto cascade = [this, driveValue, interstageCoefficient](float stageSignal)
        {
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

                // Arredonda os agudos na saída de CADA estágio, o último
                // incluído (ver "O FILTRO ENTRE ESTÁGIOS" no .h). Com o
                // coeficiente 1, o estado só copia a entrada — filtro
                // desligado. Um passa-baixa de um polo não passa do valor
                // que recebe, então a saída continua em [-1, +1].
                float& state = m_interstageState[static_cast<std::size_t>(stage)];
                state += interstageCoefficient * (stageSignal - state);
                stageSignal = state;
            }

            return stageSignal;
        };

        // A cascata inteira roda sobreamostrada: os três estágios juntos são
        // a maior fonte de harmônicos altos da cadeia (ver Oversampler.h).
        sample = m_oversampling ? m_oversampler.process(sample, cascade) : cascade(sample);
    }
}

void Preamp::setOversampling(bool enabled)
{
    m_oversampling = enabled;
    m_oversampler.reset();
}

bool Preamp::oversampling() const
{
    return m_oversampling;
}
