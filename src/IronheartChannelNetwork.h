#pragma once

#include <array>

#include "IronheartTriodeStage.h"
#include "SmoothedValue.h"

// Rede passiva específica após V1A (9505/9504, 2011). Não é um solver
// genérico de circuitos. Capacitores e matrizes têm tamanho fixo, sem heap.
class IronheartChannelNetwork
{
public:
    enum class Channel { Clean, Rhythm, Lead };
    struct Outputs { double lead = 0.0, rhythm = 0.0, clean = 0.0, grid = 0.0; };
    void prepare(double internalSampleRate, Channel channel,
                 float leadGain, float rhythmGain, float cleanVolume);
    void setControls(Channel channel, float leadGain, float rhythmGain, float cleanVolume) noexcept;
    void reset() noexcept;

    // Usar nesta ordem, uma vez por amostra interna: begin → resolver V1A
    // com a carga retornada → finish com volts AC da placa resolvida.
    IronheartTriodeStage::PlateLoad beginSample() noexcept;
    Outputs finishSample(double plateACVolts) noexcept;

private:
    // Nó 0 é a placa; os outros nove são eliminados para obter sua carga.
    static constexpr int kNodes = 10, kInternal = kNodes - 1;
    enum Node { Plate, LeadTop, LeadWiper, CH1, RhythmTop, RhythmWiper,
                CleanTop, CleanWiper, LeadGrid, LeadBus, Ground = -1 };
    using Vector = std::array<double, kInternal>;
    using Matrix = std::array<std::array<double, kNodes>, kNodes>;
    struct Capacitor
    {
        int from = Ground, to = Ground;
        double conductance = 0.0, seriesResistance = 0.0;
        double history = 0.0;
        double admittance() const noexcept;
        double source() const noexcept;
    };
    void updateMatrix(double lead, double rhythm, double clean) noexcept;
    void stamp(int from, int to, double conductance) noexcept;
    Vector solve(const Vector& rhs) const noexcept;

    Channel m_channel = Channel::Clean;
    SmoothedValue m_lead, m_rhythm, m_clean;
    std::array<Capacitor, 5> m_caps{};
    Matrix m_matrix{}, m_factor{};
    Vector m_plateCoupling{}, m_response{}, m_offset{};
    double m_portConductance = 0.0;
    double m_lastLead = -1.0, m_lastRhythm = -1.0, m_lastClean = -1.0;
    bool m_matrixDirty = true;
    bool m_prepared = false;
};
