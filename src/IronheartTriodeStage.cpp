#include "IronheartTriodeStage.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
// Modelo genérico 12AX7 de Koren; não ajustado a uma válvula Laney medida.
constexpr double kMu = 100.0, kExponent = 1.4, kKg = 1060.0;
constexpr double kKp = 600.0, kKvb = 300.0;
}

IronheartTriodeStage::Current IronheartTriodeStage::evaluate(double plateCathode,
                                                           double gridCathode) noexcept
{
    if (plateCathode <= 0.0) return {};
    const double denominator = std::sqrt(kKvb + plateCathode * plateCathode);
    const double argument = kKp * (1.0 / kMu + gridCathode / denominator);
    // softplus estável: não calcula exp(argument) quando argument é grande.
    const double exponential = std::exp(-std::fabs(argument));
    const double logarithm = std::max(argument, 0.0) + std::log1p(exponential);
    const double sigmoid = argument >= 0.0 ? 1.0 / (1.0 + exponential)
                                          : exponential / (1.0 + exponential);
    const double effective = plateCathode / kKp * logarithm;
    if (effective <= 0.0) return {};
    const double amps = 2.0 / kKg * std::pow(effective, kExponent);
    const double slope = kExponent * amps / effective;
    const double gridSlope = plateCathode * sigmoid / denominator;
    const double plateSlope = logarithm / kKp
        - sigmoid * gridCathode * plateCathode * plateCathode
          / (denominator * denominator * denominator);
    return {amps, slope * plateSlope, slope * gridSlope};
}

IronheartTriodeStage::OperatingPoint IronheartTriodeStage::solve(
    double grid, double conductance, double history, double initialCurrent,
    double plateSource, double plateResistance) noexcept
{
    // KCL com equivalente de Thévenin da alimentação + carga de placa:
    // Va=plateSource-plateResistance*I; Vk=(I+history)/conductance.
    // F(I)=I-Ip(Va-Vk,Vg-Vk). A raiz está em [0,source/resistance]
    // para source positiva; cargas capacitivas podem devolver energia.
    double low = 0.0, high = std::max(0.0, plateSource / plateResistance);
    double current = std::clamp(initialCurrent, low, high);
    bool converged = false;
    // No máximo 8 passos de Newton protegidos e 32 de bisseção. Não há
    // loop ilimitado: a bisseção reduz o intervalo mesmo em sobrecarga.
    for (int iteration = 0; iteration < 40; ++iteration)
    {
        const double plate = plateSource - plateResistance * current;
        const double cathode = (current + history) / conductance;
        const auto tube = evaluate(plate - cathode, grid - cathode);
        const double residual = current - tube.amps;
        if (std::fabs(residual) < 1e-14 || high - low < 1e-13)
        {
            converged = true;
            break;
        }
        if (residual > 0.0) high = current;
        else low = current;
        const double derivative = 1.0 + tube.plateDerivative * plateResistance
            + (tube.plateDerivative + tube.gridDerivative) / conductance;
        const double candidate = current - residual / derivative;
        current = iteration < 8 && candidate > low && candidate < high
            ? candidate : 0.5 * (low + high);
    }
    // Mesmo no limite de iterações, a corrente permanece no intervalo.
    // converged permite verificar falhas numericamente sem log no callback.
    return {plateSource - plateResistance * current, (current + history) / conductance,
            current, converged};
}

void IronheartTriodeStage::prepare(double internalSampleRate)
{
    prepare(internalSampleRate, Circuit{});
}

void IronheartTriodeStage::prepare(double internalSampleRate, Circuit circuit)
{
    if (!std::isfinite(internalSampleRate) || internalSampleRate <= 0.0)
        throw std::invalid_argument("IronheartTriodeStage exige taxa interna positiva e finita");
    if (!std::isfinite(circuit.supplyVolts) || circuit.supplyVolts <= 0.0
        || !std::isfinite(circuit.plateResistance) || circuit.plateResistance <= 0.0
        || !std::isfinite(circuit.cathodeResistance) || circuit.cathodeResistance <= 0.0
        || !std::isfinite(circuit.cathodeCapFarads) || circuit.cathodeCapFarads < 0.0
        || !std::isfinite(circuit.plateDCConductance) || circuit.plateDCConductance < 0.0)
        throw std::invalid_argument("IronheartTriodeStage exige componentes passivos validos");
    m_circuit = circuit;
    m_capConductance = 2.0 * circuit.cathodeCapFarads * internalSampleRate;
    const double resistance = 1.0 / (1.0 / circuit.plateResistance + circuit.plateDCConductance);
    const double source = resistance * circuit.supplyVolts / circuit.plateResistance;
    m_bias = solve(0.0, 1.0 / circuit.cathodeResistance, 0.0,
                   circuit.supplyVolts / circuit.plateResistance / 2.0, source, resistance);
    if (!m_bias.converged)
        throw std::runtime_error("ECC83: ponto de repouso nao convergiu");
    const auto tube = evaluate(m_bias.plateVolts - m_bias.cathodeVolts, -m_bias.cathodeVolts);
    m_bypassedGain = resistance * tube.gridDerivative
                    / (1.0 + resistance * tube.plateDerivative);
    reset();
}

void IronheartTriodeStage::reset() noexcept
{
    m_point = m_bias;
    // O bypass de cátodo começa carregado no repouso, quando presente.
    m_history = m_capConductance * m_bias.cathodeVolts;
    m_atRest = true;
}

double IronheartTriodeStage::processSample(double gridVolts) noexcept
{
    return processSample(gridVolts, {});
}

double IronheartTriodeStage::processSample(double gridVolts, PlateLoad load) noexcept
{
    // Guarda numérica, não modelo de condução da grade. A região acima de
    // 100 V de entrada está fora da escala de instrumento adotada.
    gridVolts = std::isfinite(gridVolts) ? std::clamp(gridVolts, -100.0, 100.0) : 0.0;
    if (m_atRest && gridVolts == 0.0 && load.historyAmps == 0.0) return 0.0;
    m_atRest = false;
    const double totalLoad = m_circuit.plateDCConductance + load.conductance;
    const double resistance = 1.0 / (1.0 / m_circuit.plateResistance + totalLoad);
    const double source = resistance * (m_circuit.supplyVolts / m_circuit.plateResistance
        + load.conductance * m_bias.plateVolts + load.historyAmps);
    const auto next = solve(gridVolts, 1.0 / m_circuit.cathodeResistance + m_capConductance,
                            m_history, m_point.plateCurrentAmps, source, resistance);
    if (!next.converged)
    {
        // Contenção explícita: conserva a última solução e o capacitor.
        // O próximo sample tenta novamente, sem propagar solução inválida.
        m_point.converged = false;
        return m_point.plateVolts - m_bias.plateVolts;
    }
    m_point = next;
    m_history = 2.0 * m_capConductance * m_point.cathodeVolts - m_history;
    return m_point.plateVolts - m_bias.plateVolts;
}
