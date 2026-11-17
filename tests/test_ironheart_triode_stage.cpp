#include "IronheartTriodeStage.h"
#include "test_helpers.h"

#include <algorithm>
#include <complex>
#include <limits>
#include <stdexcept>

namespace
{
constexpr double kPi = 3.14159265358979323846;

// Oráculo em long double, sem usar o avaliador/derivadas/solver de produção.
// Equação de Koren para ECC83 genérica, fonte indicada na documentação.
double referenceCurrent(double plateCathode, double gridCathode)
{
    if (plateCathode <= 0.0) return 0.0;
    const long double vp = plateCathode, vg = gridCathode;
    const auto x = 600.0L * (0.01L + vg / std::sqrt(300.0L + vp * vp));
    const auto softplus = x > 0.0L ? x + std::log1p(std::exp(-x)) : std::log1p(std::exp(x));
    return static_cast<double>(2.0L * std::pow(vp / 600.0L * softplus, 1.4L) / 1060.0L);
}

IronheartTriodeStage::OperatingPoint referenceBias(double grid)
{
    double low = 0.0, high = 383.0 / 100000.0;
    for (int n = 0; n < 100; ++n)
    {
        const double current = (low + high) / 2.0;
        const double model = referenceCurrent(383.0 - 101500.0 * current, grid - 1500.0 * current);
        if (current > model) high = current;
        else low = current;
    }
    const double current = (low + high) / 2.0;
    return {383.0 - 100000.0 * current, 1500.0 * current, current, true};
}

void testBiasAndReset()
{
    IronheartTriodeStage stage;
    stage.prepare(192000.0);
    const auto bias = stage.biasPoint();
    const auto expected = referenceBias(0.0);
    check(bias.converged && std::fabs(bias.plateVolts - expected.plateVolts) < 1e-6
          && std::fabs(bias.cathodeVolts - expected.cathodeVolts) < 1e-8,
          "repouso coincide com solucao DC independente");
    check(std::fabs(bias.plateVolts - 250.0) < 15.0 && std::fabs(bias.cathodeVolts - 1.8) < 0.15,
          "repouso proximo das anotacoes de 250 V / 1,8 V, sem forcar ajuste");
    std::cout << "  repouso: Va=" << bias.plateVolts << " V Vk=" << bias.cathodeVolts
              << " V Ia=" << bias.plateCurrentAmps * 1000.0 << " mA\n";
    bool silent = true;
    for (int n = 0; n < 10000; ++n) silent &= stage.processSample(0.0) == 0.0;
    check(silent, "inicializacao com C18 carregado nao gera pop em silencio");
    for (int n = 0; n < 5000; ++n) stage.processSample(3.0);
    stage.reset();
    check(stage.processSample(0.0) == 0.0 && stage.operatingPoint().plateVolts == bias.plateVolts,
          "reset restaura repouso e apaga polarizacao da nota anterior");
    stage.prepare(384000.0);
    check(stage.biasPoint().plateVolts == bias.plateVolts, "repouso DC independe da taxa interna");
    bool rejects = true;
    for (double rate : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::quiet_NaN()})
    {
        bool threw = false;
        try { stage.prepare(rate); }
        catch (const std::invalid_argument&) { threw = true; }
        rejects &= threw;
    }
    check(rejects, "prepare rejeita taxas invalidas");
}

std::complex<double> referenceSmallSignal(double frequency)
{
    const auto bias = referenceBias(0.0);
    const double vp = bias.plateVolts - bias.cathodeVolts, vg = -bias.cathodeVolts;
    constexpr double delta = 1e-4;
    const double gm = (referenceCurrent(vp, vg + delta) - referenceCurrent(vp, vg - delta)) / (2 * delta);
    const double gp = (referenceCurrent(vp + delta, vg) - referenceCurrent(vp - delta, vg)) / (2 * delta);
    const auto zk = 1.0 / std::complex<double>(1.0 / 1500.0, 2.0 * kPi * frequency * 1e-6);
    return -100000.0 * gm / (1.0 + 100000.0 * gp + (gm + gp) * zk);
}

std::complex<double> measuredSmallSignal(double rate, double frequency)
{
    IronheartTriodeStage stage;
    stage.prepare(rate);
    constexpr double amplitude = 1e-5;
    double ss = 0.0, cc = 0.0, sc = 0.0, ys = 0.0, yc = 0.0;
    const int settle = static_cast<int>(rate / 10.0);
    const int count = settle + static_cast<int>(rate / 2.0);
    for (int n = 0; n < count; ++n)
    {
        const double angle = 2.0 * kPi * frequency * n / rate;
        const double sine = std::sin(angle);
        const double output = stage.processSample(amplitude * sine) / amplitude;
        if (n < settle) continue;
        const double cosine = std::cos(angle);
        ss += sine * sine; cc += cosine * cosine; sc += sine * cosine;
        ys += output * sine; yc += output * cosine;
    }
    const double determinant = ss * cc - sc * sc;
    return {(ys * cc - yc * sc) / determinant, (yc * ss - ys * sc) / determinant};
}

