#include "IronheartChannelNetwork.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
float bounded(float x) { return std::isfinite(x) ? std::clamp(x, 0.0f, 1.0f) : 0.0f; }
double logTaper(double position)
{
    // VRs 220k A6S: curva log estimada, 10% no meio do curso.
    return (std::pow(81.0, position) - 1.0) / 80.0;
}
}

double IronheartChannelNetwork::Capacitor::admittance() const noexcept
{
    return conductance / (1.0 + conductance * seriesResistance);
}
double IronheartChannelNetwork::Capacitor::source() const noexcept
{
    return history / (1.0 + conductance * seriesResistance);
}

void IronheartChannelNetwork::prepare(double rate, Channel channel,
                                     float leadGain, float rhythmGain, float cleanVolume)
{
    if (!std::isfinite(rate) || rate <= 0.0)
        throw std::invalid_argument("IronheartChannelNetwork exige taxa interna positiva e finita");
    m_channel = channel;
    m_lead.prepare(rate, kDefaultRampSeconds, bounded(leadGain));
    m_rhythm.prepare(rate, kDefaultRampSeconds, bounded(rhythmGain));
    m_clean.prepare(rate, kDefaultRampSeconds, bounded(cleanVolume));
    // Dois 2n2 em série equivalem a 1n1 SOMENTE sem ramo no nó central.
    // Rs=150k pode ser eliminado junto ao capacitor sem perder sua memória.
    m_caps = {{{Plate, LeadTop, 2.0 * rate * 1.1e-9, 150e3, 0.0},
               {Plate, CH1, 2.0 * rate * 22e-9, 0.0, 0.0},
               {CH1, RhythmTop, 2.0 * rate * 1.1e-9, 150e3, 0.0},
               {CleanTop, CleanWiper, 2.0 * rate * 470e-12, 0.0, 0.0},
               {LeadBus, LeadGrid, 2.0 * rate * 100e-9, 0.0, 0.0}}};
    m_prepared = true;
    reset();
}

void IronheartChannelNetwork::setControls(Channel channel, float leadGain,
                                         float rhythmGain, float cleanVolume) noexcept
{
    if (channel != m_channel) { m_channel = channel; m_matrixDirty = true; }
    m_lead.setTarget(bounded(leadGain));
    m_rhythm.setTarget(bounded(rhythmGain));
    m_clean.setTarget(bounded(cleanVolume));
}

void IronheartChannelNetwork::reset() noexcept
{
    m_lead.snapTo(m_lead.target());
    m_rhythm.snapTo(m_rhythm.target());
    m_clean.snapTo(m_clean.target());
    for (auto& cap : m_caps) cap.history = 0.0;
    m_matrixDirty = true;
    m_offset = {};
}

void IronheartChannelNetwork::stamp(int a, int b, double g) noexcept
{
    if (a != Ground) m_matrix[a][a] += g;
    if (b != Ground) m_matrix[b][b] += g;
    if (a != Ground && b != Ground) { m_matrix[a][b] -= g; m_matrix[b][a] -= g; }
}

IronheartChannelNetwork::Vector IronheartChannelNetwork::solve(const Vector& rhs) const noexcept
{
    Vector y{}, x{};
    for (int row = 0; row < kInternal; ++row)
    {
        double value = rhs[row];
        for (int col = 0; col < row; ++col) value -= m_factor[row][col] * y[col];
        y[row] = value / m_factor[row][row];
    }
    for (int row = kInternal - 1; row >= 0; --row)
    {
        double value = y[row];
        for (int col = row + 1; col < kInternal; ++col) value -= m_factor[col][row] * x[col];
        x[row] = value / m_factor[row][row];
    }
    return x;
}

