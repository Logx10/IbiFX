// Testes do Phaser.
//
// O LFO começa no meio (seno em 0 -> 0,5), então nos primeiros
// milissegundos fc está na média geométrica da faixa: √(141 · 1600) ≈ 475
// Hz. Com o Speed no mínimo (0,1 Hz), ele mal se move em 50 ms, e os vales
// ficam onde a conta do header manda: 0,414 · fc ≈ 197 Hz e 2,414 · fc ≈
// 1147 Hz.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "Phaser.h"
#include "test_helpers.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;

// Ganho entre 10 e 50 ms — depois de os all-pass assentarem, antes de o
// LFO andar.
double earlyGainDb(double frequency, float feedback)
{
    Phaser phaser;
    phaser.setRate(0.1f);
    phaser.setFeedback(feedback);
    phaser.prepare(kSampleRate, 512);

    std::vector<float> signal(2400);
    for (std::size_t i = 0; i < signal.size(); ++i)
        signal[i] = 0.1f * static_cast<float>(std::sin(kTwoPi * frequency * static_cast<double>(i) / kSampleRate));
    phaser.process(signal);

    double sum = 0.0;
    for (std::size_t i = 480; i < signal.size(); ++i)
        sum += static_cast<double>(signal[i]) * static_cast<double>(signal[i]);
    const double rms = std::sqrt(sum / static_cast<double>(signal.size() - 480));
    return 20.0 * std::log10(rms / (0.1 / std::sqrt(2.0)));
}
}

// Os dois vales caem onde a conta dos quatro all-pass prevê; entre eles
// (600 Hz) o sinal passa quase inteiro.
void testNotchesWhereTheMathSays()
{
    std::cout << "vales em 0,414*fc e 2,414*fc\n";

    const double fc = std::sqrt(141.0 * 1600.0);

    check(earlyGainDb(0.414 * fc, 0.0f) < -15.0, "primeiro vale (~197 Hz) abaixo de -15 dB");
    check(earlyGainDb(2.414 * fc, 0.0f) < -15.0, "segundo vale (~1147 Hz) abaixo de -15 dB");
    check(earlyGainDb(600.0, 0.0f) > -3.0, "600 Hz, entre os vales, passa a menos de 3 dB");
}

// Sem realimentação, metade seco + metade all-pass nunca passa do volume
// da entrada: |0,5·(1 + e^jφ)| <= 1 para qualquer fase.
void testNoFeedbackNeverBoosts()
{
    std::cout << "sem feedback nao aumenta nenhuma frequencia\n";

    for (double frequency : {100.0, 600.0, 3000.0})
        check(earlyGainDb(frequency, 0.0f) < 0.1, "nada acima de 0 dB");
}

// Feedback no máximo, ruído no talo, LFO rápido: a saída continua finita.
void testMaxFeedbackStaysStable()
{
    std::cout << "feedback maximo continua estavel\n";

    Phaser phaser;
    phaser.setRate(8.0f);
    phaser.setFeedback(0.7f);
    phaser.prepare(kSampleRate, 256);

    float peak = 0.0f;
    unsigned seed = 1u;
    for (int block = 0; block < 400; ++block)
    {
        std::vector<float> buffer(256);
        for (float& sample : buffer)
        {
            seed = seed * 1664525u + 1013904223u;
            sample = static_cast<float>(seed >> 8) / static_cast<float>(1u << 23) - 1.0f;
        }
        phaser.process(buffer);
        for (float sample : buffer)
            peak = std::max(peak, std::isfinite(sample) ? std::fabs(sample) : 1e9f);
    }
    check(peak < 10.0f, "saida finita e limitada");
}

int main()
{
    std::cout << "\n=== testes do Phaser ===\n\n";

    testNotchesWhereTheMathSays();
    testNoFeedbackNeverBoosts();
    testMaxFeedbackStaysStable();

    return reportResults();
}
