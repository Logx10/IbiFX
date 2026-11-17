#include "IronheartToneNetwork.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
float bounded(float value) { return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f; }
}

double IronheartToneNetwork::Capacitor::admittance() const noexcept
{ return conductance / (1.0 + conductance * seriesResistance); }
double IronheartToneNetwork::Capacitor::source() const noexcept
{ return history / (1.0 + conductance * seriesResistance); }

void IronheartToneNetwork::prepare(double rate, float bass, float mid, float treble,
                                  bool deep, bool midShift, bool trebleShift)
{
    if (!std::isfinite(rate) || rate <= 0.0)
        throw std::invalid_argument("IronheartToneNetwork exige taxa interna positiva e finita");
    m_bass.prepare(rate, kDefaultRampSeconds, bounded(bass));
    m_mid.prepare(rate, kDefaultRampSeconds, bounded(mid));
    m_treble.prepare(rate, kDefaultRampSeconds, bounded(treble));
    m_deep = deep; m_midShift = midShift; m_trebleShift = trebleShift;
    m_caps = {{{Snubber, Ground, 2 * rate * 10e-9, 0, 0},
               {Feed, TrebleTop, 2 * rate * 470e-12, 0, 0},
               {Feed, TrebleTop, 2 * rate * 470e-12, 10e6, 0},
               {Slope, BassTop, 2 * rate * 22e-9, 0, 0},
               {Slope, BassTop, 2 * rate * 100e-9, 10e6, 0},
               {Slope, MidWiper, 2 * rate * 22e-9, 0, 0},
               {Slope, MidWiper, 2 * rate * 10e-9, 10e6, 0}}};
    m_prepared = true;
    reset();
}

void IronheartToneNetwork::setControls(float bass, float mid, float treble,
                                      bool deep, bool midShift, bool trebleShift) noexcept
{
    m_bass.setTarget(bounded(bass)); m_mid.setTarget(bounded(mid)); m_treble.setTarget(bounded(treble));
    if (deep != m_deep || midShift != m_midShift || trebleShift != m_trebleShift) m_dirty = true;
    m_deep = deep; m_midShift = midShift; m_trebleShift = trebleShift;
}

void IronheartToneNetwork::reset() noexcept
{
    m_bass.snapTo(m_bass.target()); m_mid.snapTo(m_mid.target()); m_treble.snapTo(m_treble.target());
    for (auto& cap : m_caps) cap.history = 0;
    m_offset = {}; m_dirty = true;
}

void IronheartToneNetwork::stamp(int a, int b, double g) noexcept
{
    if (a != Ground) m_matrix[a][a] += g;
    if (b != Ground) m_matrix[b][b] += g;
    if (a != Ground && b != Ground) { m_matrix[a][b] -= g; m_matrix[b][a] -= g; }
}

IronheartToneNetwork::Vector IronheartToneNetwork::solve(const Vector& rhs) const noexcept
{
    Vector y{}, x{};
    for (int row = 0; row < kInternal; ++row)
    {
        double v = rhs[row];
        for (int col = 0; col < row; ++col) v -= m_factor[row][col] * y[col];
        y[row] = v / m_factor[row][row];
    }
    for (int row = kInternal - 1; row >= 0; --row)
    {
        double v = y[row];
        for (int col = row + 1; col < kInternal; ++col) v -= m_factor[col][row] * x[col];
        x[row] = v / m_factor[row][row];
    }
    return x;
}

void IronheartToneNetwork::updateMatrix(double bass, double mid, double treble) noexcept
{
    // Pull fecha um contato em paralelo ao resistor de 10M. O contato
    // fechado é regularizado por 1 ohm; a memória de cada capacitor persiste.
    const double closed = 1.0 / (1.0 + 1.0 / 10e6);
    m_caps[2].seriesResistance = m_trebleShift ? closed : 10e6;
    m_caps[4].seriesResistance = m_deep ? closed : 10e6;
    m_caps[6].seriesResistance = m_midShift ? closed : 10e6;
    m_matrix = {};
    for (const auto& cap : m_caps) stamp(cap.from, cap.to, cap.admittance());
    stamp(Cathode, Feed, 1.0 / 47e3); // R39 / R75
    stamp(Feed, Snubber, 1.0 / 47e3); // R48 / R80, em série com 10n
    stamp(Feed, Slope, 1.0 / 47e3); // R49 / R24
    const double bassR = 250e3 * (std::pow(81.0, bass) - 1.0) / 80.0;
    stamp(BassTop, MidTop, 1.0 / std::max(1.0, bassR)); // cursor Bass unido ao topo
    stamp(MidTop, MidWiper, 1.0 / std::max(1.0, 25e3 * (1.0 - mid)));
    stamp(MidWiper, Ground, 1.0 / std::max(1.0, 25e3 * mid));
    stamp(TrebleTop, Output, 1.0 / std::max(1.0, 250e3 * (1.0 - treble)));
    stamp(Output, BassTop, 1.0 / std::max(1.0, 250e3 * treble));
    stamp(Output, Ground, 1.0 / 1e6); // fronteira provisória: entrada do volume de canal
    for (int row = 0; row < kInternal; ++row)
        for (int col = 0; col <= row; ++col)
        {
            double v = m_matrix[row + 1][col + 1];
            for (int k = 0; k < col; ++k) v -= m_factor[row][k] * m_factor[col][k];
            m_factor[row][col] = row == col ? std::sqrt(v) : v / m_factor[col][col];
        }
    for (int n = 0; n < kInternal; ++n) m_coupling[n] = -m_matrix[n + 1][Cathode];
    m_response = solve(m_coupling);
    m_portG = m_matrix[Cathode][Cathode];
    for (int n = 0; n < kInternal; ++n) m_portG -= m_coupling[n] * m_response[n];
    m_lastBass = bass; m_lastMid = mid; m_lastTreble = treble; m_dirty = false;
}

IronheartCathodeFollower::Load IronheartToneNetwork::beginSample() noexcept
{
    if (!m_prepared) return {};
    const double bass = bounded(m_bass.nextValue()), mid = bounded(m_mid.nextValue()), treble = bounded(m_treble.nextValue());
    if (m_dirty || bass != m_lastBass || mid != m_lastMid || treble != m_lastTreble) updateMatrix(bass, mid, treble);
    std::array<double, kNodes> rhs{};
    for (const auto& cap : m_caps)
    {
        if (cap.from != Ground) rhs[cap.from] += cap.source();
        if (cap.to != Ground) rhs[cap.to] -= cap.source();
    }
    Vector internal{};
    for (int n = 0; n < kInternal; ++n) internal[n] = rhs[n + 1];
    m_offset = solve(internal);
    double history = rhs[Cathode];
    for (int n = 0; n < kInternal; ++n) history += m_coupling[n] * m_offset[n];
    return {m_portG, history};
}

double IronheartToneNetwork::finishSample(double cathode) noexcept
{
    if (!m_prepared) return 0.0;
    std::array<double, kNodes> volts{};
    volts[Cathode] = cathode;
    for (int n = 0; n < kInternal; ++n) volts[n + 1] = m_response[n] * cathode + m_offset[n];
    for (auto& cap : m_caps)
    {
        const double drop = (cap.from == Ground ? 0.0 : volts[cap.from]) - (cap.to == Ground ? 0.0 : volts[cap.to]);
        const double current = cap.admittance() * drop - cap.source();
        cap.history = 2.0 * cap.conductance * (drop - cap.seriesResistance * current) - cap.history;
    }
    return volts[Output];
}
