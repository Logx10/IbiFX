#include <cmath>
#include <iostream>
#include <vector>

#include "GearLibrary.h"
#include "ModuleChain.h"
#include "Preset.h"
#include "Rotovibe.h"
#include "test_helpers.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;

std::vector<float> sineWave(std::size_t count)
{
    std::vector<float> samples(count);
    for (std::size_t i = 0; i < count; ++i)
        samples[i] = 0.5f * static_cast<float>(std::sin(kTwoPi * 440.0 * i / kSampleRate));
    return samples;
}

void set(Rotovibe& effect, const char* id, float value)
{
    effect.findParameter(id)->setValue(value);
}
}

void testModesAndTreadle()
{
    Rotovibe chorus;
    Rotovibe vibrato;
    set(chorus, "mode", 0.0f);
    set(vibrato, "mode", 1.0f);
    chorus.prepare(kSampleRate, 128);
    vibrato.prepare(kSampleRate, 128);

    auto dry = sineWave(24000);
    auto mixed = dry;
    auto wet = dry;
    chorus.process(mixed);
    vibrato.process(wet);

    bool modesCorrect = true;
    for (std::size_t i = 0; i < dry.size(); ++i)
        modesCorrect &= std::fabs(mixed[i] - 0.5f * (dry[i] + wet[i])) < 1e-5f;
    check(modesCorrect, "Chorus mistura dry/wet; Vibrato usa somente wet");

    Rotovibe fast;
    set(fast, "position", 1.0f);
    fast.prepare(kSampleRate, 128);
    auto fastOutput = dry;
    fast.process(fastOutput);
    double difference = 0.0;
    for (std::size_t i = 5000; i < dry.size(); ++i)
        difference += std::fabs(mixed[i] - fastOutput[i]);
    check(difference / 19000.0 > 0.01, "prancha altera a velocidade da modulação");
}

void testResetAndLiveMovement()
{
    Rotovibe effect;
    effect.prepare(kSampleRate, 128);
    auto input = sineWave(4096);
    auto first = input;
    effect.process(first);
    effect.reset();
    auto second = input;
    effect.process(second);
    check(first == second, "reset restaura fase e memória dos filtros");

    set(effect, "position", 1.0f);
    set(effect, "intensity", 1.0f);
    set(effect, "mode", 1.0f);
    auto moved = sineWave(4096);
    effect.process(moved);
    bool finite = true;
    for (float sample : moved)
        finite &= std::isfinite(sample) && std::fabs(sample) < 2.0f;
    check(finite, "movimento em tempo real gera saída finita e limitada");
}

void testCatalogAndPreset()
{
    GearLibrary library;
    const GearModel* model = library.find("stomp.rotovibe");
    check(model != nullptr, "Rotovibe aparece no catálogo");
    if (model == nullptr)
        return;

    Rig rig;
    rig.stomps.push_back(model->id);
    ModuleChain chain;
    preset::apply(library.buildPreset(rig, "Vibe"), chain);
    check(chain.moduleAt(0).name() == std::string("Rotovibe"), "rig cria o módulo Rotovibe");
    check(chain.moduleAt(0).findParameter("position") != nullptr, "prancha é parâmetro do preset e MIDI");
    const Preset saved = preset::capture("Vibe", chain);
    ModuleChain restored;
    preset::apply(saved, restored);
    check(restored.moduleAt(0).name() == std::string("Rotovibe"), "preset restaura o módulo");
}

int main()
{
    std::cout << "\n=== testes do Rotovibe ===\n\n";
    testModesAndTreadle();
    testResetAndLiveMovement();
    testCatalogAndPreset();
    return reportResults();
}
