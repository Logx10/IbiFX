#include "SmoothedValue.h"

#include <cmath>

void SmoothedValue::prepare(double sampleRate, float rampSeconds, float initialValue)
{
    if (sampleRate > 0.0 && rampSeconds > 0.0f)
    {
        m_rampLength = static_cast<int>(std::lround(static_cast<double>(rampSeconds) * sampleRate));
    }
    else
    {
        m_rampLength = 0;
    }

    // Rampa de comprimento zero e rampa de uma amostra dão no mesmo: salto.
    if (m_rampLength < 1)
    {
        m_rampLength = 0;
    }

    snapTo(initialValue);
}

void SmoothedValue::setTarget(float newTarget)
{
    // Mesmo alvo não reinicia a rampa. Sem esta guarda, chamar setTarget a
    // cada bloco com o valor inalterado congelaria o valor atual a um passo
    // do destino, para sempre.
    if (newTarget == m_target)
    {
        return;
    }

    m_target = newTarget;

    if (m_rampLength == 0)
    {
        snapTo(newTarget);
        return;
    }

    // A rampa parte de onde o valor está AGORA, e não do alvo anterior. É o
    // que permite mudar de destino no meio do caminho sem criar degrau.
    m_stepsRemaining = m_rampLength;
    m_step = (m_target - m_current) / static_cast<float>(m_rampLength);
}

void SmoothedValue::snapTo(float newValue)
{
    m_current = newValue;
    m_target = newValue;
    m_step = 0.0f;
    m_stepsRemaining = 0;
}

float SmoothedValue::current() const
{
    return m_current;
}

float SmoothedValue::target() const
{
    return m_target;
}

bool SmoothedValue::isSmoothing() const
{
    return m_stepsRemaining > 0;
}

float SmoothedValue::nextValue()
{
    if (m_stepsRemaining <= 0)
    {
        return m_current;
    }

    m_current += m_step;
    --m_stepsRemaining;

    // Somar m_step muitas vezes acumula erro de arredondamento, e o valor
    // final ficaria perto do alvo mas nunca exatamente nele. No último passo
    // atribuímos o alvo direto — barato, e evita um resíduo permanente.
    if (m_stepsRemaining == 0)
    {
        m_current = m_target;
    }

    return m_current;
}
