// Testes do Preset, Serialization e PresetManager.
//
// Cobrem as três voltas possíveis: cadeia -> Preset -> cadeia (capture/
// apply), Preset -> texto -> Preset (serialize/deserialize) e Preset ->
// arquivo -> Preset (save/load). PresetManager grava de verdade num
// diretório temporário, pelo mesmo motivo do test_wav_file.cpp: exercitar a
// ida ao disco, não só a montagem do texto em memória.
//
// A infra de verificação vive em test_helpers.h.

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "Cabinet.h"
#include "Delay.h"
#include "GainProcessor.h"
#include "ModuleChain.h"
#include "Preset.h"
#include "PresetManager.h"
#include "Serialization.h"
#include "SoftClipper.h"
#include "WavFile.h"
#include "test_helpers.h"

namespace
{
std::filesystem::path tempPath(const std::string& name)
{
    return std::filesystem::temp_directory_path() / ("ibifx_test_preset_" + name);
}

void removeIfExists(const std::filesystem::path& path)
{
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

// Uma cadeia pequena, com um módulo em bypass, para exercitar os três
// campos que um preset precisa guardar: tipo, bypass e parâmetros.
void buildSampleChain(ModuleChain& chain)
{
    auto gain = std::make_unique<GainProcessor>();
    gain->setGain(3.5f);
    chain.add(std::move(gain));

    auto drive = std::make_unique<SoftClipper>();
    drive->setDrive(12.0f);
    chain.add(std::move(drive));
    chain.setBypassed(1, true);

    auto delay = std::make_unique<Delay>();
    delay->setTime(0.4f);
    delay->setFeedback(0.6f);
    delay->setMix(0.25f);
    chain.add(std::move(delay));
}
}

// ---------------------------------------------------------------------

// capture() lê tipo, bypass e parâmetros de cada posição, na ordem certa.
void testCaptureReadsChainState()
{
    std::cout << "capture le tipo, bypass e parametros\n";

    ModuleChain chain;
    buildSampleChain(chain);

    const Preset p = preset::capture("Teste", chain);

    check(p.name == "Teste", "nome preservado");
    check(p.modules.size() == 3, "3 modulos capturados");

    check(p.modules[0].type == "Gain", "modulo 0 e Gain");
    check(!p.modules[0].bypassed, "modulo 0 nao esta em bypass");
    check(p.modules[0].parameters.size() == 1, "Gain tem 1 parametro");
    checkClose(p.modules[0].parameters[0].second, 3.5f, "gain capturado e 3.5");

    check(p.modules[1].type == "SoftClipper", "modulo 1 e SoftClipper");
    check(p.modules[1].bypassed, "modulo 1 esta em bypass");

    check(p.modules[2].type == "Delay", "modulo 2 e Delay");
    check(p.modules[2].parameters.size() == 3, "Delay tem 3 parametros");
}

// apply() reconstrói a cadeia inteira a partir do preset — tipo, ordem,
// bypass e parâmetros, inclusive depois de a cadeia já ter outra coisa
// dentro (clear() entra antes de reconstruir).
void testApplyRebuildsChain()
{
    std::cout << "apply reconstroi a cadeia\n";

    ModuleChain original;
    buildSampleChain(original);
    const Preset p = preset::capture("Teste", original);

    ModuleChain rebuilt;
    rebuilt.add(std::make_unique<GainProcessor>());
    preset::apply(p, rebuilt);

    check(rebuilt.size() == 3, "cadeia reconstruida tem 3 modulos");
    check(std::string(rebuilt.moduleAt(0).name()) == "Gain", "posicao 0 e Gain");
    check(std::string(rebuilt.moduleAt(1).name()) == "SoftClipper", "posicao 1 e SoftClipper");
    check(std::string(rebuilt.moduleAt(2).name()) == "Delay", "posicao 2 e Delay");

    check(rebuilt.isBypassed(1), "SoftClipper volta em bypass");
    check(!rebuilt.isBypassed(0), "Gain nao volta em bypass");

    checkClose(rebuilt.moduleAt(0).findParameter("gain")->value(), 3.5f, "gain restaurado");
    checkClose(rebuilt.moduleAt(2).findParameter("time")->value(), 0.4f, "delay time restaurado");
    checkClose(rebuilt.moduleAt(2).findParameter("feedback")->value(), 0.6f, "delay feedback restaurado");
}

// O caminho da IR do Cabinet não é um Parameter, e precisa sobreviver à
// mesma volta cadeia -> preset -> cadeia.
void testCabinetIrPathRoundTrips()
{
    std::cout << "ir do cabinet sobrevive ao round trip\n";

    const std::filesystem::path irPath = tempPath("ir.wav");

    WavFile ir;
    ir.sampleRate = 44100.0;
    ir.channels = {{1.0f, 0.5f}};
    wav::write(irPath.string(), ir);

    ModuleChain original;
    auto cabinet = std::make_unique<Cabinet>();
    cabinet->loadImpulseResponseFile(irPath.string());
    cabinet->setMix(0.7f);
    original.add(std::move(cabinet));

    const Preset p = preset::capture("Com cabinet", original);
    check(p.modules[0].irPath == irPath.string(), "irPath capturado");

    ModuleChain rebuilt;
    preset::apply(p, rebuilt);

    const auto& cabinetRebuilt = static_cast<const Cabinet&>(rebuilt.moduleAt(0));
    check(cabinetRebuilt.irPath() == irPath.string(), "irPath restaurado");
    checkClose(cabinetRebuilt.mix(), 0.7f, "mix do cabinet restaurado");

    removeIfExists(irPath);
}

// apply() lança para um tipo que não existe, em vez de montar uma cadeia
// incompleta em silêncio.
void testApplyThrowsOnUnknownType()
{
    std::cout << "apply lanca em tipo desconhecido\n";

    Preset p;
    p.name = "Invalido";
    Preset::ModuleState state;
    state.type = "ModuloQueNaoExiste";
    p.modules.push_back(state);

    ModuleChain chain;
    bool lancou = false;

    try
    {
        preset::apply(p, chain);
    }
    catch (const std::runtime_error&)
    {
        lancou = true;
    }

    check(lancou, "tipo desconhecido lanca runtime_error");
}

// serialize()/deserialize() não perdem nada: nome, ordem, tipos, bypass,
// parâmetros e a IR do cabinet.
void testSerializeRoundTrip()
{
    std::cout << "serialize/deserialize preservam o preset\n";

    ModuleChain chain;
    buildSampleChain(chain);
    const Preset original = preset::capture("Lead Classico", chain);

    const std::string text = preset::serialize(original);
    const Preset restored = preset::deserialize(text);

    check(restored.name == "Lead Classico", "nome preservado no texto");
    check(restored.modules.size() == original.modules.size(), "mesma quantidade de modulos");
    check(restored.modules[1].type == "SoftClipper", "tipo preservado");
    check(restored.modules[1].bypassed, "bypass preservado");

    checkClose(restored.modules[2].parameters[0].second,
               original.modules[2].parameters[0].second,
               "parametro do delay preservado");
}

// Uma linha fora do formato (module/param/ir malformados, ou palavra-chave
// desconhecida) lança, em vez de ser ignorada.
void testDeserializeThrowsOnMalformedText()
{
    std::cout << "deserialize lanca em texto malformado\n";

    bool lancouLinhaDesconhecida = false;
    try
    {
        preset::deserialize("preset X\nisto nao e uma linha valida\n");
    }
    catch (const std::runtime_error&)
    {
        lancouLinhaDesconhecida = true;
    }
    check(lancouLinhaDesconhecida, "linha desconhecida lanca");

    bool lancouParamSemModulo = false;
    try
    {
        preset::deserialize("preset X\nparam gain 1.0\n");
    }
    catch (const std::runtime_error&)
    {
        lancouParamSemModulo = true;
    }
    check(lancouParamSemModulo, "'param' antes de 'module' lanca");

    bool lancouModuleSemBypass = false;
    try
    {
        preset::deserialize("preset X\nmodule Gain\n");
    }
    catch (const std::runtime_error&)
    {
        lancouModuleSemBypass = true;
    }
    check(lancouModuleSemBypass, "'module' sem bypass lanca");
}

// save()/load() fecham o ciclo completo: cadeia -> preset -> arquivo ->
// preset -> cadeia.
void testPresetManagerRoundTripsThroughDisk()
{
    std::cout << "save/load fecham o ciclo pelo disco\n";

    const std::filesystem::path path = tempPath("preset.ibifx");

    ModuleChain chain;
    buildSampleChain(chain);
    const Preset original = preset::capture("Disco", chain);

    preset::save(original, path.string());
    const Preset loaded = preset::load(path.string());

    check(loaded.name == "Disco", "nome sobreviveu ao disco");
    check(loaded.modules.size() == 3, "3 modulos sobreviveram ao disco");

    ModuleChain rebuilt;
    preset::apply(loaded, rebuilt);

    check(rebuilt.size() == 3, "cadeia reconstruida do arquivo tem 3 modulos");
    check(rebuilt.isBypassed(1), "bypass sobreviveu ao disco");
    checkClose(rebuilt.moduleAt(1).findParameter("drive")->value(), 12.0f, "drive sobreviveu ao disco");

    removeIfExists(path);
}

// Arquivo inexistente lança com mensagem clara, igual a wav::read.
void testLoadMissingFileThrows()
{
    std::cout << "load de arquivo inexistente\n";

    bool lancou = false;
    try
    {
        preset::load("/definitivamente/nao/existe/preset.ibifx");
    }
    catch (const std::runtime_error&)
    {
        lancou = true;
    }

    check(lancou, "load de arquivo inexistente lanca");
}

int main()
{
    std::cout << "\n=== testes do Preset ===\n\n";

    testCaptureReadsChainState();
    testApplyRebuildsChain();
    testCabinetIrPathRoundTrips();
    testApplyThrowsOnUnknownType();
    testSerializeRoundTrip();
    testDeserializeThrowsOnMalformedText();
    testPresetManagerRoundTripsThroughDisk();
    testLoadMissingFileThrows();

    return reportResults();
}
