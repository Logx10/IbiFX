#include "IronheartOutputStage.h"
#include "IronheartPowerSection.h"
#include "IronheartMaster.h"
#include "Reverb.h"
#include "test_helpers.h"

#include <algorithm>
#include <array>
#include <complex>
#include <limits>
#include <stdexcept>
#include <vector>

namespace
{
using C = std::complex<double>;
constexpr double pi = 3.14159265358979323846;

// Fonte de 1V no topo de VR5/8: resolve o cursor carregado, sem o
// equivalente de resistência em paralelo empregado na produção.
std::array<double, 2> volumeReference(float knob)
{
    const double ratio = (std::pow(81.0, knob) - 1.0) / 80.0;
    if (knob == 0) return {1e-6, 0};
    if (knob == 1) return {1e-6 + 1.0 / 680e3, 330.0 / 680.0};
    const double a = 1.0 / (1e6 * (1.0 - ratio)), b = 1.0 / (1e6 * ratio), c = 1.0 / 680e3;
    const double cursor = a / (a + b + c);
    return {a * (1.0 - cursor), cursor * 330e3 / 680e3};
}

void testOutput()
{
    double maximum = 0;
    constexpr double rate = 192000;
    for (float knob : {0.0f, .2f, .5f, .8f, 1.0f})
    {
        for (int mode = 0; mode < 3; ++mode)
        {
            const bool clean = mode == 0, lead = mode == 2;
            IronheartOutputStage stage;
            stage.prepare(rate, knob, knob, clean);
            const auto expected = volumeReference(knob);
            const auto loads = stage.beginSample();
            const double rhythmLoad = clean ? 1e-6 + 1.0 / 660e3 : expected[0] + 1.0 / 330e3;
            check(std::abs(loads.lead - expected[0]) < 1e-16 && std::abs(loads.rhythm - rhythmLoad) < 1e-16,
                  "carga de VR5/VR8 e bypass Clean bate com analise nodal");
            for (double hz : {80.0, 1000.0, 8000.0})
            {
                stage.reset();
                C sum{};
                const int count = 24000;
                for (int n = 0; n < count * 2; ++n)
                {
                    const double phase = 2 * pi * hz * n / rate;
                    stage.beginSample();
                    const double y = stage.finishSample(std::sin(phase), std::sin(phase), lead ? 1 : 0);
                    if (n >= count) sum += y * C(std::sin(phase), std::cos(phase)) * (2.0 / count);
                }
                const C s(0, 2 * rate * std::tan(pi * hz / rate));
                const double transfer = clean ? .5 : expected[1];
                const C reference = transfer / ((1.0 + s * 330e3 * 22e-12) * (1.0 + s * 10e3 * 47e-12));
                const double error = std::abs(sum - reference) / std::max(1e-12, std::abs(reference));
                maximum = std::max(maximum, error);
            }
        }
    }
    std::cout << "  maior erro relativo dos buffers: " << maximum << '\n';
    check(maximum < 2e-10, "amplitude/fase dos dois buffers e somador contra impedancias complexas");
    IronheartOutputStage stage;
    stage.prepare(rate, 1, 1, false);
    stage.beginSample();
    for (int n = 0; n < 100; ++n) stage.finishSample(1000, -1000, .7);
    check(std::abs(stage.finishSample(1000, -1000, 1)) <= 13, "buffers respeitam excursao aproximada dos TL072");
    stage.reset(); stage.beginSample();
    check(stage.finishSample(0, 0, 1) == 0, "reset limpa os tres capacitores de feedback");
}

// Rede passiva resolvida por divisores de impedância, independente da
// inversa 3x3 e dos companheiros TPT usados no código de produção.
C branchReference(C s, double sourceR, double tone, double watts)
{
    const C cin = 1.0 / (s * 100e-9), cout = cin, ctone = 1.0 / (s * 10e-9);
    const C bottom = 10e3 + 220e3 * watts;
    const C cursor = 1.0 / (1.0 / bottom + 1.0 / (cout + 220e3));
    const double top = std::max(1.0, 220e3 * (1.0 - watts));
    const C toneZ = 220e3 * tone + ctone;
    const C load = 1.0 / (1.0 / toneZ + 1.0 / (top + cursor));
    return load / (sourceR + cin + load) * cursor / (top + cursor) * 220e3 / (cout + 220e3);
}

template<class Function>
C measure(double rate, double hz, Function&& process, double amplitude = 1e-6)
{
    // Janelas inteiras de 0,5 s; as frequências escolhidas são pares.
    const int count = static_cast<int>(rate / 2);
    C sum{};
    for (int n = 0; n < count * 2; ++n)
    {
        const double phase = 2 * pi * hz * n / rate;
        const double output = process(static_cast<float>(amplitude * std::sin(phase)));
        if (n >= count) sum += output * C(std::sin(phase), std::cos(phase)) * (2.0 / (count * amplitude));
    }
    return sum;
}

void testPowerResponse()
{
    double maximum = 0;
    for (double rate : {22050.0, 48000.0, 96000.0})
    {
        for (float position : {0.0f, .5f, 1.0f})
        {
            for (double hz : {80.0, 220.0, 1000.0, 4000.0})
            {
                IronheartPowerSection power;
                power.prepare(rate, position, position, position);
                const C actual = measure(rate, hz, [&power](float x) { return power.processSample(x); });
                Oversampler fir;
                const C firGain = measure(rate, hz, [&fir](float x) { return fir.process(x, [](float y) { return y; }); });
                const C s(0, 8 * rate * std::tan(pi * hz / (4 * rate)));
                const C passive = .5 * (branchReference(s, 35e3, position, position) + branchReference(s, 38e3, position, position));
                const C beta = 4.7e3 / (4.7e3 + 39e3 + 220e3 * position / (1.0 + s * (220e3 * position * 6.8e-9)));
                const C input = s * .022 / (1.0 + s * .022);
                const C expected = firGain * input * (12.0 * passive) / (1.0 + 12.0 * passive * beta);
                maximum = std::max(maximum, std::abs(actual - expected) / std::abs(expected));
            }
        }
    }
    std::cout << "  maior erro relativo do master: " << maximum << '\n';
    check(maximum < 3e-5, "Tone/Watts duplos e NFB dentro da mesma malha: ganho/fase contra circuito linear");
}

void testPowerBehaviour()
{
    const auto gain = [](float tone, float watts, float dynamics, double hz, double amplitude = 1e-6) {
        IronheartPowerSection power; power.prepare(48000, tone, watts, dynamics);
        return measure(48000, hz, [&power](float x) { return power.processSample(x); }, amplitude);
    };
    const auto minimum = std::abs(gain(.5f, 0, .5f, 220));
    const auto normal = std::abs(gain(.5f, .5f, .5f, 220));
    const auto maximum = std::abs(gain(.5f, 1, .5f, 220));
    check(minimum > 0 && minimum < normal && normal < maximum, "Watts cobre saida reduzida ate maxima sem virar mute");
    const double dark = std::abs(gain(0, .5f, .5f, 4000)) / std::abs(gain(0, .5f, .5f, 80));
    const double bright = std::abs(gain(1, .5f, .5f, 4000)) / std::abs(gain(1, .5f, .5f, 80));
    check(bright > 2 * dark, "Tone abre agudos nos dois ramos antes da saturacao");
    const double tight = std::abs(gain(.5f, .5f, 0, 80)) / std::abs(gain(.5f, .5f, 0, 4000));
    const double loose = std::abs(gain(.5f, .5f, 1, 80)) / std::abs(gain(.5f, .5f, 1, 4000));
    check(loose > tight * 1.2, "Dynamics alivia realimentacao dos graves");
    // Na região saturada, baixar Watts muda a curva, não apenas a escala.
    const double hotRatio = std::abs(gain(.5f, .25f, .5f, 220, 1.0)) / std::abs(gain(.5f, 1, .5f, 220, 1.0));
    const double smallRatio = std::abs(gain(.5f, .25f, .5f, 220)) / maximum;
    check(hotRatio > smallRatio * 1.5, "Watts muda a compressao da potencia, alem do volume");
    IronheartPowerSection power;
    power.prepare(48000, .5f, .5f, .5f);
    bool finite = true;
    for (int event = 0; event < 8; ++event)
    {
        power.setControls(event % 2 ? 0 : 1, event % 3 ? 0 : 1, event % 2 ? 1 : 0);
        for (int n = 0; n < 2048; ++n)
        {
            const float y = power.processSample(static_cast<float>(100 * std::sin(2 * pi * n / 31)));
            finite = finite && std::isfinite(y) && std::abs(y) < 2;
        }
    }
    check(finite, "automacao e entrada muito quente conservam limites e estabilidade");
    power.reset();
    check(power.processSample(0) == 0, "reset limpa rede, sag, NFB e FIR");
    check(std::isfinite(power.processSample(std::numeric_limits<float>::quiet_NaN())), "entrada nao finita nao contamina a potencia");
    bool bounded = true;
    for (int n = 0; n < 256; ++n)
        bounded &= std::isfinite(power.processSample(std::numeric_limits<float>::max()));
    check(bounded, "protecao numerica evita overflow no FIR com float extremo");
}

void testReverbAndBlocks()
{
    Reverb mixed, wetOnly;
    mixed.setMix(.37f); mixed.prepare(48000, 128); wetOnly.prepare(48000, 128);
    bool compatible = true;
    for (int n = 0; n < 8192; ++n)
    {
        const float input = n < 128 ? static_cast<float>(.1 * std::sin(2 * pi * n / 17)) : 0.0f;
        std::vector<float> single{input};
        mixed.process(single);
        const float expected = input * (1.0f - .37f) + wetOnly.processWetSample(input) * .37f;
        compatible &= single[0] == expected;
    }
    check(compatible, "API de cauda preserva mistura do Reverb existente amostra a amostra");
    // Antes do primeiro retorno dos delays, ligar o reverb não pode baixar
    // nem alterar a nota seca (o somador do cabeçote é paralelo).
    IronheartMaster dry, wet;
    dry.findParameter("reverbEnabled")->setValue(0);
    wet.findParameter("reverb")->setValue(1);
    dry.prepare(48000, 128); wet.prepare(48000, 128);
    std::vector<float> a(512, 0), b;
    a[0] = .1f; b = a; dry.process(a); wet.process(b);
    check(a == b, "reverb paralelo preserva nota seca antes da cauda");

    IronheartMaster dc;
    dc.findParameter("reverbEnabled")->setValue(0); dc.prepare(48000, 128);
    std::vector<float> offset(128, .2f);
    double last = 0;
    for (int block = 0; block < 800; ++block)
    {
        std::fill(offset.begin(), offset.end(), .2f); dc.process(offset);
        last = std::abs(offset.back());
    }
    check(last < 1e-6, "master bloqueia DC mesmo apos saturacao");

    const auto render = [](int blockSize) {
        IronheartMaster master; master.prepare(48000, blockSize);
        std::vector<float> output, block;
        for (int event = 0; event < 4; ++event)
        {
            for (const char* id : {"tone", "watts", "dynamics", "reverb", "reverbEnabled", "play"})
                master.findParameter(id)->setValue(event % 2 ? .9f : .1f);
            for (int start = 0; start < 4096; start += blockSize)
            {
                block.resize(static_cast<std::size_t>(std::min(blockSize, 4096 - start)));
                for (std::size_t n = 0; n < block.size(); ++n)
                    block[n] = static_cast<float>(.2 * std::sin(2 * pi * 220 * (event * 4096 + start + n) / 48000));
                master.process(block); output.insert(output.end(), block.begin(), block.end());
            }
        }
        return output;
    };
    check(render(1) == render(128), "master/reverb/mute com automacao independe dos blocos");
}
}

int main()
{
    testOutput(); testPowerResponse(); testPowerBehaviour(); testReverbAndBlocks();
    return reportResults();
}
