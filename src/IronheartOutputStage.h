#pragma once

#include "SmoothedValue.h"

// Volumes e buffers entre os EQs e a placa de loop. JFETs são chaves
// ideais; TL072 mantém terra virtual e os capacitores de realimentação.
// O objeto possui estados próprios, sem buffers ou memória emprestada.
class IronheartOutputStage
{
public:
    struct Loads { double lead, rhythm; }; // siemens, vistos pelos EQs
    void prepare(double internalRate, float leadVolume, float rhythmVolume, bool clean);
    void setControls(float leadVolume, float rhythmVolume, bool clean) noexcept;
    void reset() noexcept;
    Loads beginSample() noexcept;
    double finishSample(double leadToneVolts, double rhythmToneVolts, double leadMix) noexcept;

private:
    struct LowPass
    {
        double g = 0.0, history = 0.0;
        double process(double input) noexcept;
    };
    SmoothedValue m_lead, m_rhythm, m_clean;
    LowPass m_leadFeedback, m_rhythmFeedback, m_sumFeedback;
    double m_leadTransfer = 0.0, m_rhythmTransfer = 0.0;
};
