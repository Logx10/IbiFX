#include "IronheartInputStage.h"
#include "test_helpers.h"

#include <algorithm>
#include <complex>
#include <limits>
#include <stdexcept>

namespace
{
constexpr double kPi = 3.14159265358979323846;

// Oráculo independente: associa as impedâncias do esquema, sem repetir
// a transformação bilinear nem os coeficientes usados pela implementação.
double analogGainDb(double frequency, double pot, bool enabled)
{
    const std::complex<double> s(0.0, 2.0 * kPi * frequency);
    const auto load = 1.0 / (1.0 / 1e6 + s * 47e-12);
    const auto input = load / (10e3 + 1.0 / (s * 220e-9) + load);
    std::complex<double> boost = 1.0;
    if (enabled && pot > 0.0)
    {
        const auto feedback = 1.0 / (1.0 / 100e3 + 1.0 / pot + s * 220e-12);
        const auto ground = 4.7e3 + 1.0 / (s * 10e-6);
        boost += feedback / ground;
    }
    const auto grid = 1e6 / (1e6 + 1.0 / (s * 10e-9));
    return 20.0 * std::log10(std::abs(input * boost * grid));
}

double measuredGainDb(double rate, double frequency, float pot, bool enabled)
{
    IronheartInputStage stage;
    stage.prepare(rate, pot, enabled);
    // Ajuste de seno/cosseno por mínimos quadrados: não exige que a janela
    // contenha um número inteiro de ciclos (por exemplo, Mi grave: 82,41 Hz).
    double ss = 0.0, cc = 0.0, sc = 0.0, ys = 0.0, yc = 0.0;
    const int settle = static_cast<int>(rate);
    const int count = settle + static_cast<int>(rate / 2.0);
    for (int n = 0; n < count; ++n)
    {
        const double angle = 2.0 * kPi * frequency * n / rate;
        const double sine = std::sin(angle);
        const double output = stage.processSample(sine);
        if (n < settle) continue;
        const double cosine = std::cos(angle);
        ss += sine * sine;
        cc += cosine * cosine;
        sc += sine * cosine;
        ys += output * sine;
        yc += output * cosine;
    }
    const double determinant = ss * cc - sc * sc;
    const double real = (ys * cc - yc * sc) / determinant;
    const double imaginary = (yc * ss - ys * sc) / determinant;
    return 20.0 * std::log10(std::hypot(real, imaginary));
}

void testResponse()
{
    double maxWarpedError = 0.0;
    double maxAnalogError = 0.0;
    for (double rate : {176400.0, 192000.0, 384000.0})
    {
        for (float pot : {0.0f, 10000.0f, 100000.0f})
        {
            bool matchesDigital = true, matchesAnalog = true;
            for (double frequency : {20.0, 82.41, 220.0, 1000.0, 5000.0, 10000.0, 20000.0})
            {
                const double actual = measuredGainDb(rate, frequency, pot, true);
                const double warped = rate / kPi * std::tan(kPi * frequency / rate);
                const double digitalError = std::fabs(actual - analogGainDb(warped, pot, true));
                const double analogError = std::fabs(actual - analogGainDb(frequency, pot, true));
                maxWarpedError = std::max(maxWarpedError, digitalError);
                maxAnalogError = std::max(maxAnalogError, analogError);
                matchesDigital &= digitalError < 0.01;
                matchesAnalog &= analogError < 0.30;
            }
            const auto label = " fs=" + std::to_string(rate) + " Rpot=" + std::to_string(pot);
            check(matchesDigital, "resposta coincide com circuito bilinear" + label);
            check(matchesAnalog, "desvio analogico menor que 0,30 dB ate 20 kHz" + label);
        }
    }
    const double maximum = measuredGainDb(192000.0, 1000.0, 100000.0f, true);
    check(std::fabs(maximum - 21.207971) < 0.01, "rede completa: aproximadamente +21,21 dB a 1 kHz no maximo");
    std::cout << "  ganho maximo a 1 kHz: " << maximum << " dB; erro bilinear: "
              << maxWarpedError << " dB; erro analogico: " << maxAnalogError << " dB\n";
}

void testLimitsAndReset()
{
    IronheartInputStage off, zero, clampedLow, clampedHigh, maximum;
    off.prepare(192000.0, 100000.0f, false);
    zero.prepare(192000.0, 0.0f, true);
    clampedLow.prepare(192000.0, -20.0f, true);
    clampedHigh.prepare(192000.0, 200000.0f, true);
    maximum.prepare(192000.0, 100000.0f, true);
    bool equal = true;
    for (int n = 0; n < 40000; ++n)
    {
        const double input = std::sin(2.0 * kPi * 220.0 * n / 192000.0);
        const double dry = off.processSample(input);
        equal &= dry == zero.processSample(input) && dry == clampedLow.processSample(input);
        equal &= maximum.processSample(input) == clampedHigh.processSample(input);
    }
    check(equal, "boost off equivale a Rpot zero; resistencia limitada a 0..100 kohm");
    maximum.reset();
    bool silent = true;
    for (int n = 0; n < 2000; ++n) silent &= maximum.processSample(0.0) == 0.0;
    check(silent, "reset elimina toda a memoria do circuito");
    double dc = 0.0;
    for (int n = 0; n < 192000 * 5; ++n) dc = maximum.processSample(1.0);
    check(std::fabs(dc) < 1e-7, "capacitores bloqueiam DC apos assentamento");
    maximum.setResistance(std::numeric_limits<float>::quiet_NaN());
    maximum.reset();
    zero.reset();
    equal = true;
    for (int n = 0; n < 2000; ++n)
        equal &= maximum.processSample(n == 0 ? 1.0 : 0.0) == zero.processSample(n == 0 ? 1.0 : 0.0);
    check(equal, "resistencia nao finita tratada como zero");
    bool rejects = true;
    for (double rate : {0.0, -1.0, std::numeric_limits<double>::infinity()})
    {
        bool threw = false;
        try { maximum.prepare(rate, 0.0f, false); }
        catch (const std::invalid_argument&) { threw = true; }
        rejects &= threw;
    }
    check(rejects, "prepare rejeita taxas invalidas fora do callback");
    maximum.prepare(176400.0, 0.0f, false);
    check(maximum.processSample(0.0) == 0.0, "prepare pode ser repetido em outra taxa");
}

void testLiveControls()
{
    IronheartInputStage stage;
    stage.prepare(192000.0, 100000.0f, false);
    double previous = 0.0, maxJump = 0.0, maxOutput = 0.0;
    bool finite = true;
    for (int n = 0; n < 192000; ++n)
    {
        if (n == 10000) stage.setEnabled(true);
        if (n == 30000) stage.setResistance(0.0f);
        if (n == 50000) stage.setResistance(100000.0f);
        if (n == 70000) stage.setEnabled(false);
        if (n > 90000 && n % 701 == 0)
        {
            stage.setResistance(n % 2 == 0 ? 0.0f : 100000.0f);
            stage.setEnabled(n % 3 != 0);
        }
        const double output = stage.processSample(0.001 * std::sin(2.0 * kPi * 220.0 * n / 192000.0));
        finite &= std::isfinite(output);
        maxJump = std::max(maxJump, std::fabs(output - previous));
        maxOutput = std::max(maxOutput, std::fabs(output));
        previous = output;
    }
    check(finite && maxOutput < 0.02, "automacao rapida permanece finita e limitada");
    check(maxJump < 0.0003, "rampas evitam degrau de ganho ao chavear ou girar o boost");

    // O ramo de terra acompanha o sinal com boost off e a capacitancia de
    // feedback descarrega: religar depois de silencio nao revive uma cauda.
    stage.setEnabled(false);
    for (int n = 0; n < 192000 * 5; ++n) stage.processSample(0.0);
    stage.setEnabled(true);
    double tail = 0.0;
    for (int n = 0; n < 10000; ++n) tail = std::max(tail, std::fabs(stage.processSample(0.0)));
    check(tail < 1e-10, "reativar boost apos silencio nao recupera estados congelados");
}
}

int main()
{
    testResponse();
    testLimitsAndReset();
    testLiveControls();
    return reportResults();
}
