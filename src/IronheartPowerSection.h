#pragma once

#include <array>
#include "Oversampler.h"
#include "SmoothedValue.h"

// Modelo reduzido da potência: rede dupla Tone/Watts, realimentação grave
// e saturação com sag. Não é um solver de V4, quatro 6L6 e transformador.
class IronheartPowerSection
{
public:
    void prepare(double sampleRate, float tone, float watts, float dynamics);
    void setControls(float tone, float watts, float dynamics) noexcept;
    void reset() noexcept;
    float processSample(float input) noexcept;

private:
    struct Branch
    {
        // Nós: após C64/69; cursor Watts; após C33/35. C32/34 em
        // série com Tone vai do primeiro nó ao terra.
        std::array<std::array<double, 3>, 3> inverse{};
        std::array<double, 3> conductance{}, history{};
        std::array<double, 3> response{}, offset{};
        double sourceResistance = 0.0, toneResistance = 0.0;
        double lastTone = -1.0, lastWatts = -1.0;
        void prepare(double rate, double sourceR) noexcept;
        void reset() noexcept;
        std::array<double, 2> beginSample(double tone, double watts) noexcept;
        double finishSample(double source) noexcept;
    };
    double processInternal(double input) noexcept;
    double m_rate = 176400.0, m_feedbackHistory = 0.0, m_envelope = 0.0;
    double m_inputHistory = 0.0, m_inputG = 0.0, m_attack = 0.0, m_release = 0.0;
    Branch m_positive, m_negative;
    SmoothedValue m_tone, m_watts, m_dynamics;
    Oversampler m_oversampler;
};
