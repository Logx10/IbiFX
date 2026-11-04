// Testes do Flanger.
//
// Com Range em 0, o atraso fica parado no mínimo (0,8 ms) e o efeito vira
// um filtro pente fixo — dá para prever cada vale e cada pico:
// vales em 1/(2τ) = 625 Hz, 1875 Hz...; picos em 1/τ = 1250 Hz, 2500 Hz...
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "Flanger.h"
#include "test_helpers.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;

double fixedCombGainDb(double frequency, float color)
{
    Flanger flanger;
    flanger.setRange(0.0f);
    flanger.setColor(color);
    flanger.prepare(kSampleRate, 512);

    std::vector<float> signal(24000);
    for (std::size_t i = 0; i < signal.size(); ++i)
        signal[i] = 0.1f * static_cast<float>(std::sin(kTwoPi * frequency * static_cast<double>(i) / kSampleRate));
    flanger.process(signal);

    double sum = 0.0;
    for (std::size_t i = signal.size() / 2; i < signal.size(); ++i)
        sum += static_cast<double>(signal[i]) * static_cast<double>(signal[i]);
    const double rms = std::sqrt(sum / static_cast<double>(signal.size() / 2));
    return 20.0 * std::log10(rms / (0.1 / std::sqrt(2.0)));
}
}

// Sem Color: o vale em 625 Hz some, o pico em 1250 Hz sai inteiro.
void testCombNotchAndPeak()
{
    std::cout << "pente de 0,8 ms: vale em 625 Hz, pico em 1250 Hz\n";

    check(fixedCombGainDb(625.0, 0.0f) < -25.0, "625 Hz cancelado (abaixo de -25 dB)");
    check(std::fabs(fixedCombGainDb(1250.0, 0.0f)) < 0.3, "1250 Hz a 0 dB (+-0,3)");
}

// Com Color c, o atrasado é x·z^-D / (1 - c·z^-D): no pico (z^-D = 1) ele
// vale 1/(1-c). Saída = 0,5·(1 + 1/(1-c)); com c = 0,5, 1,5 = +3,5 dB.
void testColorRaisesThePeakByTheFormula()
{
    std::cout << "Color 0,5 sobe o pico para +3,5 dB\n";

    const double expectedDb = 20.0 * std::log10(0.5 * (1.0 + 1.0 / (1.0 - 0.5)));
    check(std::fabs(fixedCombGainDb(1250.0, 0.5f) - expectedDb) < 0.3, "+3,5 dB em 1250 Hz (+-0,3)");
}

// Color no máximo, Range no máximo, ruído: a realimentação abaixo de 1
// sempre morre — a saída fica finita.
void testMaxColorStaysStable()
{
    std::cout << "Color maximo continua estavel\n";

    Flanger flanger;
    flanger.setRate(10.0f);
    flanger.setRange(1.0f);
    flanger.setColor(0.9f);
    flanger.prepare(kSampleRate, 256);

    float peak = 0.0f;
    unsigned seed = 3u;
    for (int block = 0; block < 400; ++block)
    {
        std::vector<float> buffer(256);
        for (float& sample : buffer)
        {
            seed = seed * 1664525u + 1013904223u;
            sample = static_cast<float>(seed >> 8) / static_cast<float>(1u << 23) - 1.0f;
        }
        flanger.process(buffer);
        for (float sample : buffer)
            peak = std::max(peak, std::isfinite(sample) ? std::fabs(sample) : 1e9f);
    }
    check(peak < 20.0f, "saida finita e limitada");
}

int main()
{
    std::cout << "\n=== testes do Flanger ===\n\n";

    testCombNotchAndPeak();
    testColorRaisesThePeakByTheFormula();
    testMaxColorStaysStable();

    return reportResults();
}
