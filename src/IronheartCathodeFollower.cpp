#include "IronheartCathodeFollower.h"
#include "IronheartTriodeStage.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace { constexpr double kSupply = 410.0, kCathodeResistance = 100e3; }

IronheartCathodeFollower::OperatingPoint IronheartCathodeFollower::solve(
    double grid, Load load, double initialCathode) const noexcept
{
    const double g = 1.0 / kCathodeResistance + load.conductance;
    const double source = load.historyAmps + load.conductance * m_bias.cathodeVolts;
    // Uma carga capacitiva pode devolver energia. O intervalo inclui a
    // tensão com a válvula cortada, em vez de forçar clipping nos trilhos.
    double low = std::min(0.0, source / g), high = std::max(kSupply, source / g);
    double cathode = std::clamp(initialCathode, low, high);
    bool converged = false;
    // 8 passos de Newton e até 48 de bisseção. Com apenas 32, uma
    // aproximação de Newton próxima da borda podia consumir a margem
    // sem estreitar o intervalo o bastante e congelar uma amostra válida.
    for (int iteration = 0; iteration < 56; ++iteration)
    {
        const auto tube = IronheartTriodeStage::evaluate(kSupply - cathode, grid - cathode);
        const double residual = g * cathode - source - tube.amps;
        if (std::fabs(residual) < 1e-14 || high - low < 1e-9)
        { converged = true; break; }
        if (residual > 0.0) high = cathode;
        else low = cathode;
        const double derivative = g + tube.plateDerivative + tube.gridDerivative;
        const double candidate = cathode - residual / derivative;
        cathode = iteration < 8 && candidate > low && candidate < high
            ? candidate : 0.5 * (low + high);
    }
    const double current = IronheartTriodeStage::evaluate(kSupply - cathode, grid - cathode).amps;
    return {cathode, current, converged};
}

void IronheartCathodeFollower::prepare(double gridBiasVolts)
{
    if (!std::isfinite(gridBiasVolts) || gridBiasVolts < 0.0 || gridBiasVolts > kSupply)
        throw std::invalid_argument("V3 exige polarizacao de grade entre 0 e 410 V");
    m_gridBias = gridBiasVolts;
    m_bias = solve(m_gridBias, {}, m_gridBias);
    if (!m_bias.converged) throw std::runtime_error("V3: repouso nao convergiu");
    m_prepared = true;
    reset();
}

void IronheartCathodeFollower::reset() noexcept
{
    m_point = m_bias;
    m_atRest = true;
}

double IronheartCathodeFollower::processSample(double gridACVolts, Load load) noexcept
{
    if (!m_prepared) return 0.0;
    gridACVolts = std::isfinite(gridACVolts) ? std::clamp(gridACVolts, -500.0, 500.0) : 0.0;
    if (m_atRest && gridACVolts == 0.0 && load.historyAmps == 0.0) return 0.0;
    m_atRest = false;
    const auto point = solve(m_gridBias + gridACVolts, load, m_point.cathodeVolts);
    if (point.converged) m_point = point;
    else m_point.converged = false; // conserva a solução anterior, sem log no callback
    return m_point.cathodeVolts - m_bias.cathodeVolts;
}
