#include "IronheartGainStages.h"
#include "test_helpers.h"

#include <algorithm>
#include <complex>
#include <limits>
#include <stdexcept>

namespace
{
using Complex = std::complex<double>;
constexpr double kPi = 3.14159265358979323846;

double plateCurrent(double vp, double vg)
{
    if (vp <= 0.0) return 0.0;
    const long double x = 600.0L * (0.01L + vg / std::sqrt(300.0L + static_cast<long double>(vp) * vp));
    const auto softplus = x > 0 ? x + std::log1p(std::exp(-x)) : std::log1p(std::exp(x));
    return static_cast<double>(2.0L / 1060.0L * std::pow(vp / 600.0L * softplus, 1.4L));
}

struct Bias { double plate, cathode, current, gm, gp; };
Bias bias(double supply, double dcLoad)
{
    const double resistance = 1.0 / (1.0 / 100e3 + dcLoad);
    const double source = supply / 100e3 * resistance;
    double low = 0.0, high = source / resistance;
    for (int n = 0; n < 100; ++n)
    {
        const double i = (low + high) / 2.0;
        if (i > plateCurrent(source - resistance * i - 1500 * i, -1500 * i)) high = i;
        else low = i;
    }
    const double current = (low + high) / 2.0;
    const double va = source - resistance * current, vk = 1500 * current;
    constexpr double h = 1e-4;
    const double gm = (plateCurrent(va - vk, -vk + h) - plateCurrent(va - vk, -vk - h)) / (2 * h);
    const double gp = (plateCurrent(va - vk + h, -vk) - plateCurrent(va - vk - h, -vk)) / (2 * h);
    return {va, vk, current, gm, gp};
}

// Linearização por impedâncias das redes, independente dos estados e
// solvers do processamento. A grade ideal permite compor as três seções.
Complex expected(double frequency, bool clean)
{
    const Complex s(0, 2 * kPi * frequency);
    const auto b = bias(383, 0), a = bias(410, 0), c = bias(410, 2.0 / 1090e3);
    const auto gain = [](Bias point, Complex plateY, Complex cathodeZ) {
        return -point.gm / (plateY + point.gp + (point.gm + point.gp) * cathodeZ * plateY);
    };
    const auto z6 = 1.0 / (s * 10e-9) + 370e3;
    const double termination = 1.0 / (1.0 / 150e3 + (clean ? 1.0 / 22e3 : 0.0));
    const auto z7 = 1.0 / (s * 3.4e-9) + 150e3 + termination;
    const auto outputGround = 1.0 / (1.0 / 470e3 + s * 220e-9);
    const auto outputZ = 620e3 + outputGround;
    return gain(b, 1.0 / 100e3 + 1.0 / z6, 1.0 / (1.0 / 1500.0 + s * 22e-6))
        * 150e3 / z6
        * gain(a, 1.0 / 100e3 + s * 220e-12 + 1.0 / z7, 1500.0)
        * termination / z7
        * gain(c, 1.0 / 100e3 + 2.0 / outputZ, 1500.0)
        * (150e3 + outputGround) / outputZ;
}

Complex measure(double rate, double frequency, bool clean)
{
    IronheartGainStages stages;
    stages.prepare(rate, clean);
    constexpr double amplitude = 1e-7;
    double ss = 0, cc = 0, sc = 0, ys = 0, yc = 0;
    const int settle = static_cast<int>(rate / 2.0);
    for (int n = 0; n < static_cast<int>(rate); ++n)
    {
        const double angle = 2 * kPi * frequency * n / rate;
        const double sine = std::sin(angle), cosine = std::cos(angle);
        const double output = stages.processSample(amplitude * sine) / amplitude;
        if (n < settle) continue;
        ss += sine * sine; cc += cosine * cosine; sc += sine * cosine;
        ys += output * sine; yc += output * cosine;
    }
    return {(ys * cc - yc * sc) / (ss * cc - sc * sc),
            (yc * ss - ys * sc) / (ss * cc - sc * sc)};
}

void testBias()
{
    IronheartGainStages stages;
    stages.prepare(192000, true);
    const auto b = stages.v1b().biasPoint(), a = stages.v2a().biasPoint(), c = stages.v2b().biasPoint();
    const auto rb = bias(383, 0), ra = bias(410, 0), rc = bias(410, 2.0 / 1090e3);
    check(std::fabs(b.plateVolts - rb.plate) < 1e-6 && std::fabs(a.plateVolts - ra.plate) < 1e-6
          && std::fabs(c.plateVolts - rc.plate) < 1e-6, "repousos coincidem com bissecao DC independente");
    check(c.plateVolts < a.plateVolts - 20.0, "duas redes de V3 carregam a placa de V2B em DC");
    std::cout << "  repousos Va/Vk: V1B=" << b.plateVolts << '/' << b.cathodeVolts
              << " V2A=" << a.plateVolts << '/' << a.cathodeVolts
              << " V2B=" << c.plateVolts << '/' << c.cathodeVolts << '\n';
    bool silent = true;
    for (int n = 0; n < 2000; ++n) silent &= stages.processSample(0.0) == 0.0;
    check(silent, "capacitores inicializados no repouso sem pop");
}

void testResponse()
{
    double maxError = 0.0;
    for (bool clean : {false, true})
    {
        bool matches = true;
        for (double frequency : {20.0, 82.41, 220.0, 1000.0, 5000.0, 20000.0})
        {
            const double rate = 192000.0;
            const auto wanted = expected(rate / kPi * std::tan(kPi * frequency / rate), clean);
            const auto actual = measure(rate, frequency, clean);
            const double error = std::abs(actual - wanted) / std::abs(wanted);
            maxError = std::max(maxError, error);
            matches &= error < 1e-4;
        }
        check(matches, std::string("ganho e fase com shunt ") + (clean ? "fechado" : "aberto"));
    }
    for (double rate : {88200.0, 176400.0, 384000.0})
    {
        const auto actual = measure(rate, 1000, false);
        const auto wanted = expected(rate / kPi * std::tan(kPi * 1000 / rate), false);
        check(std::abs(actual - wanted) / std::abs(wanted) < 1e-4, "resposta na taxa interna " + std::to_string(rate));
    }
    check(std::abs(measure(192000, 1000, true)) < std::abs(measure(192000, 1000, false)) * 0.2,
          "R187 fechado reduz ganho do Clean sem mudar numero de triodos");
    std::cout << "  maior erro complexo relativo: " << maxError << '\n';
}

void testDynamicKCL()
{
    for (double rate : {88200.0, 192000.0, 384000.0})
    {
        IronheartGainStages stages;
        stages.prepare(rate, false);
        double oldC6V = 0, oldC6I = 0, oldC7V = 0, oldC7I = 0;
        double oldPlateA = 0, oldPlateCapI = 0, oldOutputBottom = 0, oldOutputCapI = 0;
        double maxPlate = 0, maxCap = 0;
        bool valid = true;
        for (int n = 0; n < 40000; ++n)
        {
            if (n % 4001 == 0) stages.setClean(n % 2 == 0);
            const double input = 4.0 * std::sin(2 * kPi * 110 * n / rate);
            const double output = stages.processSample(input);
            const auto v = stages.signals();
            const auto b = stages.v1b().operatingPoint(), a = stages.v2a().operatingPoint(), c = stages.v2b().operatingPoint();
            const double i6 = v.v2aGrid / 150e3, cap6V = v.v1bPlateAC - i6 * 370e3;
            const double termination = 1.0 / (1.0 / 150e3 + v.cleanShuntConductance);
            const double i7 = v.v2bGrid / termination, cap7V = v.v2aPlateAC - i7 * (150e3 + termination);
            const double plateCapI = 2 * rate * 220e-12 * (v.v2aPlateAC - oldPlateA) - oldPlateCapI;
            const double outputI = 2 * (v.v2bPlateAC - output) / 470e3;
            const double outputBottom = v.v2bPlateAC - outputI * 620e3 / 2;
            const double outputCapI = outputI - 2 * outputBottom / 470e3;
            maxCap = std::max({maxCap,
                std::fabs(2 * rate * 10e-9 * (cap6V - oldC6V) - i6 - oldC6I),
                std::fabs(2 * rate * 3.4e-9 * (cap7V - oldC7V) - i7 - oldC7I),
                std::fabs(4 * rate * 220e-9 * (outputBottom - oldOutputBottom) - outputCapI - oldOutputCapI)});
            maxPlate = std::max({maxPlate,
                std::fabs((383 - b.plateVolts) / 100e3 - b.plateCurrentAmps - i6),
                std::fabs((410 - a.plateVolts) / 100e3 - a.plateCurrentAmps - i7 - plateCapI),
                std::fabs((410 - c.plateVolts) / 100e3 - c.plateCurrentAmps
                    - stages.v2b().biasPoint().plateVolts * 2 / 1090e3 - outputI),
                std::fabs(a.cathodeVolts / 1500 - a.plateCurrentAmps),
                std::fabs(c.cathodeVolts / 1500 - c.plateCurrentAmps)});
            valid &= b.converged && a.converged && c.converged && std::isfinite(output) && std::fabs(output) < 300;
            oldC6V = cap6V; oldC6I = i6; oldC7V = cap7V; oldC7I = i7;
            oldPlateA = v.v2aPlateAC; oldPlateCapI = plateCapI;
            oldOutputBottom = outputBottom; oldOutputCapI = outputCapI;
        }
        check(valid && maxPlate < 1e-10 && maxCap < 1e-10,
              "KCL e trapezio satisfeitos sob saturacao e chaveamento: " + std::to_string(rate));
        stages.reset();
        check(stages.processSample(0.0) == 0.0, "reset remove caudas dos tres estagios");
    }
}

void testLimits()
{
    IronheartGainStages stages;
    check(stages.processSample(1.0) == 0.0, "antes de prepare retorna silencio");
    bool rejected = false;
    try { stages.prepare(std::numeric_limits<double>::quiet_NaN(), false); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "prepare valida taxa");
    stages.prepare(192000, false);
    bool finite = true;
    for (int n = 0; n < 10000; ++n)
        finite &= std::isfinite(stages.processSample(n % 2 ? -1e20 : 1e20));
    finite &= std::isfinite(stages.processSample(std::numeric_limits<double>::infinity()));
    check(finite, "sobrecarga extrema permanece finita");
    stages.prepare(176400, true);
    check(stages.processSample(0.0) == 0.0, "nova preparacao troca taxa e restaura repouso");

    IronheartTriodeStage triode;
    bool invalidCircuit = false;
    try { triode.prepare(192000, {410.0, 100e3, 1.5e3, -1e-6, 0.0}); }
    catch (const std::invalid_argument&) { invalidCircuit = true; }
    check(invalidCircuit, "componentes negativos sao rejeitados fora do callback");
}
}

int main()
{
    testBias(); testResponse(); testDynamicKCL(); testLimits();
    return reportResults();
}