void testSmallSignal()
{
    double maxError = 0.0;
    for (double rate : {176400.0, 192000.0, 384000.0})
    {
        bool matches = true;
        for (double frequency : {20.0, 82.41, 220.0, 1000.0, 5000.0, 20000.0})
        {
            const double warped = rate / kPi * std::tan(kPi * frequency / rate);
            const auto expected = referenceSmallSignal(warped);
            const auto measured = measuredSmallSignal(rate, frequency);
            const double error = std::abs(measured - expected) / std::abs(expected);
            maxError = std::max(maxError, error);
            matches &= error < 0.001;
        }
        check(matches, "ganho e fase concordam com linearizacao do circuito: " + std::to_string(rate));
    }
    const auto low = measuredSmallSignal(192000.0, 20.0);
    const auto high = measuredSmallSignal(192000.0, 5000.0);
    check(20.0 * std::log10(std::abs(high / low)) > 5.0,
          "C18 aumenta ganho acima da regiao grave");
    check(high.real() < 0.0, "estagio de catodo comum inverte a polaridade");
    std::cout << "  ganho 20 Hz=" << std::abs(low) << "; 5 kHz=" << std::abs(high)
              << "; maior erro complexo relativo=" << maxError << '\n';
}

void testDynamicEquations()
{
    for (double rate : {88200.0, 176400.0, 192000.0, 384000.0})
    {
        IronheartTriodeStage stage;
        stage.prepare(rate);
        auto previous = stage.operatingPoint();
        bool valid = true;
        double maxPlateResidual = 0.0, maxCapResidual = 0.0;
        for (int n = 0; n < 40000; ++n)
        {
            const double input = (n < 20000 ? 0.2 : 12.0) * std::sin(2 * kPi * 220.0 * n / rate);
            const double output = stage.processSample(input);
            const auto point = stage.operatingPoint();
            const double plateResidual = point.plateCurrentAmps
                - referenceCurrent(point.plateVolts - point.cathodeVolts, input - point.cathodeVolts);
            const double capResidual = 2.0 * rate * 1e-6 * (point.cathodeVolts - previous.cathodeVolts)
                - (point.plateCurrentAmps - point.cathodeVolts / 1500.0)
                - (previous.plateCurrentAmps - previous.cathodeVolts / 1500.0);
            maxPlateResidual = std::max(maxPlateResidual, std::fabs(plateResidual));
            maxCapResidual = std::max(maxCapResidual, std::fabs(capResidual));
            valid &= point.converged && std::isfinite(output) && point.plateVolts >= 0.0
                && point.plateVolts <= 383.0 && point.cathodeVolts >= 0.0
                && std::fabs((383.0 - point.plateVolts) / 100000.0 - point.plateCurrentAmps) < 1e-12;
            previous = point;
        }
        check(valid && maxPlateResidual < 1e-9 && maxCapResidual < 1e-11,
              "KCL de placa e capacitor satisfeitas sob sobrecarga: " + std::to_string(rate));
    }
    IronheartTriodeStage stage;
    stage.prepare(192000.0);
    bool bounded = true;
    for (int n = 0; n < 10000; ++n)
    {
        const double output = stage.processSample(n % 2 == 0 ? 1e30 : -1e30);
        bounded &= stage.operatingPoint().converged && std::isfinite(output) && std::fabs(output) < 383.0;
    }
    bounded &= std::isfinite(stage.processSample(std::numeric_limits<double>::quiet_NaN()));
    check(bounded, "solver protegido permanece limitado com entradas extremas e nao finitas");
}

void testBiasMemoryAndCompression()
{
    IronheartTriodeStage stage;
    stage.prepare(192000.0);
    const double initialCathode = stage.biasPoint().cathodeVolts;
    double first = 0.0, last = 0.0;
    for (int n = 0; n < 192000 / 10; ++n)
    {
        const double output = stage.processSample(1.0);
        if (n == 0) first = output;
        last = output;
    }
    const auto expected = referenceBias(1.0);
    check(stage.operatingPoint().cathodeVolts > initialCathode + 0.2
          && std::fabs(stage.operatingPoint().plateVolts - expected.plateVolts) < 1e-5,
          "C18 acompanha nova polarizacao e converge ao DC independente");
    check(std::fabs(first - last) > 10.0, "transiente depende do estado do catodo, nao so da amostra atual");
    for (int n = 0; n < 192000 / 10; ++n) stage.processSample(0.0);
    check(std::fabs(stage.operatingPoint().cathodeVolts - initialCathode) < 1e-7,
          "polarizacao recupera depois da excitacao");

    // Pulsos partindo do mesmo repouso separam a assimetria da memória.
    stage.reset(); const double positive = stage.processSample(4.0);
    stage.reset(); const double negative = stage.processSample(-4.0);
    check(std::fabs(positive + negative) > 10.0, "corte e conducao produzem saturacao assimetrica");
    stage.reset(); const double small = std::fabs(stage.processSample(0.01));
    stage.reset(); const double overloaded = std::fabs(stage.processSample(12.0));
    check(overloaded / 12.0 < small / 0.01 * 0.8,
          "ganho efetivo diminui em sobrecarga forte sem tanh");
}
}

int main()
{
    testBiasAndReset();
    testSmallSignal();
    testDynamicEquations();
    testBiasMemoryAndCompression();
    return reportResults();
}
