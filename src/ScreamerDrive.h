#pragma once

#include "AudioModule.h"
#include "Oversampler.h"
#include "SmoothedValue.h"

// Aproxima o estágio não inversor do Tube Screamer. O sinal direto chega
// à soma junto com a parcela filtrada e limitada pelos diodos.
class ScreamerDrive : public AudioModule
{
public:
    ScreamerDrive();
    const char* name() const override;
    void prepare(double sampleRate, int blockSize) override;
    void reset() override;
    void process(std::vector<float>& buffer) override;

private:
    Oversampler m_oversampler;
    SmoothedValue m_drive;
    float m_feedbackCoefficient = 0.0f;
    float m_feedbackLow = 0.0f;
};
