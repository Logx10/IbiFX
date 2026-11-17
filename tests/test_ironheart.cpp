#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "GearLibrary.h"
#include "IronheartMaster.h"
#include "IronheartPreamp.h"
#include "ModuleChain.h"
#include "Preset.h"
#include "Serialization.h"
#include "test_helpers.h"

namespace
{
void set(AudioModule& module, const std::string& id, float value)
{
    auto* parameter = module.findParameter(id);
    check(parameter != nullptr, "parametro existe: " + id);
    if (parameter) parameter->setValue(value);
}

std::vector<float> render(AudioModule& module, double rate = 48000.0, int blockSize = 128,
                          bool impulse = false, int sampleCount = 10240)
{
    module.prepare(rate, blockSize);
    std::vector<float> output;
    std::vector<float> block(static_cast<std::size_t>(blockSize));
    for (int b = 0; b < sampleCount; b += blockSize)
    {
        block.resize(static_cast<std::size_t>(std::min(blockSize, sampleCount - b)));
        for (std::size_t i = 0; i < block.size(); ++i)
        {
            const int n = b + static_cast<int>(i);
            block[i] = impulse ? (n == 0 ? 0.5f : 0.0f)
                : 0.08f * static_cast<float>(std::sin(6.283185307 * 220.0 * n / rate)
                                          + 0.3 * std::sin(6.283185307 * 1760.0 * n / rate));
        }
        module.process(block);
        output.insert(output.end(), block.begin(), block.end());
    }
    return output;
}

float energy(const std::vector<float>& samples)
{
    double sum = 0.0;
    for (float x : samples) sum += static_cast<double>(x) * x;
    return static_cast<float>(sum);
}

double relativeDifference(const std::vector<float>& a, const std::vector<float>& b)
{
    double error = 0.0, reference = 0.0;
    for (std::size_t n = 0; n < a.size(); ++n)
    {
        error += static_cast<double>(a[n] - b[n]) * (a[n] - b[n]);
        reference += static_cast<double>(a[n]) * a[n];
    }
    return std::sqrt(error / std::max(reference, 1e-30));
}

void testChannels()
{
    IronheartPreamp pre;
    const auto clean = render(pre);
    set(pre, "rhythmGain", 1.0f);
    set(pre, "rhythmVolume", 0.0f);
    const double inactiveRhythm = relativeDifference(clean, render(pre));
    check(inactiveRhythm < 1e-6, "Clean nao recebe ganho/volume Rhythm; apenas regularizacao numerica do pot inativo");
    set(pre, "leadGain", 1.0f);
    set(pre, "leadBass", 1.0f);
    const double inactiveLead = relativeDifference(clean, render(pre));
    check(inactiveLead < 1e-6, "Clean nao recebe ganho/EQ Lead; apenas regularizacao numerica do pot inativo");
    set(pre, "rhythmBass", 1.0f);
    check(clean != render(pre), "Clean usa EQ Rhythm");

    set(pre, "clean", 0.0f);
    set(pre, "rhythmVolume", 0.5f);
    const auto rhythm = render(pre);
    set(pre, "cleanVolume", 0.0f);
    const double passiveClean = relativeDifference(rhythm, render(pre));
    // Clean nao multiplica o sinal Rhythm, mas o bright/pot inativo ainda
    // altera levemente a carga passiva comum de V1A.
    check(passiveClean > 1e-8 && passiveClean < 0.001, "Clean Volume so tem efeito passivo pequeno sobre Rhythm");
    std::cout << "  erro relativo de controles inativos: Rhythm=" << inactiveRhythm
              << " Lead=" << inactiveLead << "; carga Clean sobre Rhythm=" << passiveClean << '\n';
    set(pre, "lead", 1.0f);
    const auto lead = render(pre);
    check(lead != rhythm && lead != clean, "tres modos produzem respostas diferentes");
    set(pre, "clean", 1.0f);
    check(lead == render(pre), "Lead tem prioridade sobre Clean/Rhythm");
    set(pre, "leadVolume", 0.0f);
    check(energy(render(pre)) == 0.0f, "Lead Volume zero silencia o canal");
    set(pre, "lead", 0.0f);
    check(energy(render(pre)) == 0.0f, "Clean Volume zero silencia Clean");
}

void testControls()
{
    IronheartPreamp pre;
    const auto dry = render(pre);
    set(pre, "boost", 1.0f);
    check(dry == render(pre), "Boost desligado ignora amount");
    set(pre, "boostEnabled", 1.0f);
    check(energy(render(pre)) > energy(dry), "Pre-Boost empurra a entrada");

    for (const std::string prefix : {"rhythm", "lead"})
    {
        IronheartPreamp channel;
        set(channel, "lead", prefix == "lead" ? 1.0f : 0.0f);
        set(channel, "clean", 0.0f);
        const auto normal = render(channel);
        for (const char* suffix : {"Deep", "MidShift", "TrebleShift"})
        {
            set(channel, prefix + suffix, 1.0f);
            check(render(channel) != normal, prefix + suffix + " altera a resposta");
            set(channel, prefix + suffix, 0.0f);
        }
        for (const char* suffix : {"Gain", "Bass", "Mid", "Treble", "Volume"})
        {
            set(channel, prefix + suffix, 0.9f);
            check(render(channel) != normal, prefix + suffix + " altera audio");
            set(channel, prefix + suffix, 0.5f);
        }
    }
}

void testMaster()
{
    IronheartMaster master;
    set(master, "reverbEnabled", 0.0f);
    const auto normal = render(master);
    set(master, "watts", 0.25f);
    const auto quiet = render(master);
    check(std::fabs(energy(quiet) / energy(normal) - 0.25f) < 1e-4f,
          "Watts atenua depois da saturacao sem mudar seu drive");
    for (const char* mute : {"watts", "power", "play"})
    {
        IronheartMaster muted;
        set(muted, mute, 0.0f);
        check(energy(render(muted)) == 0.0f, std::string(mute) + " silencia");
    }
    for (const char* id : {"dynamics", "tone"})
    {
        IronheartMaster changed;
        set(changed, "reverbEnabled", 0.0f);
        set(changed, id, 0.9f);
        check(render(changed) != normal, std::string(id) + " altera audio");
    }
    IronheartMaster wet;
    set(wet, "reverb", 1.0f);
    set(wet, "reverbEnabled", 0.0f);
    const auto dryImpulse = render(wet, 48000, 128, true);
    set(wet, "reverbEnabled", 1.0f);
    const auto wetImpulse = render(wet, 48000, 128, true);
    check(wetImpulse != dryImpulse, "Reverb On habilita ambiente");
    check(energy(std::vector<float>(wetImpulse.begin() + 4000, wetImpulse.end())) > 1e-10f,
          "reverb produz cauda apos impulso");
}

void testLifecycle()
{
    for (double rate : {22050.0, 44100.0, 48000.0, 96000.0})
    {
        IronheartPreamp pre;
        for (std::size_t p = 0; p < pre.parameterCount(); ++p) pre.parameterAt(p).setNormalized(1.0f);
        const auto output = render(pre, rate, 17);
        check(std::all_of(output.begin(), output.end(), [](float x) { return std::isfinite(x) && std::fabs(x) < 10.0f; }),
              "pre estavel em extremos: " + std::to_string(rate));
        pre.reset();
        std::vector<float> silence(127, 0.0f);
        pre.process(silence);
        check(energy(silence) == 0.0f, "reset limpa filtros e oversampling");
        std::vector<float> empty;
        pre.process(empty);
        check(empty.empty(), "buffer vazio aceito");

        IronheartMaster master;
        for (std::size_t p = 0; p < master.parameterCount(); ++p) master.parameterAt(p).setNormalized(1.0f);
        const auto tail = render(master, rate, 17);
        check(std::all_of(tail.begin(), tail.end(), [](float x) { return std::isfinite(x) && std::fabs(x) < 10.0f; }),
              "master estavel em extremos");
        master.reset();
        master.process(silence);
        check(energy(silence) == 0.0f, "reset limpa reverb e potencia");
    }
    IronheartPreamp one, chunked;
    check(render(one, 48000, 1, false, 4096) == render(chunked, 48000, 128, false, 4096),
          "audio independe do tamanho de bloco");
    set(one, "boostEnabled", 1.0f);
    set(chunked, "boostEnabled", 1.0f);
    set(one, "boost", 1.0f);
    set(chunked, "boost", 1.0f);
    check(render(one, 48000, 1, false, 4096) == render(chunked, 48000, 128, false, 4096),
          "rede de entrada com boost independe do tamanho de bloco");
}

void testPresets()
{
    GearLibrary library;
    const auto* amp = library.find("amp.ironheart-120");
    check(amp != nullptr, "Ironheart disponivel no catalogo");
    if (!amp) return;
    check(amp->controls.size() == 28, "painel expoe todos os 28 controles");
    ModuleChain chain;
    preset::apply(library.buildSignalChain({amp->id, "rack.output-limiter"}, "Ironheart"), chain);
    check(chain.size() == 3, "pre + master + limiter");
    for (std::size_t p = 0; p < chain.moduleAt(0).parameterCount(); ++p)
        chain.moduleAt(0).parameterAt(p).setNormalized(0.73f);
    for (std::size_t p = 0; p < chain.moduleAt(1).parameterCount(); ++p)
        chain.moduleAt(1).parameterAt(p).setNormalized(0.27f);
    const auto saved = preset::serialize(preset::capture("Ironheart", chain));
    ModuleChain restored;
    preset::apply(preset::deserialize(saved), restored);
    check(preset::serialize(preset::capture("Ironheart", restored)) == saved,
          "todos os controles sobrevivem ao round trip");
    const auto loop = library.buildSignalChain({"amp.ironheart-preamp", "rack.digital-delay",
                                               "rack.ironheart-master"}, "FX loop");
    check(loop.modules[0].type == "IronheartPreamp" && loop.modules[1].type == "Delay"
          && loop.modules[2].type == "IronheartMaster", "efeito pode ficar entre pre e potencia");

    for (int mode = 0; mode < 3; ++mode)
    {
        ModuleChain playable;
        preset::apply(library.buildSignalChain({amp->id, "rack.output-limiter"}, "Ironheart"), playable);
        playable.moduleAt(0).findParameter("lead")->setValue(mode == 2 ? 1.0f : 0.0f);
        playable.moduleAt(0).findParameter("clean")->setValue(mode == 0 ? 1.0f : 0.0f);
        playable.prepare(48000, 128);
        std::vector<float> signal(128);
        double sum = 0.0;
        for (int b = 0; b < 80; ++b)
        {
            for (std::size_t i = 0; i < signal.size(); ++i)
                signal[i] = 0.08f * static_cast<float>(std::sin(6.283185307 * 220.0 * (b * 128 + i) / 48000.0));
            playable.process(signal);
            sum += energy(signal);
        }
        const double rms = std::sqrt(sum / 10240.0);
        std::cout << "  modo " << mode << " RMS=" << rms << '\n';
        check(rms > 0.005 && rms < 0.5, "nivel inicial audivel e controlado");
    }
}

void testAutomatedBlockBoundaries()
{
    const auto run = [](int blockSize) {
        IronheartPreamp pre;
        pre.prepare(48000, blockSize);
        std::vector<float> output, block;
        for (int event = 0; event < 4; ++event)
        {
            set(pre, "lead", event == 1 ? 1.0f : 0.0f);
            set(pre, "clean", event == 0 || event == 3 ? 1.0f : 0.0f);
            set(pre, "leadGain", event % 2 == 0 ? 0.15f : 0.85f);
            set(pre, "rhythmGain", event % 2 == 0 ? 0.8f : 0.2f);
            set(pre, "cleanVolume", event % 2 == 0 ? 0.25f : 0.75f);
            for (const std::string prefix : {"lead", "rhythm"})
            {
                set(pre, prefix + "Bass", event % 2 == 0 ? .1f : .9f);
                set(pre, prefix + "Mid", event % 2 == 0 ? .8f : .2f);
                set(pre, prefix + "Treble", event % 2 == 0 ? .2f : .8f);
                set(pre, prefix + "Deep", event % 2 == 0 ? 1.0f : 0.0f);
                set(pre, prefix + "MidShift", event % 3 == 0 ? 1.0f : 0.0f);
                set(pre, prefix + "TrebleShift", event % 2 == 0 ? 0.0f : 1.0f);
            }
            for (int start = 0; start < 2048; start += blockSize)
            {
                block.resize(static_cast<std::size_t>(blockSize));
                for (int i = 0; i < blockSize; ++i)
                    block[static_cast<std::size_t>(i)] = 0.1f * static_cast<float>(
                        std::sin(6.283185307 * 220.0 * (event * 2048 + start + i) / 48000.0));
                pre.process(block);
                output.insert(output.end(), block.begin(), block.end());
            }
        }
        return output;
    };
    check(run(1) == run(128), "automacao de ganho/EQ/pulls/canais independe da particao de blocos");
}
}

int main()
{
    testChannels();
    testControls();
    testMaster();
    testLifecycle();
    testPresets();
    testAutomatedBlockBoundaries();
    return reportResults();
}
