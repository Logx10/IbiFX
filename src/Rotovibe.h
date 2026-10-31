#pragma once

#include <array>
#include <vector>

#include "AudioModule.h"
#include "SmoothedValue.h"

// Uni-Vibe inspirado no pedal de expressão: quatro filtros all-pass em série.
// Chorus mistura sinal direto e filtrado; Vibrato entrega só o filtrado.
// A prancha controla a velocidade do LFO, e Intensity controla a varredura.
class Rotovibe : public AudioModule
{
public:
    Rotovibe();

    const char* name() const override;
    void prepare(double sampleRate, int blockSize) override;
    void reset() override;
    void process(std::vector<float>& buffer) override;

private:
    struct Stage
    {
        float previousInput = 0.0f;
        float previousOutput = 0.0f;
    };

    double m_sampleRate = 44100.0;
    double m_phase = 0.0;
    std::array<Stage, 4> m_stages{};
    std::array<float, 4> m_coefficients{};
    std::array<float, 4> m_coefficientSteps{};
    int m_samplesUntilUpdate = 0;
    SmoothedValue m_rate;
    SmoothedValue m_intensity;
    SmoothedValue m_wetMix;
};
