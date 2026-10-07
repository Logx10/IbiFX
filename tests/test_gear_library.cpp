// Testes da GearLibrary.
//
// O catálogo é dado escrito à mão, e preset::apply() IGNORA em silêncio um
// parâmetro que o módulo não reconhece (ver Preset.cpp). Um "drvie" digitado
// errado numa receita de ampli passaria despercebido para sempre — por isso
// o teste mais importante aqui confere cada parâmetro de cada equipamento
// contra o módulo de verdade.
//
// A infra de verificação vive em test_helpers.h.

#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "GearLibrary.h"
#include "ModuleChain.h"
#include "Preset.h"
#include "WavFile.h"
#include "test_helpers.h"

namespace
{
std::filesystem::path tempDir()
{
    return std::filesystem::temp_directory_path() / "ibifx_test_gear_irs";
}

void writeIr(const std::filesystem::path& path)
{
    WavFile file;
    file.sampleRate = 48000.0;
    file.channels = {{1.0f, 0.5f, 0.25f}};
    wav::write(path.string(), file);
}

bool throws(const GearLibrary& library, const Rig& rig)
{
    try
    {
        library.buildPreset(rig, "x");
    }
    catch (const std::runtime_error&)
    {
        return true;
    }
    return false;
}
}

// ---------------------------------------------------------------------

void testBuiltInCatalogHasEveryCategoryButCabinet()
{
    std::cout << "catalogo embutido tem stomps, amps e rack\n";

    GearLibrary library;

    check(!library.modelsIn(GearCategory::Stomp).empty(), "ha stomps");
    check(!library.modelsIn(GearCategory::Amp).empty(), "ha amps");
    check(!library.modelsIn(GearCategory::Rack).empty(), "ha rack");
    check(library.modelsIn(GearCategory::Cabinet).empty(), "sem cabinets antes do scan");
}

void testIdsAreUnique()
{
    std::cout << "ids sao unicos\n";

    GearLibrary library;
    std::set<std::string> seen;
    bool unique = true;

    for (const GearModel& model : library.models())
    {
        if (!seen.insert(model.id).second)
        {
            std::cout << "  repetido: " << model.id << "\n";
            unique = false;
        }
    }

    check(unique, "nenhum id repetido");
}

// Cada parâmetro de cada receita precisa existir no módulo e caber na faixa
// dele — senão o valor seria ignorado ou cortado na borda sem aviso.
void testEveryRecipeParameterExistsAndFitsItsRange()
{
    std::cout << "toda receita usa parametros validos\n";

    GearLibrary library;
    bool allValid = true;

    for (const GearModel& model : library.models())
    {
        Preset single;
        single.modules = model.modules;

        ModuleChain chain;
        preset::apply(single, chain);

        for (std::size_t m = 0; m < model.modules.size(); ++m)
        {
            const AudioModule& audioModule = chain.moduleAt(m);

            for (const auto& [id, value] : model.modules[m].parameters)
            {
                const Parameter* parameter = audioModule.findParameter(id);
                if (parameter == nullptr)
                {
                    std::cout << "  " << model.id << ": " << audioModule.name()
                              << " nao tem '" << id << "'\n";
                    allValid = false;
                }
                else if (value < parameter->minValue() || value > parameter->maxValue())
                {
                    std::cout << "  " << model.id << ": " << id << " = " << value
                              << " fora da faixa\n";
                    allValid = false;
                }
            }
        }
    }

    check(allValid, "todos os parametros existem e estao dentro da faixa");
}

// Cada knob de painel precisa apontar para um módulo que existe na receita,
// um parâmetro que esse módulo tem, e uma faixa que cabe na dele — e o
// valor da receita precisa estar dentro da faixa do knob, senão o painel
// nasceria com o knob "fora do batente".
void testEveryControlPointsToARealParameter()
{
    std::cout << "todo knob de painel aponta para um parametro real\n";

    GearLibrary library;
    bool allValid = true;
    bool everyModelHasControls = true;

    for (const GearModel& model : library.models())
    {
        if (model.controls.empty())
            everyModelHasControls = false;

        Preset single;
        single.modules = model.modules;
        ModuleChain chain;
        preset::apply(single, chain);

        for (const GearControl& control : model.controls)
        {
            if (control.moduleOffset >= chain.size())
            {
                std::cout << "  " << model.id << ": " << control.label << " fora da receita\n";
                allValid = false;
                continue;
            }

            const Parameter* parameter = chain.moduleAt(control.moduleOffset).findParameter(control.parameterId);
            if (parameter == nullptr
                || control.minValue < parameter->minValue()
                || control.maxValue > parameter->maxValue()
                || control.minValue >= control.maxValue
                || parameter->value() < control.minValue
                || parameter->value() > control.maxValue)
            {
                std::cout << "  " << model.id << ": " << control.label << " invalido\n";
                allValid = false;
            }
        }
    }

    check(everyModelHasControls, "todo equipamento tem painel");
    check(allValid, "todos os knobs sao validos");
}

