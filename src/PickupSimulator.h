#pragma once

#include <atomic>
#include <vector>

// Simulacao global da captacao na entrada, antes da cadeia de efeitos.
// Os perfis representam captadores genericos, nao modelos de instrumentos.
class PickupSimulator
{
public:
    enum class Mode { Original, SingleToHumbucker, HumbuckerToSingle };

    void prepare(double sampleRate);
    void setMode(Mode mode);
    Mode mode() const;
    void process(std::vector<float>& buffer);

private:
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;

        float process(float sample);
        void reset();
    };

    static Biquad makeConversion(double sampleRate, double sourceHz, double sourceQ,
                                 double targetHz, double targetQ, double levelDb);

    Biquad m_singleToHumbucker;
    Biquad m_humbuckerToSingle;
    std::atomic<Mode> m_mode{Mode::Original};
    Mode m_activeMode = Mode::Original;
    double m_weights[3] = {1.0, 0.0, 0.0};
    double m_transition = 1.0;
};
