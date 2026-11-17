#include "IronheartGainStages.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
// Duas redes iguais de grade em paralelo: R1/R10=470k, R36/R16=150k,
// R46/R19=470k e C21/C31=220n. Corrente de grade dos seguidores idealmente zero.
constexpr double kOutputSeriesG = 2.0 / (470e3 + 150e3);
constexpr double kOutputGroundG = 2.0 / 470e3;
constexpr double kOutputDCG = 2.0 / (470e3 + 150e3 + 470e3);
}

IronheartTriodeStage::PlateLoad IronheartGainStages::Coupling::load(
    double seriesResistance, double termination) const noexcept
{
    const double denominator = 1.0 + conductance * (seriesResistance + termination);
    return {conductance / denominator, history / denominator};
}

double IronheartGainStages::Coupling::finish(double plateAC, double seriesResistance,
                                            double termination) noexcept
{
    const auto equivalent = load(seriesResistance, termination);
    const double current = equivalent.conductance * plateAC - equivalent.historyAmps;
    const double capacitorVolts = plateAC - current * (seriesResistance + termination);
    history = 2.0 * conductance * capacitorVolts - history;
    return termination * current;
}

void IronheartGainStages::prepare(double rate, bool clean)
{
    if (!std::isfinite(rate) || rate <= 0.0)
        throw std::invalid_argument("IronheartGainStages exige taxa interna positiva e finita");
    m_v1b.prepare(rate, {383.0, 100e3, 1.5e3, 22e-6, 0.0});
    m_v2a.prepare(rate, {410.0, 100e3, 1.5e3, 0.0, 0.0});
    // A carga DC das duas redes de grade de V3 participa do repouso.
    m_v2b.prepare(rate, {410.0, 100e3, 1.5e3, 0.0, kOutputDCG});
    m_c6.conductance = 2.0 * rate * 10e-9;
    m_c7c3.conductance = 2.0 * rate * 3.4e-9; // dois 6n8 em série, sem derivação
    m_plateCapConductance = 2.0 * rate * 220e-12; // C2 em paralelo com R14
    m_outputCapConductance = 2.0 * 2.0 * rate * 220e-9; // C21/C31 em paralelo equivalente
    m_clean.prepare(rate, kDefaultRampSeconds, clean ? 1.0f : 0.0f);
    m_prepared = true;
    reset();
}

void IronheartGainStages::setClean(bool clean) noexcept
{
    m_clean.setTarget(clean ? 1.0f : 0.0f);
}

void IronheartGainStages::reset() noexcept
{
    m_v1b.reset(); m_v2a.reset(); m_v2b.reset();
    m_c6.history = m_c7c3.history = 0.0;
    m_plateCapHistory = m_outputCapHistory = 0.0;
    m_clean.snapTo(m_clean.target());
    m_signals = {};
}

double IronheartGainStages::processSample(double gridVolts) noexcept
{
    if (!m_prepared) return 0.0;
    const double shuntG = std::clamp(m_clean.nextValue(), 0.0f, 1.0f) / 22e3;
    const double termination = 1.0 / (1.0 / 150e3 + shuntG);
    // Não há reset de capacitores na troca Clean/Rhythm. A condutância
    // do shunt é suavizada; é uma transição numérica, não a bobina do relé.
    m_signals.cleanShuntConductance = shuntG;
    m_signals.v1bPlateAC = m_v1b.processSample(gridVolts, m_c6.load(220e3, 150e3));
    m_signals.v2aGrid = m_c6.finish(m_signals.v1bPlateAC, 220e3, 150e3);

    auto load = m_c7c3.load(150e3, termination);
    load.conductance += m_plateCapConductance;
    load.historyAmps += m_plateCapHistory;
    m_signals.v2aPlateAC = m_v2a.processSample(m_signals.v2aGrid, load);
    m_plateCapHistory = 2.0 * m_plateCapConductance * m_signals.v2aPlateAC - m_plateCapHistory;
    m_signals.v2bGrid = m_c7c3.finish(m_signals.v2aPlateAC, 150e3, termination);

    const double denominator = kOutputSeriesG + kOutputGroundG + m_outputCapConductance;
    const double outputG = kOutputSeriesG * (kOutputGroundG + m_outputCapConductance) / denominator;
    const double outputH = kOutputSeriesG * m_outputCapHistory / denominator;
    // A parcela DC já está no ponto de repouso: acrescentar somente o
    // desvio dinâmico evita contar a carga resistiva duas vezes.
    m_signals.v2bPlateAC = m_v2b.processSample(m_signals.v2bGrid, {outputG - kOutputDCG, outputH});
    const double bottom = (kOutputSeriesG * m_signals.v2bPlateAC + m_outputCapHistory) / denominator;
    const double current = kOutputSeriesG * (m_signals.v2bPlateAC - bottom);
    m_outputCapHistory = 2.0 * m_outputCapConductance * bottom - m_outputCapHistory;
    m_signals.followerGridAC = m_signals.v2bPlateAC - 470e3 * current / 2.0;
    return m_signals.followerGridAC;
}
