#include "IronheartInputStage.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
constexpr double kSeriesResistance = 10e3;  // R26
constexpr double kInputResistance = 1e6;   // R30
constexpr double kInputCap = 220e-9;       // C13
constexpr double kRadioCap = 47e-12;       // C15
constexpr double kFeedbackResistance = 100e3; // R38, paralelo a VR2
constexpr double kFeedbackCap = 220e-12;  // C23
constexpr double kGroundResistance = 4.7e3; // R59
constexpr double kGroundCap = 10e-6;      // C27
constexpr double kGridResistance = 1e6;  // R37
constexpr double kGridCap = 10e-9;       // C14

float boundedResistance(float value)
{
    return std::isfinite(value) ? std::clamp(value, 0.0f, 100000.0f) : 0.0f;
}
}

void IronheartInputStage::prepare(double internalSampleRate, float resistanceOhms, bool enabled)
{
    if (!std::isfinite(internalSampleRate) || internalSampleRate <= 0.0)
        throw std::invalid_argument("IronheartInputStage exige taxa interna positiva e finita");
    m_rate = internalSampleRate;
    const double k = 2.0 * m_rate;

    // Hin(s) = R30*C13*s / (1 + a1*s + a2*s^2).
    // A entrada tem carga R30 || C15; não são dois filtros RC isolados.
    const double a1 = kInputCap * (kSeriesResistance + kInputResistance)
                    + kInputResistance * kRadioCap;
    const double a2 = kSeriesResistance * kInputResistance * kInputCap * kRadioCap;
    const double denominator = 1.0 + a1 * k + a2 * k * k;
    m_b0 = kInputResistance * kInputCap * k / denominator;
    m_b2 = -m_b0;
    m_a1 = 2.0 * (1.0 - a2 * k * k) / denominator;
    m_a2 = (1.0 - a1 * k + a2 * k * k) / denominator;
    m_groundCoefficient = 1.0 / (1.0 + k * kGroundResistance * kGroundCap);
    m_gridCoefficient = 1.0 / (1.0 + k * kGridResistance * kGridCap);
    m_resistance.prepare(m_rate, kDefaultRampSeconds, boundedResistance(resistanceOhms));
    m_enabled.prepare(m_rate, kDefaultRampSeconds, enabled ? 1.0f : 0.0f);
    reset();
}

void IronheartInputStage::setResistance(float ohms) noexcept
{
    m_resistance.setTarget(boundedResistance(ohms));
}

void IronheartInputStage::setEnabled(bool enabled) noexcept
{
    m_enabled.setTarget(enabled ? 1.0f : 0.0f);
}

void IronheartInputStage::reset() noexcept
{
    m_z1 = m_z2 = 0.0;
    m_groundCap.state = m_feedbackCap.state = m_gridCap.state = 0.0;
    m_resistance.snapTo(m_resistance.target());
    m_enabled.snapTo(m_enabled.target());
}

double IronheartInputStage::processSample(double input) noexcept
{
    const double filtered = m_b0 * input + m_z1;
    m_z1 = -m_a1 * filtered + m_z2; // B1 = 0
    m_z2 = m_b2 * input - m_a2 * filtered;

    // O ramo R59/C27 continua acompanhando o sinal mesmo com boost off.
    // Assim, religar não ressuscita uma tensão de capacitor congelada.
    const double high = filtered - m_groundCap.process(filtered, m_groundCoefficient);
    const double pot = m_resistance.nextValue();
    const double enabled = m_enabled.nextValue();
    const double feedbackR = enabled * kFeedbackResistance * pot / (kFeedbackResistance + pot);
    double added = 0.0;
    if (feedbackR > 0.0)
    {
        const double coefficient = 1.0 / (1.0 + 2.0 * m_rate * feedbackR * kFeedbackCap);
        added = m_feedbackCap.process(high * feedbackR / kGroundResistance, coefficient);
    }
    else
    {
        // Curto ideal do JFET (ou VR2=0) descarrega a capacitância de
        // feedback. Evita um estado oculto alternando no polo z=-1.
        m_feedbackCap.state = 0.0;
    }
    const double boosted = filtered + added;
    // Hgrid(s) = s*R37*C14 / (1 + s*R37*C14), sem carga não linear de V1A.
    return boosted - m_gridCap.process(boosted, m_gridCoefficient);
}