void testRigOrderIsStompAmpCabinetRackLimiter()
{
    std::cout << "rig segue a ordem stomp -> amp -> cabinet -> rack -> limiter\n";

    GearLibrary library;

    Rig rig;
    rig.stomps = {"stomp.gate", "stomp.yellow-drive"};
    rig.amp = "amp.brit-800";
    rig.rack = {"rack.hall"};

    const Preset result = library.buildPreset(rig, "Teste");

    check(result.name == "Teste", "nome do preset");
    check(result.modules.front().type == "NoiseGate", "primeiro e o gate");
    check(result.modules[1].type == "SoftClipper", "depois o drive");
    check(result.modules[result.modules.size() - 2].type == "Reverb", "rack logo antes do limiter");
    check(result.modules.back().type == "Limiter", "sempre termina no limiter");

    bool sawPowerAmp = false;
    for (const Preset::ModuleState& state : result.modules)
        sawPowerAmp = sawPowerAmp || state.type == "PowerAmp";
    check(sawPowerAmp, "o amp entrou na cadeia");

    ModuleChain chain;
    preset::apply(result, chain);
    check(chain.size() == result.modules.size(), "o preset aplica numa ModuleChain");
}

// A UI agrupa os pedais da tela por equipamento a partir desta lista — ela
// precisa bater, módulo a módulo, com o que buildPreset() monta.
void testRigModelsMatchBuildPreset()
{
    std::cout << "rigModels() bate com os modulos de buildPreset()\n";

    GearLibrary library;

    Rig rig;
    rig.stomps = {"stomp.clean-boost"};
    rig.amp = "amp.tweed-59";
    rig.rack = {"rack.slapback", "rack.room"};

    const std::vector<const GearModel*> models = library.rigModels(rig);
    check(models.size() == 4, "4 equipamentos");
    check(models[1]->id == "amp.tweed-59", "amp depois dos stomps");

    std::size_t total = 0;
    for (const GearModel* model : models)
        total += model->modules.size();

    check(total + 1 == library.buildPreset(rig, "x").modules.size(),
          "soma dos modulos + limiter = tamanho do preset");
}

void testEmptyRigIsJustTheLimiter()
{
    std::cout << "rig vazio vira so o limiter\n";

    GearLibrary library;
    const Preset result = library.buildPreset(Rig{}, "Vazio");

    check(result.modules.size() == 1, "um modulo");
    check(result.modules.front().type == "Limiter", "o limiter");
}

void testUnknownOrMisplacedGearThrows()
{
    std::cout << "id desconhecido ou na categoria errada lanca\n";

    GearLibrary library;

    Rig unknown;
    unknown.amp = "amp.nao-existe";
    check(throws(library, unknown), "amp desconhecido lanca");

    Rig misplaced;
    misplaced.stomps = {"amp.brit-800"};
    check(throws(library, misplaced), "amp na lista de stomps lanca");
}

void testScanFindsCabinetsAndMicrophones()
{
    std::cout << "scan encontra cabinets (pastas) e microfones (.wav)\n";

    const std::filesystem::path root = tempDir();
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "4x12 Brit");
    std::filesystem::create_directories(root / "1x12 Open");

    writeIr(root / "4x12 Brit" / "SM57 On-Axis.wav");
    writeIr(root / "4x12 Brit" / "Ribbon.WAV");
    writeIr(root / "1x12 Open" / "Condenser.wav");
    {
        WavFile notAnIr;
        notAnIr.sampleRate = 48000.0;
        notAnIr.channels = {{0.0f}};
        wav::write((root / "leia-me.wav").string(), notAnIr);
    }

    GearLibrary library;
    const std::size_t found = library.scanImpulseResponses(root.string());

    check(found == 3, "3 IRs encontradas (o .wav solto na raiz nao conta)");

    const GearModel* sm57 = library.find("cab.4x12-brit.sm57-on-axis");
    check(sm57 != nullptr, "id montado a partir de pasta + arquivo");
    if (sm57 != nullptr)
    {
        check(sm57->name == "4x12 Brit", "nome do cabinet = nome da pasta");
        check(sm57->microphone == "SM57 On-Axis", "microfone = nome do arquivo");
        check(sm57->modules.front().type == "Cabinet", "vira um modulo Cabinet");
        check(sm57->controls.size() == 1 && sm57->controls[0].parameterId == "mix", "painel com o mix");
    }

    Rig rig;
    rig.amp = "amp.tweed-59";
    rig.cabinet = "cab.1x12-open.condenser";
    ModuleChain chain;
    preset::apply(library.buildPreset(rig, "Com cab"), chain);
    check(chain.moduleAt(chain.size() - 2).name() == std::string("Cabinet"),
          "a IR carrega de verdade ao aplicar");

    check(library.scanImpulseResponses(root.string()) == 3, "re-scan nao duplica");
    check(library.modelsIn(GearCategory::Cabinet).size() == 3, "continua com 3 cabinets");

    std::filesystem::remove_all(root);
}

