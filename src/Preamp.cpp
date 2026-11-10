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

// Corte do capacitor de acoplamento entre estágios — ver "O CAPACITOR DE
// ACOPLAMENTO" no .h. ~10 Hz é o que um cap de 22 nF com o resistor de
// grade de 1 MΩ típico dá num ampli real: bem abaixo do Mi grave da
// guitarra (82 Hz), bloqueia só o DC.
constexpr float kCouplingHz = 10.0f;
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

    // 0 = simétrico (o comportamento antigo, intacto). 0.3 já é uma
    // assimetria forte — tanh(0.3) desloca ~29% do teto de um lado. Cada
    // ampli da GearLibrary escolhe o seu (ver addAmps).
    m_parameters.emplace_back("bias", "Bias", 0.0f, 0.3f, 0.0f);
}

void Preamp::setBias(float amount)
{
    m_parameters[2].setValue(amount);
}

float Preamp::bias() const
{
    return m_parameters[2].value();
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
    m_couplingX.fill(0.0f);
    m_couplingY.fill(0.0f);
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

    // As constantes do bias e do capacitor de acoplamento, uma vez por
    // bloco pelo mesmo motivo do interstage: só mudam com o knob.
    //
    // A renormalização é uma DIVISÃO por (1 + tanh(bias)), não uma
    // multiplicação pelo recíproco pré-calculado: no piso da curva,
    // (-1 - tanhBias) / (1 + tanhBias) dá exatamente -1.0 em float (IEEE
    // arredonda os dois operandos de forma simétrica), enquanto o recíproco
    // arredondado pode estourar -1 por 1 ulp — e a garantia de faixa do
    // módulo é exata, não "quase".
    const float biasValue = m_parameters[2].value();
    const float tanhBias = std::tanh(biasValue);
    const float onePlusTanhBias = 1.0f + tanhBias;

    // Passa-alta de um polo na forma y[n] = g*(x[n] - x[n-1]) + r*y[n-1],
    // com r = exp(-2*pi*fc/taxa) e g = (1+r)/2 para o ganho em banda
    // passante ficar em 1.
    const float couplingR = std::exp(-2.0f * kPi * kCouplingHz / static_cast<float>(cascadeRate));
    const float couplingG = 0.5f * (1.0f + couplingR);

    for (float& sample : buffer)
    {
        const float driveValue = m_smoothedDrive.nextValue();

        const auto cascade = [this, driveValue, interstageCoefficient,
                              biasValue, tanhBias, onePlusTanhBias, couplingR, couplingG](float stageSignal)
        {
            for (int stage = 0; stage < kStageCount; ++stage)
            {
                // A curva deslocada e renormalizada — ver "O BIAS" no .h.
                // Com bias 0 isto é exatamente tanh(drive*x), como sempre
                // foi. A renormalização garante a faixa [-1, +1] da saída
                // sem precisar de nada depois dela.
                stageSignal = (std::tanh(driveValue * stageSignal + biasValue) - tanhBias) / onePlusTanhBias;

                // Reexpande antes do PRÓXIMO estágio, exceto depois do último —
                // é o que garante que a saída final seja sempre a curva
                // renormalizada pura, sem nunca escapar de [-1, +1].
                if (stage < kStageCount - 1)
                {
                    // O capacitor de acoplamento bloqueia o DC que a
                    // assimetria do bias gera, ANTES de o boost dobrá-lo —
                    // ver "O CAPACITOR DE ACOPLAMENTO" no .h. Com bias 0
                    // não há DC e ele fica fora do caminho.
                    if (biasValue > 0.0f)
                    {
                        float& x1 = m_couplingX[static_cast<std::size_t>(stage)];
                        float& y1 = m_couplingY[static_cast<std::size_t>(stage)];
                        const float blocked = couplingG * (stageSignal - x1) + couplingR * y1;
                        x1 = stageSignal;
                        y1 = blocked;
                        stageSignal = blocked;
                    }

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
