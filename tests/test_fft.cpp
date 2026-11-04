// Testes da Fft.
//
// Casos com resposta conhecida de cabeça, e a volta completa
// inverse(forward(x)) == x.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "Fft.h"
#include "test_helpers.h"

using Complex = Fft::Complex;

// Um impulso na amostra 0 contém todas as frequências com a mesma força:
// todo bin vale 1.
void testImpulseIsFlat()
{
    std::cout << "impulso tem espectro plano\n";

    Fft fft;
    fft.prepare(16);

    std::vector<Complex> data(16, Complex(0.0f, 0.0f));
    data[0] = Complex(1.0f, 0.0f);
    fft.forward(data);

    float worst = 0.0f;
    for (const Complex& bin : data)
        worst = std::max(worst, std::abs(bin - Complex(1.0f, 0.0f)));
    check(worst < 1e-6f, "todos os 16 bins valem 1");
}

// Um cosseno que dá exatamente 3 voltas em N amostras cai inteiro nos bins
// 3 e N-3 (a frequência positiva e a negativa), N/2 em cada; o resto é 0.
void testCosineLandsOnItsBin()
{
    std::cout << "cosseno cai no proprio bin\n";

    constexpr std::size_t kSize = 64;
    constexpr double kTwoPi = 6.283185307179586;

    Fft fft;
    fft.prepare(kSize);

    std::vector<Complex> data(kSize);
    for (std::size_t n = 0; n < kSize; ++n)
        data[n] = Complex(static_cast<float>(std::cos(kTwoPi * 3.0 * static_cast<double>(n) / kSize)), 0.0f);
    fft.forward(data);

    check(std::fabs(std::abs(data[3]) - 32.0f) < 1e-4f, "bin 3 vale N/2 = 32");
    check(std::fabs(std::abs(data[kSize - 3]) - 32.0f) < 1e-4f, "bin N-3 vale N/2 = 32");

    float leak = 0.0f;
    for (std::size_t k = 0; k < kSize; ++k)
    {
        if (k != 3 && k != kSize - 3)
            leak = std::max(leak, std::abs(data[k]));
    }
    check(leak < 1e-4f, "os outros bins ficam em zero");
}

// inverse(forward(x)) devolve x — a inversa já divide por N.
void testRoundTrip()
{
    std::cout << "ida e volta devolve o sinal\n";

    Fft fft;
    fft.prepare(256);

    std::vector<Complex> original(256);
    for (std::size_t n = 0; n < original.size(); ++n)
        original[n] = Complex(std::sin(0.37f * static_cast<float>(n)), 0.25f * std::cos(1.3f * static_cast<float>(n)));

    std::vector<Complex> data = original;
    fft.forward(data);
    fft.inverse(data);

    float worst = 0.0f;
    for (std::size_t n = 0; n < data.size(); ++n)
        worst = std::max(worst, std::abs(data[n] - original[n]));
    check(worst < 1e-5f, "diferenca maxima abaixo de 1e-5");
}

void testSizeMustBePowerOfTwo()
{
    std::cout << "tamanho que nao e potencia de 2 lanca\n";

    bool lancou = false;
    try
    {
        Fft fft;
        fft.prepare(100);
    }
    catch (const std::invalid_argument&)
    {
        lancou = true;
    }
    check(lancou, "prepare(100) lancou");
}

int main()
{
    std::cout << "\n=== testes da Fft ===\n\n";

    testImpulseIsFlat();
    testCosineLandsOnItsBin();
    testRoundTrip();
    testSizeMustBePowerOfTwo();

    return reportResults();
}
