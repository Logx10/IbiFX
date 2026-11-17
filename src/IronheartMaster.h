#pragma once

#include "AudioModule.h"
#include "IronheartPowerSection.h"
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
    float m_dcX = 0.0f, m_dcY = 0.0f, m_dcR = 0.0f, m_dcG = 1.0f;
    IronheartPowerSection m_powerSection;
    Reverb m_reverb;
    SmoothedValue m_level, m_reverbLevel;
};
