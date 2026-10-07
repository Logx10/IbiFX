// Testes do resample().
//
// A referência é a matemática, não outro conversor: um seno amostrado a
// 44100 Hz e convertido para 48000 Hz tem que ser, amostra a amostra, o
// MESMO seno amostrado direto a 48000 Hz. As bordas ficam de fora da
// comparação — ali a sinc não tem vizinhos dos dois lados, e todo conversor
// por janela erra um pouco nas pontas.
//
// A infra de verificação vive em test_helpers.h.

#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "Resampler.h"
#include "test_helpers.h"

namespace
{
constexpr double kPi = 3.14159265358979323846;

std::vector<float> sine(double frequency, double sampleRate, std::size_t length)
{
    std::vector<float> result(length);
    for (std::size_t i = 0; i < length; ++i)
        result[i] = static_cast<float>(0.5 * std::sin(2.0 * kPi * frequency * static_cast<double>(i) / sampleRate));
    return result;
}

// Maior diferença entre os dois, ignorando `edge` amostras em cada ponta.
float maxErrorAwayFromEdges(const std::vector<float>& actual, const std::vector<float>& expected, std::size_t edge)
{
    float worst = 0.0f;
    const std::size_t length = std::min(actual.size(), expected.size());
    for (std::size_t i = edge; i + edge < length; ++i)
        worst = std::max(worst, std::fabs(actual[i] - expected[i]));
    return worst;
}
}

// ---------------------------------------------------------------------

void testSameRateIsIdentity()
{
    std::cout << "mesma taxa devolve o sinal intocado\n";

    const std::vector<float> input = {0.1f, -0.2f, 0.3f};
    check(resample(input, 48000.0, 48000.0) == input, "identico");
    check(resample({}, 44100.0, 48000.0).empty(), "vazio continua vazio");
}

void testOutputLengthFollowsTheRatio()
{
    std::cout << "tamanho da saida acompanha a razao das taxas\n";

    check(resample(std::vector<float>(441, 0.0f), 44100.0, 48000.0).size() == 480, "441 -> 480");
    check(resample(std::vector<float>(480, 0.0f), 48000.0, 44100.0).size() == 441, "480 -> 441");
    check(resample(std::vector<float>(1000, 0.0f), 96000.0, 48000.0).size() == 500, "1000 -> 500");
}

void testUpsampledSineMatchesTheSineAtTheNewRate()
{
    std::cout << "seno de 1 kHz de 44100 para 48000 Hz e o mesmo seno a 48000 Hz\n";

    const std::vector<float> converted = resample(sine(1000.0, 44100.0, 4410), 44100.0, 48000.0);
    const std::vector<float> expected = sine(1000.0, 48000.0, converted.size());

    check(maxErrorAwayFromEdges(converted, expected, 200) < 1e-3f, "erro abaixo de 0.001 longe das bordas");
}

void testDownsampledSineMatchesTheSineAtTheNewRate()
{
    std::cout << "seno de 1 kHz de 48000 para 44100 Hz e o mesmo seno a 44100 Hz\n";

    const std::vector<float> converted = resample(sine(1000.0, 48000.0, 4800), 48000.0, 44100.0);
    const std::vector<float> expected = sine(1000.0, 44100.0, converted.size());

    check(maxErrorAwayFromEdges(converted, expected, 200) < 1e-3f, "erro abaixo de 0.001 longe das bordas");
}

void testDownsamplingFiltersWhatNoLongerFits()
{
    std::cout << "descendo de 96000 para 48000 Hz, um seno de 30 kHz e filtrado, nao espelhado\n";

    // 30 kHz não existe a 48000 Hz (Nyquist = 24 kHz). Sem filtro ele
    // voltaria como um seno de 18 kHz com quase a amplitude original.
    const std::vector<float> converted = resample(sine(30000.0, 96000.0, 9600), 96000.0, 48000.0);

    float peak = 0.0f;
    for (std::size_t i = 200; i + 200 < converted.size(); ++i)
        peak = std::max(peak, std::fabs(converted[i]));

    check(peak < 0.01f, "sobra menos de 2% da amplitude original (0.5)");
}

int main()
{
    std::cout << "\n=== testes do resample ===\n\n";

    testSameRateIsIdentity();
    testOutputLengthFollowsTheRatio();
    testUpsampledSineMatchesTheSineAtTheNewRate();
    testDownsampledSineMatchesTheSineAtTheNewRate();
    testDownsamplingFiltersWhatNoLongerFits();

    return reportResults();
}
