#include "IronheartCathodeFollower.h"
#include "IronheartToneNetwork.h"
#include "IronheartChannelNetwork.h"
#include "IronheartGainStages.h"
#include "test_helpers.h"

#include <algorithm>
#include <array>
#include <complex>
#include <limits>
#include <stdexcept>

namespace
{
using C = std::complex<double>;
constexpr double kPi = 3.14159265358979323846;
struct Settings { float bass = .5f, mid = .5f, treble = .5f; bool deep = false, midShift = false, trebleShift = false; };
struct Reference { C output, load; };

// Oráculo analógico: impedâncias complexas e Gauss-Jordan com pivotamento.
// Não utiliza Cholesky, companheiros trapezoidais nem estados de produção.
Reference reference(double frequency, Settings p)
{
    const C s(0, 2 * kPi * frequency);
    std::array<std::array<C, 9>, 8> a{};
    // Feed, Snubber, Slope, TrebleTop, BassTop, MidTop, MidWiper, Output.
    const auto stamp = [&a](int from, int to, C y) {
        if (from >= 0) a[from][from] += y;
        if (to >= 0) a[to][to] += y;
        if (from >= 0 && to >= 0) { a[from][to] -= y; a[to][from] -= y; }
    };
    a[0][0] += 1.0 / 47e3; a[0][8] += 1.0 / 47e3; // fonte ideal de 1 V em R39
    stamp(0, 1, 1.0 / 47e3); stamp(1, -1, s * 10e-9);
    stamp(0, 2, 1.0 / 47e3); stamp(0, 3, s * 470e-12);
    const auto switched = [s](double cap, bool on) {
        const double resistance = on ? 1.0 / (1.0 + 1.0 / 10e6) : 10e6;
        return 1.0 / (resistance + 1.0 / (s * cap));
    };
    stamp(0, 3, switched(470e-12, p.trebleShift));
    stamp(2, 4, s * 22e-9); stamp(2, 4, switched(100e-9, p.deep));
    stamp(2, 6, s * 22e-9); stamp(2, 6, switched(10e-9, p.midShift));
    const double bassR = 250e3 * (std::pow(81.0, p.bass) - 1.0) / 80.0;
    stamp(4, 5, 1.0 / std::max(1.0, bassR));
    stamp(5, 6, 1.0 / std::max(1.0, 25e3 * (1.0 - p.mid)));
    stamp(6, -1, 1.0 / std::max(1.0, 25e3 * p.mid));
    stamp(3, 7, 1.0 / std::max(1.0, 250e3 * (1.0 - p.treble)));
    stamp(7, 4, 1.0 / std::max(1.0, 250e3 * p.treble));
    stamp(7, -1, 1.0 / 1e6);
    for (int col = 0; col < 8; ++col)
    {
        int pivot = col;
        for (int row = col + 1; row < 8; ++row)
            if (std::abs(a[row][col]) > std::abs(a[pivot][col])) pivot = row;
        std::swap(a[col], a[pivot]);
        const auto divisor = a[col][col];
        for (int k = col; k < 9; ++k) a[col][k] /= divisor;
        for (int row = 0; row < 8; ++row)
        {
            if (row == col) continue;
            const auto factor = a[row][col];
            for (int k = col; k < 9; ++k) a[row][k] -= factor * a[col][k];
        }
    }
    return {a[7][8], (1.0 - a[0][8]) / 47e3};
}

double current(double vp, double vg)
{
    if (vp <= 0) return 0;
    const long double x = 600.0L * (.01L + vg / std::sqrt(300.0L + static_cast<long double>(vp) * vp));
    const auto softplus = x > 0 ? x + std::log1p(std::exp(-x)) : std::log1p(std::exp(x));
    return static_cast<double>(2.0L / 1060 * std::pow(vp / 600 * softplus, 1.4L));
}

double biasCathode(double grid)
{
    double low = 0, high = 410;
    for (int n = 0; n < 100; ++n)
    {
        const double vk = (low + high) / 2;
        if (vk / 100e3 > current(410 - vk, grid - vk)) high = vk;
        else low = vk;
    }
    return (low + high) / 2;
}

std::array<double, 2> derivatives(double grid)
{
    const double vk = biasCathode(grid), vp = 410 - vk, vg = grid - vk;
    constexpr double h = 1e-4;
    return {(current(vp, vg + h) - current(vp, vg - h)) / (2 * h),
            (current(vp + h, vg) - current(vp - h, vg)) / (2 * h)};
}

Reference measure(double rate, double frequency, Settings p, bool coupled = false)
{
    IronheartToneNetwork tone;
    tone.prepare(rate, p.bass, p.mid, p.treble, p.deep, p.midShift, p.trebleShift);
    IronheartCathodeFollower follower;
    follower.prepare(139.6);
    const double amplitude = coupled ? 1e-4 : 1.0;
    double ss = 0, cc = 0, sc = 0;
    std::array<double, 2> ys{}, yc{};
    const int settle = static_cast<int>(rate / 2);
    for (int n = 0; n < settle + static_cast<int>(rate / 4); ++n)
    {
        const double phase = 2 * kPi * frequency * n / rate;
        const double sine = std::sin(phase), cosine = std::cos(phase);
        const auto load = tone.beginSample();
        const double cathode = coupled ? follower.processSample(amplitude * sine, load) : amplitude * sine;
        const double output = tone.finishSample(cathode);
        if (n < settle) continue;
        ss += sine * sine; cc += cosine * cosine; sc += sine * cosine;
        const std::array<double, 2> values{output / amplitude,
            (load.conductance * cathode - load.historyAmps) / amplitude};
        for (int k = 0; k < 2; ++k) { ys[k] += values[k] * sine; yc[k] += values[k] * cosine; }
    }
    std::array<C, 2> result{};
    for (int k = 0; k < 2; ++k)
        result[k] = {(ys[k] * cc - yc[k] * sc) / (ss * cc - sc * sc),
                     (yc[k] * ss - ys[k] * sc) / (ss * cc - sc * sc)};
    return {result[0], result[1]};
}

void testToneResponse()
{
    double maxError = 0;
    for (int switches = 0; switches < 8; ++switches)
    {
        Settings p; p.deep = (switches & 1) != 0; p.midShift = (switches & 2) != 0; p.trebleShift = (switches & 4) != 0;
        bool valid = true;
        for (double f : {20.0, 82.41, 1000.0, 5000.0, 20000.0})
        {
            const auto wanted = reference(192000 / kPi * std::tan(kPi * f / 192000), p);
            const auto got = measure(192000, f, p);
            const double error = std::max(std::abs(got.output - wanted.output) / std::abs(wanted.output),
                                          std::abs(got.load - wanted.load) / std::abs(wanted.load));
            maxError = std::max(error, maxError);
            valid &= error < 1e-4 && got.load.real() > 0;
        }
        check(valid, "ganho/fase/carga concordam com oraculo, pull mask=" + std::to_string(switches));
    }
    for (double rate : {88200.0, 176400.0, 384000.0})
        for (const auto p : {Settings{0, 0, 0}, Settings{1, 1, 1}, Settings{.2f, .8f, .7f, true, false, true}})
        {
            const auto got = measure(rate, 1000, p);
            const auto wanted = reference(rate / kPi * std::tan(kPi * 1000 / rate), p);
            check(std::abs(got.output - wanted.output) / std::abs(wanted.output) < 1e-4,
                  "extremos e outra taxa interna: " + std::to_string(rate));
        }
    std::cout << "  maior erro relativo de tone/carga: " << maxError
              << "; insercao a 220 Hz=" << 20 * std::log10(std::abs(reference(220, {}).output)) << " dB\n";
}

void testFollower()
{
    IronheartCathodeFollower follower;
    follower.prepare(139.6);
    const auto point = follower.biasPoint();
    check(point.converged && std::fabs(point.cathodeVolts - biasCathode(139.6)) < 1e-7,
          "V3 coincide com repouso por bissecao independente");
    check(std::fabs(point.plateCurrentAmps - point.cathodeVolts / 100e3) < 1e-12,
          "R100k determina corrente de catodo no repouso");
    const auto d = derivatives(139.6);
    const double expected = d[0] / (d[0] + d[1] + 1.0 / 100e3);
    check(std::fabs(follower.processSample(.001, {}) / .001 - expected) < 1e-5 && expected > .95 && expected < 1,
          "seguidor tem ganho positivo proximo de um sem inversao");
    follower.reset(); check(follower.processSample(0, {}) == 0, "reset V3 sem pop");
    std::cout << "  V3: Vk=" << point.cathodeVolts << " V Ia=" << point.plateCurrentAmps * 1000 << " mA ganho=" << expected << '\n';
    for (double rate : {88200.0, 192000.0, 384000.0})
    {
        const Settings p{.3f, .7f, .6f, true, true, true};
        const auto wantedTone = reference(rate / kPi * std::tan(kPi * 1000 / rate), p);
        const auto wanted = wantedTone.output * d[0] / (d[0] + d[1] + 1.0 / 100e3 + wantedTone.load);
        const auto got = measure(rate, 1000, p, true);
        check(std::abs(got.output - wanted) / std::abs(wanted) < 1e-4,
              "V3 e tone resolvidos juntos, taxa=" + std::to_string(rate));
    }
}

void testControlDirections()
{
    Settings low, high;
    low.bass = .1f; high.bass = .9f;
    const double bass = std::abs(reference(80, high).output / reference(80, low).output)
        / std::abs(reference(5000, high).output / reference(5000, low).output);
    low = {}; high = {}; low.treble = .1f; high.treble = .9f;
    const double treble = std::abs(reference(5000, high).output / reference(5000, low).output)
        / std::abs(reference(80, high).output / reference(80, low).output);
    low = {}; high = {}; low.mid = .1f; high.mid = .9f;
    const double mid = std::abs(reference(700, high).output / reference(700, low).output);
    high = {}; high.deep = true;
    const double deep = std::abs(reference(80, high).output / reference(80, {}).output);
    check(bass > 1 && treble > 1 && mid > 1 && deep > 1,
          "sentido de Bass/Mid/Treble e extensao grave do Deep coerentes");
    const auto notch = [](bool shifted) {
        Settings p; p.midShift = shifted;
        double best = std::numeric_limits<double>::max(), at = 0;
        for (int n = 0; n <= 400; ++n)
        {
            const double frequency = 100 * std::pow(30.0, n / 400.0);
            const double level = std::abs(reference(frequency, p).output);
            if (level < best) { best = level; at = frequency; }
        }
        return at;
    };
    const double normal = notch(false), shifted = notch(true);
    check(shifted < normal, "Mid Shift desloca a regiao de atenuacao para baixo");
    std::cout << "  direcoes: bass=" << bass << " mid=" << mid << " treble=" << treble
              << " deep=" << deep << "; notch=" << normal << " -> " << shifted << " Hz\n";
}

void testAutomationAndLimits()
{
    IronheartCathodeFollower follower;
    IronheartToneNetwork tone;
    check(follower.processSample(1, {}) == 0 && tone.finishSample(1) == 0,
          "componentes nao preparados retornam silencio");
    tone.prepare(192000, .5f, .5f, .5f, false, false, false);
    follower.prepare(139.6);
    bool valid = true;
    double residual = 0;
    for (int n = 0; n < 40000; ++n)
    {
        if (n % 997 == 0)
            tone.setControls(n % 2 ? 0 : 1, n % 3 ? 1 : 0, n % 2 ? 1 : 0,
                             n % 2 == 0, n % 3 == 0, n % 5 == 0);
        const double input = 180 * std::sin(2 * kPi * 110 * n / 192000);
        const auto load = tone.beginSample();
        const double cathode = follower.processSample(input, load);
        const double out = tone.finishSample(cathode);
        const auto point = follower.operatingPoint();
        const double model = current(410 - point.cathodeVolts, 139.6 + input - point.cathodeVolts);
        residual = std::max(residual, std::fabs(point.cathodeVolts / 100e3
            + load.conductance * cathode - load.historyAmps - model));
        valid &= point.converged && std::isfinite(out) && std::fabs(out) < 1000 && load.conductance > 0;
    }
    check(valid && residual < 1e-10, "KCL de V3 sob corte/saturacao, pulls e pots moveis");
    tone.reset(); follower.reset();
    bool silent = true;
    for (int n = 0; n < 1000; ++n)
    { const auto load = tone.beginSample(); silent &= tone.finishSample(follower.processSample(0, load)) == 0; }
    check(silent, "reset conjunto limpa memorias de seguidor e tone");
    tone.setControls(std::numeric_limits<float>::quiet_NaN(), -2, 3, false, false, false);
    tone.reset();
    const auto load = tone.beginSample();
    check(std::isfinite(tone.finishSample(follower.processSample(0, load))), "pots invalidos sao limitados");
    bool rejectedRate = false, rejectedBias = false;
    try { tone.prepare(0, 0, 0, 0, false, false, false); } catch (const std::invalid_argument&) { rejectedRate = true; }
    try { follower.prepare(std::numeric_limits<double>::infinity()); } catch (const std::invalid_argument&) { rejectedBias = true; }
    check(rejectedRate && rejectedBias, "preparacao valida taxa e polarizacao fora do callback");
}

void testFullPathConvergence()
{
    IronheartTriodeStage v1a;
    IronheartChannelNetwork channels;
    IronheartGainStages gains;
    IronheartCathodeFollower follower;
    IronheartToneNetwork tone;
    v1a.prepare(192000);
    gains.prepare(192000, false);
    channels.prepare(192000, IronheartChannelNetwork::Channel::Lead, .5f, .5f, .5f);
    follower.prepare(gains.v2b().biasPoint().plateVolts * 620 / 1090);
    tone.prepare(192000, .5f, .5f, .5f, false, false, false);
    int failures = 0;
    for (int n = 0; n < 12000; ++n)
    {
        const auto inputLoad = channels.beginSample();
        const double plate = v1a.processSample(2 * std::sin(2 * kPi * 220 * n / 192000), inputLoad);
        const auto out = channels.finishSample(plate);
        const double grid = gains.processSample(out.grid);
        const auto load = tone.beginSample();
        tone.finishSample(follower.processSample(grid, load));
        failures += !follower.operatingPoint().converged;
    }
    check(failures == 0, "V3 converge sem reter amostras na forma de onda da cadeia high gain completa");
}
}

int main()
{
    testToneResponse(); testFollower(); testControlDirections(); testAutomationAndLimits(); testFullPathConvergence();
    return reportResults();
}