bool sameRig(const Rig& a, const Rig& b)
{
    return a.stomps == b.stomps && a.amp == b.amp && a.cabinet == b.cabinet && a.rack == b.rack;
}

void testInferRigRecoversTheRigOfASavedChain()
{
    std::cout << "inferRig reconhece o rig de uma cadeia salva, mesmo com knobs mexidos\n";

    const std::filesystem::path root = tempDir();
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "4x12 Brit");
    writeIr(root / "4x12 Brit" / "SM57.wav");

    GearLibrary library;
    library.scanImpulseResponses(root.string());

    Rig original;
    original.stomps = {"stomp.orange-squeeze", "stomp.yellow-drive"};
    original.amp = "amp.brit-plexi";
    original.cabinet = "cab.4x12-brit.sm57";
    original.rack = {"rack.room"};

    // Como a janela salvaria depois de girar knobs: mesma estrutura,
    // valores e bypass diferentes da receita.
    Preset saved = library.buildPreset(original, "Interstate");
    for (Preset::ModuleState& state : saved.modules)
    {
        for (auto& [id, value] : state.parameters)
            value *= 0.9f;
    }
    saved.modules.back().bypassed = true;

    Rig inferred;
    check(library.inferRig(saved, inferred), "deduz um rig");
    check(sameRig(inferred, original), "o mesmo rig que montou a cadeia");
    check(library.matchesRig(saved, inferred), "e a estrutura bate com ele");

    std::filesystem::remove_all(root);
}

void testInferRigPicksTheClosestOfTwoLookalikes()
{
    std::cout << "inferRig desempata receitas de mesma estrutura pelos valores\n";

    GearLibrary library;

    // Room e Hall são os dois um Reverb só — a estrutura não separa os
    // dois, os valores sim.
    for (const char* rackId : {"rack.room", "rack.hall"})
    {
        Rig original;
        original.amp = "amp.american-clean";
        original.rack = {rackId};

        Rig inferred;
        check(library.inferRig(library.buildPreset(original, "x"), inferred), "deduz um rig");
        check(inferred.rack.size() == 1 && inferred.rack[0] == rackId,
              std::string("reconhece ") + rackId);
    }
}

void testInferRigRejectsAChainThatIsNotARig()
{
    std::cout << "inferRig recusa cadeia que nao veio de um rig\n";

    GearLibrary library;
    Rig rig;

    Preset noLimiter;
    noLimiter.modules.push_back({"Delay", false, {}, ""});
    check(!library.inferRig(noLimiter, rig), "sem o Limiter no fim");

    Preset onlyLimiter;
    onlyLimiter.modules.push_back({"Limiter", false, {}, ""});
    check(!library.inferRig(onlyLimiter, rig), "so o Limiter nao e um rig");

    Rig stomp;
    stomp.stomps = {"stomp.gate"};
    check(!library.matchesRig(onlyLimiter, stomp), "estrutura diferente nao bate");

    Rig unknown;
    unknown.amp = "amp.nao-existe";
    check(!library.matchesRig(library.buildPreset(Rig{}, "x"), unknown), "id desconhecido nao bate");
}

void testScanOfMissingDirectoryFindsNothing()
{
    std::cout << "scan de pasta inexistente nao e erro\n";

    GearLibrary library;
    check(library.scanImpulseResponses((tempDir() / "nao-existe").string()) == 0,
          "nenhum cabinet");
}

void testOnlyBritAmpsHavePresence()
{
    std::cout << "so os Brit tem o knob de Presence\n";

    GearLibrary library;
    for (const GearModel* amp : library.modelsIn(GearCategory::Amp))
    {
        bool hasPresence = false;
        for (const GearControl& control : amp->controls)
            hasPresence = hasPresence || control.parameterId == "presence";

        const bool isBrit = (amp->id == "amp.brit-plexi" || amp->id == "amp.brit-800");
        check(hasPresence == isBrit, amp->id + (isBrit ? " tem Presence" : " nao tem Presence"));
    }
}
int main()
{
    std::cout << "\n=== testes da GearLibrary ===\n\n";

    testBuiltInCatalogHasEveryCategoryButCabinet();
    testIdsAreUnique();
    testEveryRecipeParameterExistsAndFitsItsRange();
    testEveryControlPointsToARealParameter();
    testRigOrderIsStompAmpCabinetRackLimiter();
    testRigModelsMatchBuildPreset();
    testEmptyRigIsJustTheLimiter();
    testUnknownOrMisplacedGearThrows();
    testScanFindsCabinetsAndMicrophones();
    testScanOfMissingDirectoryFindsNothing();
    testInferRigRecoversTheRigOfASavedChain();
    testInferRigPicksTheClosestOfTwoLookalikes();
    testInferRigRejectsAChainThatIsNotARig();
    testOnlyBritAmpsHavePresence();

    return reportResults();
}
