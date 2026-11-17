#pragma once

#include "SmoothedValue.h"

// Rede de entrada/Pre-Boost do esquema IRT120H 9505 rev.3 (2011).
// Modelo LINEAR: TL072 e chave JFET ideais, grade sem corrente/Miller.
// O pré é dono deste objeto, que roda dentro do oversampling existente.
// Controles não são atômicos: somente a thread de áudio toca neste objeto;
// a UI escreve os Parameters do IronheartPreamp pelo mecanismo do motor.
class IronheartInputStage
{
public:
    // Taxa INTERNA (4x o motor). Pode lançar por taxa inválida; fora do callback.
    void prepare(double internalSampleRate, float resistanceOhms, bool enabled);
    void setResistance(float ohms) noexcept;
    void setEnabled(bool enabled) noexcept;
    void reset() noexcept;
    double processSample(double input) noexcept;

private:
    // Integrador trapezoidal RC: guarda a história elétrica de cada ramo,
    // em vez de interpolar coeficientes de um biquad com controles móveis.
    struct LowPass
    {
        double state = 0.0;
        double process(double input, double coefficient) noexcept
        {
            const double delta = (input - state) * coefficient;
            const double output = state + delta;
            state = output + delta;
            return output;
        }
    };

    double m_rate = 176400.0;
    double m_b0 = 0.0, m_b2 = 0.0, m_a1 = 0.0, m_a2 = 0.0;
    double m_z1 = 0.0, m_z2 = 0.0;
    double m_groundCoefficient = 0.0;
    double m_gridCoefficient = 0.0;
    LowPass m_groundCap;
    LowPass m_feedbackCap;
    LowPass m_gridCap;
    SmoothedValue m_resistance;
    SmoothedValue m_enabled;
};
