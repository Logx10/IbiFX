#pragma once

#include <array>
#include "IronheartCathodeFollower.h"
#include "SmoothedValue.h"

// Rede de tone Lead/Rhythm do IRT120H, incluindo R39/R75 e R48/C24.
// Mesma topologia nos dois canais, com estados e controles independentes.
class IronheartToneNetwork
{
public:
    void prepare(double internalSampleRate, float bass, float mid, float treble,
                 bool deep, bool midShift, bool trebleShift);
    void setControls(float bass, float mid, float treble,
                     bool deep, bool midShift, bool trebleShift) noexcept;
    void reset() noexcept;
    // begin → resolver seguidor com esta carga → finish(cátodo AC resolvido).
    IronheartCathodeFollower::Load beginSample() noexcept;
    double finishSample(double cathodeACVolts) noexcept;

private:
    static constexpr int kNodes = 9, kInternal = 8;
    enum Node { Cathode, Feed, Snubber, Slope, TrebleTop, BassTop, MidTop,
                MidWiper, Output, Ground = -1 };
    using Vector = std::array<double, kInternal>;
    using Matrix = std::array<std::array<double, kNodes>, kNodes>;
    struct Capacitor
    {
        int from = Ground, to = Ground;
        double conductance = 0.0, seriesResistance = 0.0, history = 0.0;
        double admittance() const noexcept;
        double source() const noexcept;
    };
    void updateMatrix(double bass, double mid, double treble) noexcept;
    void stamp(int a, int b, double g) noexcept;
    Vector solve(const Vector& rhs) const noexcept;
    SmoothedValue m_bass, m_mid, m_treble;
    std::array<Capacitor, 7> m_caps{};
    Matrix m_matrix{}, m_factor{};
    Vector m_coupling{}, m_response{}, m_offset{};
    double m_portG = 0.0;
    double m_lastBass = -1.0, m_lastMid = -1.0, m_lastTreble = -1.0;
    bool m_deep = false, m_midShift = false, m_trebleShift = false;
    bool m_dirty = true, m_prepared = false;
};
