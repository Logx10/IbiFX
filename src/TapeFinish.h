#pragma once

#include "AudioModule.h"
#include "Oversampler.h"
#include "SmoothedValue.h"

// Acabamento de fita. Suaviza picos e colore a resposta após a saturação.
class TapeFinish : public AudioModule
{
public:
    TapeFinish();
    const char* name() const override;
    void prepare(double sampleRate, int blockSize) override;
    void reset() override;
    void process(std::vector<float>& buffer) override;

private:
    Oversampler m_oversampler;
    SmoothedValue m_drive;
    SmoothedValue m_color;
    SmoothedValue m_output;
    float m_trebleCoefficient = 0.0f;
    float m_bassUpperCoefficient = 0.0f;
    float m_bassLowerCoefficient = 0.0f;
    float m_trebleLow = 0.0f;
    float m_bassUpper = 0.0f;
    float m_bassLower = 0.0f;
};
