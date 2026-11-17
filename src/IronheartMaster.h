#pragma once

#include "AudioModule.h"
#include "PowerAmp.h"
#include "Reverb.h"
#include "SmoothedValue.h"

// Seção após o FX Send/Return. Separada do pré para permitir inserir efeitos
// na cadeia entre as duas seções, ou usar o pré com outra potência.
class IronheartMaster : public AudioModule
{
public:
    IronheartMaster();
    const char* name() const override;
    void prepare(double sampleRate, int blockSize) override;
    void reset() override;
    void process(std::vector<float>& buffer) override;

private:
    enum Index { Dynamics, Tone, ReverbOn, ReverbLevel, Watts, Power, Play };
    double m_sampleRate = 44100.0;
    float m_bass = 0.0f;
    float m_top = 0.0f;
    PowerAmp m_powerAmp;
    Reverb m_reverb;
    SmoothedValue m_level;
    SmoothedValue m_dynamics;
    SmoothedValue m_tone;
};
