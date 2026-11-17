#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "GearLibrary.h"
#include "ModuleChain.h"
#include "Preset.h"
#include "test_helpers.h"

namespace
{
std::vector<float> input(double rate, std::size_t count)
{
    std::vector<float> result(count);
    for (std::size_t n = 0; n < count; ++n)
        result[n] = static_cast<float>(0.15 * std::sin(6.283185307179586 * 82.41 * n / rate)
                                     + 0.05 * std::sin(6.283185307179586 * 1100.0 * n / rate));
    return result;
}

std::vector<float> render(ModuleChain& chain, const std::vector<float>& source, std::size_t blockSize)
{
    auto result = source;
    std::vector<float> block;
    block.reserve(blockSize);
    for (std::size_t start = 0; start < result.size(); start += blockSize)
    {
        const auto count = std::min(blockSize, result.size() - start);
        block.assign(result.begin() + static_cast<std::ptrdiff_t>(start),
                     result.begin() + static_cast<std::ptrdiff_t>(start + count));
        chain.process(block);
        std::copy(block.begin(), block.end(), result.begin() + static_cast<std::ptrdiff_t>(start));
    }
    return result;
}

void testCatalogSafety(const GearLibrary& library, const GearModel& amp)
{
    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        for (int setting = 0; setting < 3; ++setting)
        {
            ModuleChain chain;
            preset::apply(library.buildSignalChain({amp.id}, "Safety"), chain);
            if (setting != 0)
                for (const auto& control : amp.controls)
                    chain.moduleAt(control.moduleOffset).findParameter(control.parameterId)
                        ->setValue(setting == 1 ? control.minValue : control.maxValue);
            chain.prepare(rate, 127);
            const auto signal = input(rate, 4096);
            const auto output = render(chain, signal, 127);
            const bool stable = std::all_of(output.begin(), output.end(), [](float x) {
                return std::isfinite(x) && std::fabs(x) < 10.0f;
            });
            check(stable, amp.id + " estavel: taxa=" + std::to_string(rate)
                          + " ajuste=" + std::to_string(setting));
            if (setting == 0)
                check(std::any_of(output.begin(), output.end(), [](float x) { return std::fabs(x) > 1e-6f; }),
                      amp.id + " produz audio no padrao");
            chain.reset();
            std::vector<float> silence(256, 0.0f);
            chain.process(silence);
            check(std::all_of(silence.begin(), silence.end(), [](float x) { return x == 0.0f; }),
                  amp.id + " reset remove notas anteriores");
        }
    }
}

void testBlockBoundaries(const GearLibrary& library, const GearModel& amp)
{
    const auto recipe = library.buildSignalChain({amp.id}, "Blocks");
    ModuleChain one, many;
    preset::apply(recipe, one);
    preset::apply(recipe, many);
    one.prepare(48000.0, 1);
    many.prepare(48000.0, 128);
    const auto source = input(48000.0, 4096);
    check(render(one, source, 1) == render(many, source, 128),
          amp.id + " mesmo audio com blocos de 1 e 128 amostras");
    one.setBypassed(0, true);
    // Bypass da receita inteira: impede que um amp multi-módulo fique
    // parcialmente ligado por erro na seleção dos índices.
    for (std::size_t m = 0; m < one.size(); ++m) one.setBypassed(m, true);
    check(render(one, source, 1) == source, amp.id + " bypass integral preserva entrada");
}

void testMasterChangesPowerDrive(const GearLibrary& library)
{
    for (const std::string id : {"amp.brit-plexi", "amp.brit-silver"})
    {
        const auto* model = library.find(id);
        const GearControl* master = nullptr;
        const GearControl* output = nullptr;
        for (const auto& c : model->controls)
        {
            if (c.label == "Power Master") master = &c;
            if (c.label == "Output") output = &c;
        }
        check(master && output && master->moduleOffset < output->moduleOffset,
              id + " separa master de potencia e saida");
        if (!master || !output) continue;
        const auto source = input(48000.0, 4096);
        const auto make = [&](bool powerChange, bool outputChange) {
            ModuleChain chain;
            preset::apply(library.buildSignalChain({id}, "Masters"), chain);
            if (powerChange)
            {
                auto* p = chain.moduleAt(master->moduleOffset).findParameter(master->parameterId);
                p->setValue(p->value() * 0.5f);
            }
            if (outputChange)
            {
                auto* p = chain.moduleAt(output->moduleOffset).findParameter(output->parameterId);
                p->setValue(p->value() * 2.0f);
            }
            chain.prepare(48000.0, 128);
            return render(chain, source, 128);
        };
        const auto base = make(false, false), louder = make(false, true), cleaner = make(true, false);
        float outputError = 0.0f, powerDifference = 0.0f;
        for (std::size_t n = 512; n < base.size(); ++n)
        {
            outputError = std::max(outputError, std::fabs(louder[n] - 2.0f * base[n]));
            powerDifference = std::max(powerDifference, std::fabs(cleaner[n] - 0.5f * base[n]));
        }
        check(outputError < 1e-6f, "Output muda volume sem mudar saturacao: " + id);
        check(powerDifference > 0.001f, "Power Master muda a saturacao: " + id);
    }
}

double toneLevel(const GearLibrary& library, const std::string& id, float amplitude, int mode = -1)
{
    ModuleChain chain;
    preset::apply(library.buildSignalChain({id}, "Dynamics"), chain);
    if (mode >= 0)
    {
        chain.moduleAt(0).findParameter("lead")->setValue(mode == 2 ? 1.0f : 0.0f);
        chain.moduleAt(0).findParameter("clean")->setValue(mode == 0 ? 1.0f : 0.0f);
    }
    chain.prepare(48000.0, 128);
    std::vector<float> source(48000);
    for (std::size_t n = 0; n < source.size(); ++n)
        source[n] = amplitude * static_cast<float>(std::sin(6.283185307179586 * 220.0 * n / 48000.0));
    const auto output = render(chain, source, 128);
    double sum = 0.0;
    for (std::size_t n = 24000; n < output.size(); ++n) sum += output[n] * output[n];
    return std::sqrt(sum / 24000.0);
}

void testUsefulDynamicsAndChannelBalance(const GearLibrary& library)
{
    for (const std::string id : {"amp.brit-plexi", "amp.brit-800", "amp.modern-hi-gain"})
    {
        const double span = 20.0 * std::log10(toneLevel(library, id, 0.3f) / toneLevel(library, id, 0.01f));
        const double minimum = id == "amp.brit-plexi" ? 10.0 : 3.0;
        check(span > minimum, id + " responde ao volume de entrada no padrao");
        check(span < 20.0, id + " ainda comprime como amp saturado");
    }
    const double clean = toneLevel(library, "amp.ironheart-120", 0.08f, 0);
    for (int mode : {1, 2})
    {
        const double channel = toneLevel(library, "amp.ironheart-120", 0.08f, mode);
        check(std::fabs(20.0 * std::log10(channel / clean)) < 3.0,
              "Ironheart troca de canal sem salto grande no nivel de referencia");
    }
}
}

int main()
{
    GearLibrary library;
    for (const auto* amp : library.modelsIn(GearCategory::Amp))
    {
        testCatalogSafety(library, *amp);
        testBlockBoundaries(library, *amp);
    }
    testMasterChangesPowerDrive(library);
    testUsefulDynamicsAndChannelBalance(library);
    return reportResults();
}
