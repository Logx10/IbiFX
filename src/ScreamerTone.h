#pragma once

#include "AudioModule.h"
#include "SmoothedValue.h"

// Rede de Tone do Tube Screamer depois da clipagem, com o op amp ideal.
class ScreamerTone : public AudioModule
{
public:
    ScreamerTone();
    const char* name() const override;
    void prepare(double sampleRate, int blockSize) override;
    void reset() override;
    void process(std::vector<float>& buffer) override;

private:
    void updateCoefficients(float position);

    SmoothedValue m_tone;
    double m_bilinearScale = 96000.0;
    double m_b0 = 1.0;
    double m_b1 = 0.0;
    double m_b2 = 0.0;
    double m_a1 = 0.0;
    double m_a2 = 0.0;
    double m_x1 = 0.0;
    double m_x2 = 0.0;
    double m_y1 = 0.0;
    double m_y2 = 0.0;
};