void IronheartChannelNetwork::updateMatrix(double lead, double rhythm, double clean) noexcept
{
    m_matrix = {};
    for (const auto& cap : m_caps) stamp(cap.from, cap.to, cap.admittance());
    stamp(LeadTop, Ground, 1.0 / 150e3); // R21, placa principal
    stamp(RhythmTop, Ground, 1.0 / 150e3); // R2, placa CH1
    stamp(CH1, CleanTop, 1.0 / 220e3); // R3, placa CH1
    stamp(CleanTop, Ground, 1.0 / 4.7e3); // R4 é 4k7, não 47k
    const auto pot = [this](int top, int wiper, double position) {
        const double lower = 220e3 * logTaper(position);
        // 1 ohm regulariza os extremos sem criar condutância infinita.
        stamp(top, wiper, 1.0 / std::max(1.0, 220e3 - lower));
        stamp(wiper, Ground, 1.0 / std::max(1.0, lower));
    };
    pot(LeadTop, LeadWiper, lead);
    pot(RhythmTop, RhythmWiper, rhythm);
    pot(CleanTop, CleanWiper, clean);
    stamp(LeadBus, Ground, 1.0 / 1e6); // R31
    stamp(LeadGrid, Ground, 1.0 / 1e6); // R4, placa principal
    // Todos os modos chegam à grade de V1B via R31/C5/R4. A leitura
    // anterior de R187 como terminação de CH1 era incorreta: ele é um
    // shunt comutado para terra no nó de V2B, modelado no bloco seguinte.
    const int selected = m_channel == Channel::Lead ? LeadWiper
        : m_channel == Channel::Clean ? CleanWiper : RhythmWiper;
    stamp(selected, LeadBus, 1.0); // contato fechado aproximado por 1 ohm

    // Cholesky da rede passiva: matriz simétrica definida positiva.
    // Calculada só quando posição dos pots ou seleção muda, sem alocação.
    for (int row = 0; row < kInternal; ++row)
        for (int col = 0; col <= row; ++col)
        {
            double value = m_matrix[row + 1][col + 1];
            for (int k = 0; k < col; ++k) value -= m_factor[row][k] * m_factor[col][k];
            m_factor[row][col] = row == col ? std::sqrt(value) : value / m_factor[col][col];
        }
    for (int n = 0; n < kInternal; ++n) m_plateCoupling[n] = -m_matrix[n + 1][Plate];
    m_response = solve(m_plateCoupling);
    m_portConductance = m_matrix[Plate][Plate];
    for (int n = 0; n < kInternal; ++n) m_portConductance -= m_plateCoupling[n] * m_response[n];
    m_lastLead = lead; m_lastRhythm = rhythm; m_lastClean = clean;
    m_matrixDirty = false;
}

IronheartTriodeStage::PlateLoad IronheartChannelNetwork::beginSample() noexcept
{
    if (!m_prepared) return {};
    const double lead = bounded(m_lead.nextValue());
    const double rhythm = bounded(m_rhythm.nextValue());
    const double clean = bounded(m_clean.nextValue());
    if (m_matrixDirty || lead != m_lastLead || rhythm != m_lastRhythm || clean != m_lastClean)
        updateMatrix(lead, rhythm, clean);
    std::array<double, kNodes> rhs{};
    for (const auto& cap : m_caps)
    {
        rhs[cap.from] += cap.source();
        rhs[cap.to] -= cap.source();
    }
    Vector internal{};
    for (int n = 0; n < kInternal; ++n) internal[n] = rhs[n + 1];
    m_offset = solve(internal);
    double history = rhs[Plate];
    for (int n = 0; n < kInternal; ++n) history += m_plateCoupling[n] * m_offset[n];
    return {m_portConductance, history};
}

IronheartChannelNetwork::Outputs IronheartChannelNetwork::finishSample(double plate) noexcept
{
    if (!m_prepared) return {};
    std::array<double, kNodes> volts{};
    volts[Plate] = plate;
    for (int n = 0; n < kInternal; ++n) volts[n + 1] = m_response[n] * plate + m_offset[n];
    for (auto& cap : m_caps)
    {
        const double drop = volts[cap.from] - volts[cap.to];
        const double current = cap.admittance() * drop - cap.source();
        const double capacitorVolts = drop - cap.seriesResistance * current;
        cap.history = 2.0 * cap.conductance * capacitorVolts - cap.history;
    }
    // Um cursor no batente inferior é terra. A regularização de 1 ohm
    // serve apenas ao solver e não deve deixar escapar áudio no zero.
    // C5 ainda pode descarregar sobre R4 com Lead Gain no zero.
    return {m_channel == Channel::Lead ? volts[LeadGrid] : 0.0,
            m_lastRhythm == 0.0 ? 0.0 : volts[RhythmWiper],
            m_lastClean == 0.0 ? 0.0 : volts[CleanWiper], volts[LeadGrid]};
}
