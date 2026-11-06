// Testes do Wah.
//
// Mede o ganho como o ouvido mede: um seno puro passando pelo filtro, RMS
// da saída contra o da entrada, depois de o filtro assentar.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <cmath>
#include <iostream>
#include <vector>

#include "Wah.h"
#include "test_helpers.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;

double gainAtDb(float position, float resonance, double frequency)
{
    Wah wah;
    wah.setPosition(position);
    wah.setResonance(resonance);
    wah.prepare(kSampleRate, 512);

    std::vector<float> signal(24000);
    for (std::size_t i = 0; i < signal.size(); ++i)
        signal[i] = 0.1f * static_cast<float>(std::sin(kTwoPi * frequency * static_cast<double>(i) / kSampleRate));
    wah.process(signal);

    double sum = 0.0;
    for (std::size_t i = signal.size() / 2; i < signal.size(); ++i)
        sum += static_cast<double>(signal[i]) * static_cast<double>(signal[i]);
    const double rms = std::sqrt(sum / static_cast<double>(signal.size() / 2));
    return 20.0 * std::log10(rms / (0.1 / std::sqrt(2.0)));
}
}

// A faixa do WH-10, com o meio do curso no meio em OITAVAS (a média
// geométrica), não em Hz.
void testSweepRangeIsExponential()
{
    std::cout << "varredura de 350 Hz a 2,2 kHz, exponencial\n";

    checkClose(Wah::centerFrequency(0.0f), 350.0f, "calcanhar em 350 Hz");
    check(std::fabs(Wah::centerFrequency(1.0f) - 2200.0f) < 0.1f, "ponta em 2200 Hz");
    check(std::fabs(Wah::centerFrequency(0.5f) - std::sqrt(350.0f * 2200.0f)) < 0.5f,
          "meio do curso na media geometrica (~877 Hz)");
}

// O pico anda com o pé: calcanhar realça o grave, ponta realça o agudo.
void testPeakFollowsThePedal()
{
    std::cout << "o pico anda com o pedal\n";

    const double heelLow = gainAtDb(0.0f, 3.0f, 350.0);
    const double heelHigh = gainAtDb(0.0f, 3.0f, 2200.0);
    const double toeLow = gainAtDb(1.0f, 3.0f, 350.0);
    const double toeHigh = gainAtDb(1.0f, 3.0f, 2200.0);

    check(heelLow - heelHigh > 10.0, "calcanhar: 350 Hz pelo menos 10 dB acima de 2,2 kHz");
    check(toeHigh - toeLow > 10.0, "ponta: 2,2 kHz pelo menos 10 dB acima de 350 Hz");
}

// No centro, o passa-banda normalizado dá 1 (fase 0) e o passa-baixa dá Q
// com fase -90°. Saída = 2·1 + 0,25·Q·(-j): com Q = 3, |2 - 0,75j| = 2,136,
// ou +6,6 dB. Confere a conta da saída do header com o filtro rodando.
void testGainAtTheCenterMatchesTheFormula()
{
    std::cout << "ganho no centro bate com a conta (+6,6 dB com Q 3)\n";

    const double center = Wah::centerFrequency(0.5f);
    const double gainDb = gainAtDb(0.5f, 3.0f, center);
    const double expectedDb = 20.0 * std::log10(std::hypot(2.0, 0.25 * 3.0));

    check(std::fabs(gainDb - expectedDb) < 0.3, "+6,6 dB no centro (+-0,3)");
}

// Mais resonance = pico mais estreito: meia oitava acima do centro, Q 8
// corta mais que Q 2.
void testResonanceNarrowsThePeak()
{
    std::cout << "resonance estreita o pico\n";

    const double offCenter = Wah::centerFrequency(0.5f) * std::sqrt(2.0);
    check(gainAtDb(0.5f, 8.0f, offCenter) < gainAtDb(0.5f, 2.0f, offCenter) - 3.0,
          "meia oitava acima do centro, Q 8 fica 3 dB abaixo de Q 2");
}

// Pisar no pedal de um extremo ao outro com o sinal no talo não pode fazer
// o filtro disparar — é o caso que a forma TPT existe para aguentar.
void testFastSweepStaysStable()
{
    std::cout << "varrer o pedal rapido nao dispara o filtro\n";

    Wah wah;
    wah.setResonance(10.0f);
    wah.prepare(kSampleRate, 64);

    float peak = 0.0f;
    for (int block = 0; block < 400; ++block)
    {
        wah.setPosition((block / 10) % 2 == 0 ? 0.0f : 1.0f);

        std::vector<float> buffer(64);
        for (std::size_t i = 0; i < buffer.size(); ++i)
            buffer[i] = (i % 2 == 0) ? 1.0f : -1.0f;
        wah.process(buffer);

        for (float sample : buffer)
            peak = std::max(peak, std::isfinite(sample) ? std::fabs(sample) : 1e9f);
    }
    check(peak < 20.0f, "saida finita e limitada");
}

int main()
{
    std::cout << "\n=== testes do Wah ===\n\n";

    testSweepRangeIsExponential();
    testPeakFollowsThePedal();
    testGainAtTheCenterMatchesTheFormula();
    testResonanceNarrowsThePeak();
    testFastSweepStaysStable();

    return reportResults();
}
