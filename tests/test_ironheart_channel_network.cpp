#include "IronheartChannelNetwork.h"
#include "test_helpers.h"

#include <algorithm>
#include <array>
#include <complex>
#include <limits>
#include <stdexcept>

namespace
{
using Channel = IronheartChannelNetwork::Channel;
using Complex = std::complex<double>;
constexpr double kPi = 3.14159265358979323846;
Complex parallel(Complex a, Complex b) { return a * b / (a + b); }
double taper(double position) { return (std::pow(81.0, position) - 1.0) / 80.0; }

// Oráculo por divisores/impedâncias; não usa matriz, Cholesky ou estados
// trapezoidais da implementação. Retorna Lead, Rhythm, Clean, Y da placa e grade V1B.
std::array<Complex, 5> reference(double frequency, Channel channel,
                               double lead, double rhythm, double clean)
{
    const Complex s(0.0, 2 * kPi * frequency);
    const Complex coupling = 1.0 / (s * 1.1e-9), gridCap = 1.0 / (s * 100e-9);
    const double leadLower = std::max(1.0, 220e3 * taper(lead));
    const double leadUpper = std::max(1.0, 220e3 * (1.0 - taper(lead)));
    const auto busLoad = parallel(1e6, gridCap + 1e6);
    const auto leadBottom = channel == Channel::Lead ? parallel(leadLower, 1.0 + busLoad) : Complex(leadLower);
    const auto leadTop = parallel(150e3, leadUpper + leadBottom);
    const auto leadBranch = coupling + 150e3 + leadTop;
    const auto leadGrid = channel == Channel::Lead
        ? leadTop / leadBranch * leadBottom / (leadUpper + leadBottom)
          * busLoad / (1.0 + busLoad) * 1e6 / (gridCap + 1e6) : Complex(0.0);

    const double rhythmLower = std::max(1.0, 220e3 * taper(rhythm));
    const double rhythmUpper = std::max(1.0, 220e3 * (1.0 - taper(rhythm)));
    const auto rhythmBottom = channel == Channel::Rhythm ? parallel(rhythmLower, 1.0 + busLoad) : Complex(rhythmLower);
    const auto rhythmTop = parallel(150e3, rhythmUpper + rhythmBottom);
    const auto rhythmBranch = coupling + 150e3 + rhythmTop;

    const double cleanLower = std::max(1.0, 220e3 * taper(clean));
    const double cleanUpperR = std::max(1.0, 220e3 * (1.0 - taper(clean)));
    const auto cleanUpper = parallel(cleanUpperR, 1.0 / (s * 470e-12));
    const auto cleanBottom = channel == Channel::Clean ? parallel(cleanLower, 1.0 + busLoad) : Complex(cleanLower);
    const auto cleanTop = parallel(4.7e3, cleanUpper + cleanBottom);
    const auto cleanBranch = 220e3 + cleanTop;
    const auto ch1Load = parallel(rhythmBranch, cleanBranch);
    const auto ch1Branch = 1.0 / (s * 22e-9) + ch1Load;
    const auto ch1 = ch1Load / ch1Branch;
    const auto rhythmOut = ch1 * rhythmTop / rhythmBranch * rhythmBottom / (rhythmUpper + rhythmBottom);
    const auto cleanOut = ch1 * cleanTop / cleanBranch * cleanBottom / (cleanUpper + cleanBottom);
    const auto selected = channel == Channel::Clean ? cleanOut : rhythmOut;
    const auto sharedGrid = channel == Channel::Lead ? leadGrid
        : selected * busLoad / (1.0 + busLoad) * 1e6 / (gridCap + 1e6);
    return {leadGrid, rhythmOut, cleanOut, 1.0 / leadBranch + 1.0 / ch1Branch, sharedGrid};
}

std::array<Complex, 5> measure(double rate, double frequency, Channel channel,
                             float lead = 0.5f, float rhythm = 0.5f, float clean = 0.5f)
{
    IronheartChannelNetwork network;
    network.prepare(rate, channel, lead, rhythm, clean);
    std::array<double, 5> ys{}, yc{};
    double ss = 0.0, cc = 0.0, sc = 0.0;
    const int settle = static_cast<int>(rate);
    for (int n = 0; n < settle + static_cast<int>(rate / 2); ++n)
    {
        const double phase = 2 * kPi * frequency * n / rate;
        const double sine = std::sin(phase), cosine = std::cos(phase);
        const auto load = network.beginSample();
        const auto out = network.finishSample(sine);
        const std::array<double, 5> values{out.lead, out.rhythm, out.clean,
                                         load.conductance * sine - load.historyAmps, out.grid};
        if (n < settle) continue;
        ss += sine * sine; cc += cosine * cosine; sc += sine * cosine;
        for (std::size_t k = 0; k < values.size(); ++k)
        { ys[k] += values[k] * sine; yc[k] += values[k] * cosine; }
    }
    std::array<Complex, 5> result{};
    for (std::size_t k = 0; k < result.size(); ++k)
        result[k] = {(ys[k] * cc - yc[k] * sc) / (ss * cc - sc * sc),
                     (yc[k] * ss - ys[k] * sc) / (ss * cc - sc * sc)};
    return result;
}

void testReference()
{
    double maxRelativeError = 0.0;
    for (Channel channel : {Channel::Clean, Channel::Rhythm, Channel::Lead})
    {
        bool matches = true;
        for (double frequency : {20.0, 82.41, 220.0, 1000.0, 5000.0, 20000.0})
        {
            const double rate = 192000.0;
            const auto actual = measure(rate, frequency, channel);
            const auto expected = reference(rate / kPi * std::tan(kPi * frequency / rate), channel, 0.5, 0.5, 0.5);
            for (std::size_t k = 0; k < actual.size(); ++k)
            {
                const double error = std::abs(actual[k] - expected[k]) / std::max(std::abs(expected[k]), 1e-12);
                matches &= error < 1e-5;
                maxRelativeError = std::max(maxRelativeError, error);
            }
        }
        check(matches, "tres saidas e carga da placa concordam com impedancias: modo "
                       + std::to_string(static_cast<int>(channel)));
    }
    for (double rate : {88200.0, 176400.0, 384000.0})
    {
        const auto actual = measure(rate, 1000.0, Channel::Lead, 0.8f, 0.3f, 0.7f);
        const auto expected = reference(rate / kPi * std::tan(kPi * 1000.0 / rate), Channel::Lead, 0.8f, 0.3f, 0.7f);
        bool matches = true;
        for (std::size_t k = 0; k < actual.size(); ++k)
            matches &= std::abs(actual[k] - expected[k]) < 1e-5 * std::max(std::abs(expected[k]), 1e-12);
        check(matches, "outras posicoes e taxa interna: " + std::to_string(rate));
    }
    std::cout << "  maior erro relativo de ganho/fase/carga: " << maxRelativeError << '\n';
}

void testBrightAndParallelLoading()
{
    const auto lowQuiet = reference(82.41, Channel::Clean, 0.5, 0.5, 0.2);
    const auto highQuiet = reference(5000, Channel::Clean, 0.5, 0.5, 0.2);
    const auto lowLoud = reference(82.41, Channel::Clean, 0.5, 0.5, 0.8);
    const auto highLoud = reference(5000, Channel::Clean, 0.5, 0.5, 0.8);
    check(std::abs(highQuiet[2] / lowQuiet[2]) > 1.5 * std::abs(highLoud[2] / lowLoud[2]),
          "bright contorna parte do Clean Volume e atua mais em volume baixo");
    const auto a = measure(192000, 5000, Channel::Lead, 0.5f, 0.5f, 0.2f);
    const auto b = measure(192000, 5000, Channel::Lead, 0.5f, 0.5f, 0.8f);
    check(std::abs(a[3] - b[3]) > 1e-11,
          "ramo Clean continua carregando V1A enquanto Lead esta selecionado");
}

void testCoupledTriodeAndAutomation()
{
    for (double rate : {88200.0, 192000.0, 384000.0})
    {
        IronheartChannelNetwork network;
        IronheartTriodeStage triode;
        triode.prepare(rate);
        network.prepare(rate, Channel::Lead, 0.5f, 0.5f, 0.5f);
        bool stable = true;
        double maxResidual = 0.0, maximum = 0.0;
        for (int n = 0; n < 40000; ++n)
        {
            if (n % 4001 == 0)
                network.setControls(n % 3 == 0 ? Channel::Clean : n % 3 == 1 ? Channel::Rhythm : Channel::Lead,
                                    n % 2 ? 0.0f : 1.0f, n % 2 ? 1.0f : 0.0f, n % 2 ? 0.0f : 1.0f);
            const auto load = network.beginSample();
            const double plate = triode.processSample(4.0 * std::sin(2 * kPi * 110.0 * n / rate), load);
            const auto out = network.finishSample(plate);
            const auto point = triode.operatingPoint();
            const double residual = (383.0 - point.plateVolts) / 100000.0
                - point.plateCurrentAmps - (load.conductance * plate - load.historyAmps);
            maxResidual = std::max(maxResidual, std::fabs(residual));
            maximum = std::max({maximum, std::fabs(out.lead), std::fabs(out.rhythm), std::fabs(out.clean)});
            stable &= std::isfinite(plate) && std::isfinite(maximum) && point.converged
                && load.conductance > 0.0;
        }
        check(stable && maxResidual < 1e-10 && maximum < 400.0,
              "V1A acoplado satisfaz KCL sob automacao e troca de canal: " + std::to_string(rate));
        triode.reset(); network.reset();
        bool silent = true;
        for (int n = 0; n < 1000; ++n)
        {
            const auto load = network.beginSample();
            const auto out = network.finishSample(triode.processSample(0.0, load));
            silent &= out.lead == 0.0 && out.rhythm == 0.0 && out.clean == 0.0;
        }
        check(silent, "reset conjunto restaura capacitores sem transiente");
    }
}

void testLimits()
{
    IronheartChannelNetwork network;
    const auto unpreparedLoad = network.beginSample();
    const auto unpreparedOutput = network.finishSample(1.0);
    check(unpreparedLoad.conductance == 0.0 && unpreparedLoad.historyAmps == 0.0
          && unpreparedOutput.lead == 0.0 && unpreparedOutput.rhythm == 0.0 && unpreparedOutput.clean == 0.0,
          "rede nao preparada retorna silencio sem acessar estados invalidos");
    network.prepare(192000.0, Channel::Clean, -2.0f, 5.0f, std::numeric_limits<float>::quiet_NaN());
    bool finite = true;
    for (int n = 0; n < 10000; ++n)
    {
        const auto load = network.beginSample();
        const auto out = network.finishSample(n == 0 ? 1.0 : 0.0);
        finite &= std::isfinite(load.conductance) && std::isfinite(load.historyAmps)
            && std::isfinite(out.lead) && std::isfinite(out.rhythm) && out.clean == 0.0;
    }
    check(finite, "pots limitados e batente Clean zero silencioso mesmo com bright");
    bool rejected = false;
    try { network.prepare(0, Channel::Clean, 0, 0, 0); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "prepare valida taxa interna");

    network.prepare(192000.0, Channel::Lead, 0.5f, 0.5f, 0.5f);
    IronheartChannelNetwork::Outputs dc{};
    double current = 0.0;
    for (int n = 0; n < 192000 * 3; ++n)
    {
        const auto load = network.beginSample();
        dc = network.finishSample(1.0);
        current = load.conductance - load.historyAmps;
    }
    check(std::max({std::fabs(dc.lead), std::fabs(dc.rhythm), std::fabs(dc.clean)}) < 1e-8
          && std::fabs(current) < 1e-10, "acoplamentos bloqueiam DC sem carga estatica sobre V1A");
}
}

int main()
{
    testReference();
    testBrightAndParallelLoading();
    testCoupledTriodeAndAutomation();
    testLimits();
    return reportResults();
}
