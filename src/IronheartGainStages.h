#pragma once

#include "IronheartTriodeStage.h"
#include "SmoothedValue.h"

// V1B → V2A → V2B e divisores de grade dos seguidores V3A/V3B.
// Grade ideal (sem corrente/Miller): o fluxo não linear é unidirecional;
// cada placa é resolvida junto da carga RC que alimenta a grade seguinte.
class IronheartGainStages
{
public:
    struct Signals
    {
        double v1bPlateAC = 0.0, v2aGrid = 0.0, v2aPlateAC = 0.0;
        double v2bGrid = 0.0, v2bPlateAC = 0.0, followerGridAC = 0.0;
        double cleanShuntConductance = 0.0;
    };
    void prepare(double internalSampleRate, bool clean);
    void setClean(bool clean) noexcept;
    void reset() noexcept;
    // Entrada: volts na grade de V1B; saída: volts AC na grade de V3.
    double processSample(double gridVolts) noexcept;

    // Diagnóstico na mesma thread do áudio ou com processamento parado.
    const IronheartTriodeStage& v1b() const noexcept { return m_v1b; }
    const IronheartTriodeStage& v2a() const noexcept { return m_v2a; }
    const IronheartTriodeStage& v2b() const noexcept { return m_v2b; }
    Signals signals() const noexcept { return m_signals; }

private:
    struct Coupling
    {
        double conductance = 0.0, history = 0.0;
        IronheartTriodeStage::PlateLoad load(double seriesResistance, double termination) const noexcept;
        double finish(double plateAC, double seriesResistance, double termination) noexcept;
    };
    IronheartTriodeStage m_v1b, m_v2a, m_v2b;
    Coupling m_c6, m_c7c3;
    SmoothedValue m_clean;
    double m_plateCapConductance = 0.0, m_plateCapHistory = 0.0;
    double m_outputCapConductance = 0.0, m_outputCapHistory = 0.0;
    Signals m_signals;
    bool m_prepared = false;
};
