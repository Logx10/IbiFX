#include "NoiseGate.h"

#include <cmath>

#include "OnePole.h"

namespace
{
// 2 ms: rápido o bastante para o ataque de uma nota tocada normalmente não
// ser percebido como atrasado, devagar o bastante para não soar como um
// clique de liga/desliga instantâneo. Não é ajustável ainda porque nenhum
// caso de uso real pediu isso — abre-se um parâmetro quando houver um
// problema concreto, não por antecipação.
constexpr float kAttackSeconds = 0.002f;

// 5 ms: curto o bastante para não atrasar perceptivelmente o
// reconhecimento de uma nota nova, longo o bastante para filtrar o ripple
// de retificação (|amostra|) até da corda mais grave da guitarra (Mi
// grave, ~82 Hz — um ciclo inteiro dura 12 ms, e o sinal retificado tem
// dois picos por ciclo, a cada 6 ms; 5 ms já atenua isso de forma
// audível). Ver o comentário "A decisão é tomada em cima de um envelope"
// em NoiseGate.h para o porquê deste estágio existir.
constexpr float kEnvelopeSeconds = 0.005f;

// O limiar de FECHAR é esta fração do limiar de abrir — metade da amplitude
// é ~6 dB abaixo, a largura de banda de histerese típica de um gate de
// hardware. Ver "HISTERESE" em NoiseGate.h.
constexpr float kHysteresisRatio = 0.5f;

// 50 ms totalmente aberto antes de o release começar — o bastante para o
// espaço entre notas de uma passagem rápida não fechar o gate, curto o
// bastante para o ruído não vazar perceptivelmente depois da última nota.
// Ver "HOLD" em NoiseGate.h. Fixo pelo mesmo motivo de kAttackSeconds.
constexpr float kHoldSeconds = 0.05f;
}

NoiseGate::NoiseGate()
{
    // -46 dBFS de amplitude é um ponto de partida mais permissivo para
    // captadores passivos e palhetadas leves. O ruído da entrada varia:
    // ajuste o threshold acima do piso real do seu instrumento, se preciso.
    m_parameters.emplace_back("threshold", "Threshold", 0.0f, 0.3f, 0.005f);

    // 150 ms é rápido o bastante para não deixar o ruído ecoar por muito
    // tempo depois que a nota para, devagar o bastante para a cauda de uma
    // nota decaindo não ser cortada no meio.
    m_parameters.emplace_back("release", "Release", 0.01f, 1.0f, 0.15f);
}

void NoiseGate::setThreshold(float newThreshold)
{
    m_parameters[0].setValue(newThreshold);
}

float NoiseGate::threshold() const
{
    return m_parameters[0].value();
}

void NoiseGate::setRelease(float seconds)
{
    m_parameters[1].setValue(seconds);
}

float NoiseGate::release() const
{
    return m_parameters[1].value();
}

const char* NoiseGate::name() const
{
    return "NoiseGate";
}

void NoiseGate::prepare(double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    m_attackCoeff = onePoleCoefficient(kAttackSeconds, m_sampleRate);
    m_envelopeCoeff = onePoleCoefficient(kEnvelopeSeconds, m_sampleRate);
    m_holdSamplesTotal = static_cast<int>(kHoldSeconds * m_sampleRate + 0.5);
}

void NoiseGate::reset()
{
    m_gain = 0.0f;
    m_envelope = 0.0f;
    m_open = false;
    m_holdSamplesLeft = 0;
}

void NoiseGate::process(std::vector<float>& buffer)
{
    const float thresholdValue = m_parameters[0].value();
    const float closeThreshold = thresholdValue * kHysteresisRatio;

    // Recalculado uma vez por bloco, não por amostra: o parâmetro só muda
    // quando alguém mexe no controle, o que é raro comparado ao número de
    // amostras num bloco (recalcular por amostra seria trabalho jogado
    // fora, embora não seja errado).
    const float releaseCoeff = onePoleCoefficient(m_parameters[1].value(), m_sampleRate);

    for (float& sample : buffer)
    {
        // Segue o NÍVEL do sinal, não a amostra instantânea — é isto que
        // evita o gate abrir/fechar a cada cruzamento de zero da onda. Ver
        // o comentário em NoiseGate.h.
        m_envelope += m_envelopeCoeff * (std::fabs(sample) - m_envelope);

        // O termostato da histerese: abrir e fechar usam limiares
        // diferentes, e entre eles o estado atual é mantido — ver
        // "HISTERESE" em NoiseGate.h. Fechar ainda exige o hold esgotado —
        // ver "HOLD" lá também.
        if (m_envelope > thresholdValue)
        {
            m_open = true;
            m_holdSamplesLeft = m_holdSamplesTotal;
        }
        else if (m_envelope < closeThreshold)
        {
            if (m_holdSamplesLeft > 0)
                --m_holdSamplesLeft;
            else
                m_open = false;
        }

        const float target = m_open ? 1.0f : 0.0f;
        const float coeff = target > m_gain ? m_attackCoeff : releaseCoeff;

        m_gain += coeff * (target - m_gain);

        sample *= m_gain;
    }
}
