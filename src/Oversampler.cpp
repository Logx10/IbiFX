#include "Oversampler.h"

#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;

// Corte do passa-baixa, como fração da taxa SOBREAMOSTRADA: 22 kHz de
// 192 kHz. Ver o comentário do header sobre a escolha.
constexpr double kCutoff = 22000.0 / 192000.0;
}

Oversampler::Oversampler()
{
    // Força o cálculo dos coeficientes aqui, ao construir o módulo, e não
    // na primeira chamada de process() — que seria na thread de áudio.
    coefficients();
}

void Oversampler::reset()
{
    m_upHistory.fill(0.0f);
    m_downHistory.fill(0.0f);
    m_upPosition = 0;
    m_downPosition = 0;
}

const std::array<float, Oversampler::kTaps>& Oversampler::coefficients()
{
    static const std::array<float, kTaps> taps = []
    {
        std::array<float, kTaps> result{};
        const double center = static_cast<double>(kTaps - 1) / 2.0;
        double sum = 0.0;

        for (std::size_t n = 0; n < kTaps; ++n)
        {
            const double x = static_cast<double>(n) - center;
            const double sinc = (std::fabs(x) < 1e-9)
                ? 2.0 * kCutoff
                : std::sin(2.0 * kPi * kCutoff * x) / (kPi * x);
            const double window = 0.42 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(n) / (kTaps - 1))
                                + 0.08 * std::cos(4.0 * kPi * static_cast<double>(n) / (kTaps - 1));

            const double tap = sinc * window;
            result[n] = static_cast<float>(tap);
            sum += tap;
        }

        // Ganho exatamente 1 em DC: um sinal grave passa sem mudar de volume.
        for (float& tap : result)
            tap = static_cast<float>(static_cast<double>(tap) / sum);

        return result;
    }();

    return taps;
}
