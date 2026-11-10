// Testes do LowPassFilter.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <cmath>
#include <iostream>
#include <vector>

#include "LowPassFilter.h"
#include "test_helpers.h"

namespace
{
constexpr double kSampleRate = 44100.0;

std::vector<float> generateSine(double frequency, std::size_t sampleCount, float amplitude = 0.5f)
{
    std::vector<float> signal(sampleCount);

    for (std::size_t i = 0; i < sampleCount; ++i)
    {
        const double phase = 2.0 * 3.14159265358979323846 * frequency * static_cast<double>(i) / kSampleRate;
        signal[i] = amplitude * static_cast<float>(std::sin(phase));
    }

    return signal;
}

// RMS da segunda metade do sinal — pula o transiente do filtro assentando.
float rmsOfSecondHalf(const std::vector<float>& signal)
{
    const std::size_t start = signal.size() / 2;
    double soma = 0.0;

    for (std::size_t i = start; i < signal.size(); ++i)
    {
        soma += static_cast<double>(signal[i]) * static_cast<double>(signal[i]);
    }

    return static_cast<float>(std::sqrt(soma / static_cast<double>(signal.size() - start)));
}
}

// ---------------------------------------------------------------------

// Um tom bem abaixo do corte passa quase inteiro; um bem acima sai bem
// atenuado — a definição de um passa-baixa, medida com tons de verdade.
void testPassesLowsAndCutsHighs()
{
    std::cout << "passa o grave, corta o agudo\n";

    auto medir = [](double frequency) -> float
    {
        std::vector<float> sinal = generateSine(frequency, 8000);

        LowPassFilter filter;
        filter.setFrequency(1000.0f);
        filter.prepare(kSampleRate, 64);
        filter.process(sinal);

        return rmsOfSecondHalf(sinal);
    };

    const float grave = medir(100.0);   // uma década abaixo do corte
    const float agudo = medir(10000.0); // uma década acima do corte

    // Entrada de amplitude 0.5 -> RMS ~0.3536. Uma década abaixo do corte,
    // um polo atenua ~0.04 dB (nada); uma década acima, ~20 dB (10x).
    check(grave > 0.33f, "100 Hz passa quase inteiro com corte em 1 kHz");
    check(agudo < grave / 5.0f, "10 kHz sai pelo menos 5x menor que o grave");
}

// Em DC o ganho é exatamente 1: um passa-baixa não mexe no nível, só no
// brilho — o oposto do HighPassFilter, cujo teste prova que DC morre.
void testDCPassesUntouched()
{
    std::cout << "DC passa inteiro\n";

    std::vector<float> dc(8000, 0.8f);

    LowPassFilter filter;
    filter.setFrequency(500.0f);
    filter.prepare(kSampleRate, 64);
    filter.process(dc);

    check(std::fabs(dc.back() - 0.8f) < 1e-3f, "sinal constante converge para ele mesmo");
}

// Corte mais baixo escurece mais — o parâmetro faz o que promete.
void testLowerCutoffDarkensMore()
{
    std::cout << "corte mais baixo escurece mais\n";

    auto medir = [](float cutoff) -> float
    {
        std::vector<float> sinal = generateSine(3000.0, 8000);

        LowPassFilter filter;
        filter.setFrequency(cutoff);
        filter.prepare(kSampleRate, 64);
        filter.process(sinal);

        return rmsOfSecondHalf(sinal);
    };

    check(medir(500.0f) < medir(8000.0f), "3 kHz sai menor com corte em 500 Hz que em 8 kHz");
}

// reset() apaga a memória: depois de sinal de verdade, silêncio tem que
// sair silêncio, sem resquício da média anterior.
void testResetClearsState()
{
    std::cout << "reset apaga a memoria\n";

    LowPassFilter filter;
    filter.prepare(kSampleRate, 64);

    std::vector<float> tom = generateSine(440.0, 1000);
    filter.process(tom);

    filter.reset();

    std::vector<float> silencio(100, 0.0f);
    filter.process(silencio);

    for (float sample : silencio)
    {
        checkClose(sample, 0.0f, "amostra continua 0.0 depois do reset");
    }
}

// Os getters devolvem o que os setters guardaram.
void testParameterRoundTrip()
{
    std::cout << "setter e getter\n";

    LowPassFilter filter;

    checkClose(filter.frequency(), 5000.0f, "corte padrao e 5 kHz");

    filter.setFrequency(2200.0f);
    checkClose(filter.frequency(), 2200.0f, "corte vira 2.2 kHz");
}

// Buffer vazio não pode quebrar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    std::vector<float> buffer;

    LowPassFilter filter;
    filter.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

int main()
{
    std::cout << "\n=== testes do LowPassFilter ===\n\n";

    testPassesLowsAndCutsHighs();
    testDCPassesUntouched();
    testLowerCutoffDarkensMore();
    testResetClearsState();
    testParameterRoundTrip();
    testEmptyBufferDoesNotCrash();

    return reportResults();
}
