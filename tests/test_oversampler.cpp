// Testes do Oversampler.
//
// A MEDIDA: QUANTO DA ENERGIA NÃO É HARMÔNICO
// Um seno distorcido só deveria ter energia nos múltiplos da sua frequência
// (os harmônicos). Tudo que aparecer em outra frequência é aliasing — o
// "fizz" que o Oversampler existe para tirar. A frequência do seno é
// escolhida para caber um número inteiro de períodos na janela de análise
// (sem vazamento espectral) E para 48000 não ser múltiplo dela — senão os
// harmônicos espelhados cairiam exatamente em cima dos verdadeiros, e a
// medida não os enxergaria.
//
// A infra de verificação vive em test_helpers.h.

#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "Oversampler.h"
#include "SoftClipper.h"
#include "test_helpers.h"

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;

// Janela de análise e o trecho inicial descartado (o filtro enchendo).
constexpr std::size_t kWindow = 4800;
constexpr std::size_t kSkip = 960;

std::vector<float> sine(double frequency, std::size_t length, float amplitude)
{
    std::vector<float> result(length);
    for (std::size_t i = 0; i < length; ++i)
        result[i] = amplitude * static_cast<float>(std::sin(2.0 * kPi * frequency * static_cast<double>(i) / kSampleRate));
    return result;
}

// Energia fora dos harmônicos, em dB relativos à energia total. bin0 é o
// bin da fundamental na janela de kWindow amostras.
double aliasingDb(const std::vector<float>& signal, std::size_t bin0)
{
    double total = 0.0;
    double aliasing = 0.0;

    for (std::size_t bin = 1; bin <= kWindow / 2; ++bin)
    {
        double real = 0.0;
        double imaginary = 0.0;
        for (std::size_t n = 0; n < kWindow; ++n)
        {
            const double phase = 2.0 * kPi * static_cast<double>(bin * n) / static_cast<double>(kWindow);
            real += signal[kSkip + n] * std::cos(phase);
            imaginary -= signal[kSkip + n] * std::sin(phase);
        }

        const double power = real * real + imaginary * imaginary;
        total += power;
        if (bin % bin0 != 0)
            aliasing += power;
    }

    return 10.0 * std::log10(aliasing / total);
}

double rms(const std::vector<float>& signal)
{
    double sum = 0.0;
    for (std::size_t n = 0; n < kWindow; ++n)
        sum += static_cast<double>(signal[kSkip + n]) * signal[kSkip + n];
    return std::sqrt(sum / static_cast<double>(kWindow));
}
}

// ---------------------------------------------------------------------

// Com uma "curva" que não faz nada, o que entra sai — só atrasado. O volume
// de um seno na faixa audível tem que continuar o mesmo.
void testIdentityCurvePreservesAudibleSignal()
{
    std::cout << "curva identidade: seno audivel passa com o mesmo volume\n";

    for (double frequency : {100.0, 1010.0, 10000.0})
    {
        const std::vector<float> input = sine(frequency, kWindow + kSkip, 0.5f);
        std::vector<float> output(input.size());

        Oversampler oversampler;
        for (std::size_t i = 0; i < input.size(); ++i)
            output[i] = oversampler.process(input[i], [](float x) { return x; });

        const double differenceDb = 20.0 * std::log10(rms(output) / rms(input));
        check(std::fabs(differenceDb) < 0.05,
              std::to_string(static_cast<int>(frequency)) + " Hz com o mesmo volume (+-0.05 dB)");
    }
}

// O motivo de existir: saturar com força um seno de 2,5 kHz sem
// oversampling joga uns -33 dB da energia em frequências que não são
// harmônicos (fizz audível); com ele, isso cai para perto de -115 dB.
void testSaturationAliasingDropsBelowAudibility()
{
    std::cout << "tanh forte num seno de 2,5 kHz: aliasing cai abaixo do audivel\n";

    const std::size_t bin0 = 250;  // 2500 Hz numa janela de 4800 a 48 kHz
    const std::vector<float> input = sine(2500.0, kWindow + kSkip, 0.5f);
    const auto curve = [](float x) { return std::tanh(10.0f * x); };

    std::vector<float> direct(input.size());
    std::vector<float> oversampled(input.size());
    Oversampler oversampler;

    for (std::size_t i = 0; i < input.size(); ++i)
    {
        direct[i] = curve(input[i]);
        oversampled[i] = oversampler.process(input[i], curve);
    }

    const double withoutDb = aliasingDb(direct, bin0);
    const double withDb = aliasingDb(oversampled, bin0);

    std::cout << "    sem oversampling: " << withoutDb << " dB, com: " << withDb << " dB\n";

    check(withoutDb > -40.0, "sem oversampling o aliasing e alto (o problema existe)");
    check(withDb < -90.0, "com oversampling fica abaixo de -90 dB");
}

// Os módulos de distorção já nascem com oversampling ligado — é o caso de
// uso real, montado por preset ou pela GearLibrary.
void testDistortionModulesOversampleByDefault()
{
    std::cout << "SoftClipper ja nasce sobreamostrado\n";

    SoftClipper softClipper;
    check(softClipper.oversampling(), "oversampling ligado por padrao");

    softClipper.prepare(kSampleRate, 128);
    softClipper.setDrive(10.0f);
    softClipper.reset();

    std::vector<float> buffer = sine(1010.0, kWindow + kSkip, 0.5f);
    softClipper.process(buffer);

    check(aliasingDb(buffer, 101) < -90.0, "seno de 1010 Hz saturado sai sem aliasing (< -90 dB)");
}

void testResetClearsHistory()
{
    std::cout << "reset() esquece o sinal anterior\n";

    Oversampler oversampler;
    for (int i = 0; i < 100; ++i)
        oversampler.process(1.0f, [](float x) { return x; });

    oversampler.reset();

    float worst = 0.0f;
    for (int i = 0; i < 100; ++i)
        worst = std::max(worst, std::fabs(oversampler.process(0.0f, [](float x) { return x; })));

    check(worst == 0.0f, "silencio depois do reset e silencio");
}

int main()
{
    std::cout << "\n=== testes do Oversampler ===\n\n";

    testIdentityCurvePreservesAudibleSignal();
    testSaturationAliasingDropsBelowAudibility();
    testDistortionModulesOversampleByDefault();
    testResetClearsHistory();

    return reportResults();
}
