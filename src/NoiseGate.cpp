#include "NoiseGate.h"

#include <cmath>

namespace
{
// 2 ms: rápido o bastante para o ataque de uma nota tocada normalmente não
// ser percebido como atrasado, devagar o bastante para não soar como um
// clique de liga/desliga instantâneo. Não é ajustável ainda porque nenhum
// caso de uso real pediu isso — abre-se um parâmetro quando houver um
// problema concreto, não por antecipação.
constexpr float kAttackSeconds = 0.002f;

// Converte um tempo em segundos no coeficiente de um filtro de um polo:
//
//     atual += coeficiente * (alvo - atual)
//
// Quanto maior o coeficiente, mais rápido "atual" se aproxima de "alvo" a
// cada amostra. A fórmula vem de pedir que, depois de `seconds` segundos, a
// distância até o alvo tenha caído para 1/e (~37%) do valor inicial — a
// definição usual de "tempo de resposta" de um filtro exponencial.
float onePoleCoefficient(float seconds, double sampleRate)
{
    if (seconds <= 0.0f)
    {
        return 1.0f;
    }

    return 1.0f - std::exp(-1.0f / (seconds * static_cast<float>(sampleRate)));
}
}

NoiseGate::NoiseGate()
{
    // 0.02 de amplitude (~-34 dB) é um piso conservador: alto o bastante
    // para cortar chiado de captador e hum de rede, baixo o bastante para
    // não engolir o final de uma nota dedilhada suavemente.
    m_parameters.emplace_back("threshold", "Threshold", 0.0f, 0.3f, 0.02f);

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
}

void NoiseGate::reset()
{
    m_gain = 0.0f;
}

void NoiseGate::process(std::vector<float>& buffer)
{
    const float thresholdValue = m_parameters[0].value();

    // Recalculado uma vez por bloco, não por amostra: o parâmetro só muda
    // quando alguém mexe no controle, o que é raro comparado ao número de
    // amostras num bloco (recalcular por amostra seria trabalho jogado
    // fora, embora não seja errado).
    const float releaseCoeff = onePoleCoefficient(m_parameters[1].value(), m_sampleRate);

    for (float& sample : buffer)
    {
        const float target = std::fabs(sample) > thresholdValue ? 1.0f : 0.0f;
        const float coeff = target > m_gain ? m_attackCoeff : releaseCoeff;

        m_gain += coeff * (target - m_gain);

        sample *= m_gain;
    }
}
