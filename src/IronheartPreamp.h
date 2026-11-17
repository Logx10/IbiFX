#pragma once

#include "AudioModule.h"
#include "IronheartInputStage.h"
#include "IronheartChannelNetwork.h"
#include "IronheartTriodeStage.h"
#include "IronheartGainStages.h"
#include "IronheartCathodeFollower.h"
#include "IronheartToneNetwork.h"
#include "IronheartOutputStage.h"
#include "Oversampler.h"
#include "SmoothedValue.h"

// Pré IRT120H: V1/V2/V3 e redes de ganho/tone baseados no esquema.
// Inclui volumes/buffers até a placa de loop. Grades ideais, sem Miller.
// O objeto possui todos os filtros e históricos, sem ponteiros emprestados.
class IronheartPreamp : public AudioModule
{
public:
    IronheartPreamp();
    const char* name() const override;
    void prepare(double sampleRate, int blockSize) override;
    void reset() override;
    void process(std::vector<float>& buffer) override;

private:
    enum Index { Lead, Clean, BoostOn, Boost, CleanVolume,
                 RhythmGain, RhythmBass, RhythmMid, RhythmTreble,
                 RhythmDeep, RhythmMidShift, RhythmTrebleShift, RhythmVolume,
                 LeadGain, LeadBass, LeadMid, LeadTreble,
                 LeadDeep, LeadMidShift, LeadTrebleShift, LeadVolume };
    float value(Index index) const;
    void configure();

    double m_sampleRate = 44100.0;
    Oversampler m_oversampler;
    IronheartInputStage m_inputStage;
    IronheartTriodeStage m_v1a;
    IronheartChannelNetwork m_channels;
    IronheartGainStages m_gainStages;
    IronheartCathodeFollower m_leadFollower, m_rhythmFollower;
    IronheartToneNetwork m_leadTone, m_rhythmTone;
    IronheartOutputStage m_outputStage;
    SmoothedValue m_leadMix, m_outputEnable;
};
