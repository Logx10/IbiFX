// Testes da GearLibrary.
//
// O catálogo é dado escrito à mão, e preset::apply() IGNORA em silêncio um
// parâmetro que o módulo não reconhece (ver Preset.cpp). Um "drvie" digitado
// errado numa receita de ampli passaria despercebido para sempre — por isso
// o teste mais importante aqui confere cada parâmetro de cada equipamento
// contra o módulo de verdade.
//
// A infra de verificação vive em test_helpers.h.

#include <algorithm>
#include <array>
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
#include "Serialization.h"
#include "WavFile.h"
#include "offline.h"
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
        // O Merge é a única exceção: só junta os caminhos, não tem o que
        // ajustar (o volume de cada caminho mora no marcador que o abre).
        if (model.controls.empty() && model.id != "route.merge")
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

// A cadeia livre não tem ordem por categoria: amp depois de cabinet, o
// mesmo pedal duas vezes e nenhum Limiter escondido no fim.
void testSignalChainKeepsAnyOrderAndDuplicates()
{
    std::cout << "cadeia livre aceita qualquer ordem e duplicatas\n";

    GearLibrary library;
    const GearModel* drive = library.find("stomp.yellow-drive");
    const GearModel* amp = library.find("amp.brit-800");
    const GearModel* eq = library.find("eq.high-pass");
    check(drive != nullptr && amp != nullptr && eq != nullptr, "modelos existem no catalogo");
    if (drive == nullptr || amp == nullptr || eq == nullptr)
        return;

    const Preset result = library.buildSignalChain(
        {"amp.brit-800", "stomp.yellow-drive", "eq.high-pass", "stomp.yellow-drive"}, "Livre");

    check(result.name == "Livre", "nome");
    check(result.rig.empty(), "sem rig legado");
    check(result.chainItems.size() == 4, "um item por equipamento, duplicatas inclusas");
    check(result.chainItems[0].gearId == "amp.brit-800", "amp primeiro");
    check(result.chainItems[1].gearId == "stomp.yellow-drive", "stomp depois do amp");
    check(result.chainItems[3].gearId == "stomp.yellow-drive", "mesmo stomp de novo");

    std::size_t covered = 0;
    for (const Preset::ChainItemState& item : result.chainItems)
        covered += item.moduleCount;
    check(covered == result.modules.size(), "itens cobrem a lista plana inteira");
    check(result.chainItems[0].moduleCount == amp->modules.size(), "amp ocupa a receita inteira");

    const std::size_t total = 2 * drive->modules.size() + amp->modules.size() + eq->modules.size();
    check(result.modules.size() == total, "nenhum Limiter implicito");
    check(result.modules.front().type == amp->modules.front().type, "receita do amp no inicio");

    ModuleChain chain;
    preset::apply(result, chain);
    check(chain.size() == total, "a cadeia livre aplica sem erro");
}

void testEmptySignalChainAndUnknownId()
{
    std::cout << "cadeia livre vazia e id desconhecido\n";

    GearLibrary library;
    const Preset empty = library.buildSignalChain({}, "Vazia");
    check(empty.modules.empty(), "lista vazia vira cadeia vazia");
    check(empty.chainItems.empty(), "sem itens");

    bool lancou = false;
    try
    {
        library.buildSignalChain({"amp.nao-existe"}, "x");
    }
    catch (const std::runtime_error&)
    {
        lancou = true;
    }
    check(lancou, "id desconhecido lanca");
}

// Passa um tom pela receita de um equipamento, em blocos do tamanho
// preparado como o callback faz, e devolve o nível de saída em dB relativo
// à entrada (RMS da segunda metade, depois do transiente).
double recipeGainDb(const GearLibrary& library, const std::string& gearId, double frequency, float amplitude)
{
    ModuleChain chain;
    preset::apply(library.buildSignalChain({gearId}, "teste"), chain);
    chain.prepare(44100.0, 512);

    std::vector<float> signal(22050);
    for (std::size_t i = 0; i < signal.size(); ++i)
        signal[i] = amplitude * static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * frequency
                                                            * static_cast<double>(i) / 44100.0));

    for (std::size_t start = 0; start < signal.size(); start += 512)
    {
        std::vector<float> block(signal.begin() + static_cast<std::ptrdiff_t>(start),
                                 signal.begin() + static_cast<std::ptrdiff_t>(std::min(start + 512, signal.size())));
        chain.process(block);
        std::copy(block.begin(), block.end(), signal.begin() + static_cast<std::ptrdiff_t>(start));
    }

    double sum = 0.0;
    for (std::size_t i = signal.size() / 2; i < signal.size(); ++i)
        sum += static_cast<double>(signal[i]) * static_cast<double>(signal[i]);
    const double rms = std::sqrt(sum / static_cast<double>(signal.size() - signal.size() / 2));
    return 20.0 * std::log10(rms / (static_cast<double>(amplitude) / std::sqrt(2.0)));
}

void testTapeFinishRoundsPeaksAndDarkensTheTop()
{
    std::cout << "Tape Finish amacia picos e atenua agudos\n";
    const GearLibrary library;
    constexpr const char* id = "rack.tape-finish";
    const double quiet = recipeGainDb(library, id, 440.0, 0.02f);
    const double hot = recipeGainDb(library, id, 440.0, 0.8f);
    const double bright = recipeGainDb(library, id, 8000.0, 0.02f);
    check(std::abs(quiet) < 0.5, "Drive padrao preserva notas fracas perto da unidade");
    check(hot < quiet - 1.5, "Drive padrao reduz o ganho nas notas fortes");
    check(bright < quiet - 0.8, "Color padrao suaviza agudos");

    Rig rig;
    rig.rack = {id};
    const auto preset = preset::deserialize(preset::serialize(library.buildPreset(rig, "Fita")));
    ModuleChain chain;
    preset::apply(preset, chain);
    check(chain.size() == 2 && std::string(chain.moduleAt(0).name()) == "TapeFinish"
          && std::string(chain.moduleAt(1).name()) == "Limiter", "preset restaura Tape Finish antes do limiter");
}

void testGreenScreamerSymmetricSuppressesTheSecondHarmonic()
{
    std::cout << "Green Screamer simetrico reduz o segundo harmonico\n";
    constexpr double twoPi = 6.283185307179586;
    constexpr double frequency = 1000.0;
    const GearLibrary library;

    for (const double rate : {44100.0, 48000.0, 96000.0})
    {
        WavFile input;
        input.sampleRate = rate;
        auto& samples = input.channels.emplace_back(static_cast<std::size_t>(rate / 5));
        for (std::size_t n = 0; n < samples.size(); ++n)
            samples[n] = static_cast<float>(0.25 * std::sin(twoPi * frequency * n / rate));

        const auto secondHarmonicRatio = [&](const char* id, int blockSize) {
            const Preset saved = library.buildSignalChain({id}, "Green Screamer");
            const Preset reopened = preset::deserialize(preset::serialize(saved));
            ModuleChain chain;
            preset::apply(reopened, chain);
            const auto output = offline::processFile(input, chain, blockSize);
            const auto& channel = output.channels.front();
            const auto magnitude = [&](int harmonic) {
                double cosine = 0.0;
                double sine = 0.0;
                for (std::size_t n = channel.size() / 2; n < channel.size(); ++n)
                {
                    const double phase = twoPi * frequency * harmonic * n / rate;
                    cosine += channel[n] * std::cos(phase);
                    sine += channel[n] * std::sin(phase);
                }
                return std::hypot(cosine, sine);
            };
            return magnitude(2) / magnitude(1);
        };

        const double original = secondHarmonicRatio("stomp.green-screamer", 128);
        const double symmetric = secondHarmonicRatio("stomp.green-screamer-symmetric", 128);
        const double symmetricShortBlock = secondHarmonicRatio("stomp.green-screamer-symmetric", 64);
        const std::string hz = " em " + std::to_string(static_cast<int>(rate)) + " Hz";
        check(original > 0.01, "original conserva segundo harmonico" + hz);
        check(symmetric < 0.0001 && symmetricShortBlock < 0.0001,
              "simetrico abaixo de -80 dBc nos dois tamanhos de bloco" + hz);
    }
}

void testGreenScreamer808RetainsBassAndFavorsMids()
{
    std::cout << "Green Screamer 808 preserva sinal grave e amplifica mais os medios\n";
    const GearLibrary library;
    constexpr const char* id = "stomp.green-screamer-808";
    const double low = recipeGainDb(library, id, 20.0, 0.001f);
    const double bass = recipeGainDb(library, id, 82.0, 0.001f);
    const double mid = recipeGainDb(library, id, 1000.0, 0.001f);
    check(low > -2.0 && low < 6.0, "grave profundo passa em vez de sumir no filtro de 400 Hz");
    check(mid > bass + 10.0, "ganho extra se concentra nos medios");
    const double hot = recipeGainDb(library, id, 1000.0, 0.25f);
    check(hot < mid - 10.0 && hot > 0.0, "saturacao reduz o ganho sem suprimir a nota");
}

void testGreenScreamer808ActiveToneMatchesCircuit()
{
    std::cout << "Tone ativo do 808 segue a curva analogica\n";
    const GearLibrary library;
    const auto legacy = library.buildSignalChain({"stomp.green-screamer-808"}, "Tone simples");
    const auto active = library.buildSignalChain({"stomp.green-screamer-808-circuit"}, "Tone ativo");
    ModuleChain chain;
    preset::apply(preset::deserialize(preset::serialize(legacy)), chain);
    check(std::string(chain.moduleAt(2).name()) == "LowPass", "preset anterior carrega o LowPass");
    preset::apply(preset::deserialize(preset::serialize(active)), chain);
    check(std::string(chain.moduleAt(2).name()) == "ScreamerTone", "preset novo carrega o Tone ativo");

    // Ganho AC em dB da rede de resistores e capacitores da Fig. 15
    // (DAFx-07), para 220, 1000 e 4000 Hz, sem Drive ou Level.
    constexpr std::array frequencies{220.0, 1000.0, 4000.0};
    constexpr std::array reference{
        std::array{-2.10, -9.64, -18.74},
        std::array{-1.14, -4.50, -14.26},
        std::array{-0.61, 0.52, -2.29},
    };
    AudioModule& tone = chain.moduleAt(2);
    for (const double rate : {44100.0, 48000.0, 96000.0})
    {
        bool matchesCurve = true;
        for (std::size_t pos = 0; pos < reference.size(); ++pos)
        {
            tone.findParameter("tone")->setValue(static_cast<float>(pos) * 0.5f);
            for (std::size_t i = 0; i < frequencies.size(); ++i)
            {
                tone.prepare(rate, 128);
                std::vector<float> samples(static_cast<std::size_t>(rate / 5));
                for (std::size_t n = 0; n < samples.size(); ++n)
                    samples[n] = 0.1f * static_cast<float>(std::sin(6.283185307179586 * frequencies[i] * n / rate));
                tone.process(samples);

                double energy = 0.0;
                const std::size_t start = samples.size() / 2;
                for (std::size_t n = start; n < samples.size(); ++n)
                    energy += static_cast<double>(samples[n]) * samples[n];
                const double meanSquare = energy / static_cast<double>(samples.size() - start);
                const double measured = 10.0 * std::log10(meanSquare / 0.005);
                matchesCurve = matchesCurve && std::abs(measured - reference[pos][i]) < 0.35;
            }
        }
        check(matchesCurve, "curvas em ate 0,35 dB da rede analogica em " + std::to_string(static_cast<int>(rate)) + " Hz");
    }
}

// O tone stack é passivo e perde volume; a receita avulsa repõe com um
// estágio de ganho, como os amplis. Com os knobs no meio, tons de 250 Hz,
// 1 kHz e 3 kHz saem a até 4 dB do nível de entrada.
void testStandaloneToneStackKeepsTheLevel()
{
    std::cout << "tone stack avulso nao derruba o volume\n";

    GearLibrary library;
    for (const double frequency : {250.0, 1000.0, 3000.0})
    {
        const double levelDb = recipeGainDb(library, "eq.tone-stack", frequency, 0.5f);
        check(std::fabs(levelDb) < 4.0, "nivel em " + std::to_string(static_cast<int>(frequency))
                                            + " Hz a ate 4 dB da entrada");
    }
}

// O compressor de pedal nos valores do catálogo: uma palhetada forte
// (-12 dBFS) sai perto do mesmo nível — o Output repõe o que o compressor
// tira —, e uma nota fraca (-30 dBFS) sobe bem mais que ela. A distância
// entre as duas, 18 dB na entrada, encolhe: é o sustain do pedal.
void testCompressorPedalKeepsTheLevelAndSqueezes()
{
    std::cout << "compressor de pedal mantem o volume e espreme a dinamica\n";

    GearLibrary library;
    const double loudGainDb = recipeGainDb(library, "stomp.orange-squeeze", 440.0, 0.25f);
    const double softGainDb = recipeGainDb(library, "stomp.orange-squeeze", 440.0, 0.0316f);

    check(std::fabs(loudGainDb) < 3.0, "palhetada forte sai a ate 3 dB da entrada");
    check(softGainDb - loudGainDb > 9.0, "nota fraca sobe pelo menos 9 dB a mais que a forte");
}

// O compressor de metal num Mi grave (82 Hz), a nota dos chugs: palhetada
// forte (-12 dBFS) sai perto do nível de entrada, e uma nota fraca
// (-30 dBFS) sobe bem mais — mesmo com o Blend devolvendo parte do seco.
// Em Blend 1.0 (só comprimido) a distância entre as duas encolhe ainda mais.
void testMetalCompressorEvensTheChugs()
{
    std::cout << "compressor de metal nivela os chugs no Mi grave\n";

    GearLibrary library;
    const double loudGainDb = recipeGainDb(library, "stomp.iron-clamp", 82.4, 0.25f);
    const double softGainDb = recipeGainDb(library, "stomp.iron-clamp", 82.4, 0.0316f);

    std::cout << "    forte " << loudGainDb << " dB, fraca " << softGainDb << " dB\n";
    check(std::fabs(loudGainDb) < 3.0, "palhetada forte sai a ate 3 dB da entrada");
    check(softGainDb - loudGainDb > 9.0, "nota fraca sobe pelo menos 9 dB a mais que a forte");
}

// Os drives do rig do Frusciante com os knobs no padrão: ligar o pedal com
// uma palhetada forte (-12 dBFS) não muda o volume MÉDIO em mais de 3 dB,
// tirando a média de uma nota grave, uma média e uma aguda. Nota a nota
// eles não são planos de propósito — o Turbo sobe ~10 dB perto de 900 Hz,
// o Siberian é escuro —, então o volume de uma nota só não diria nada.
void testFruscianteDrivesKeepTheLevel()
{
    std::cout << "drives do rig do Frusciante mantem o volume medio\n";

    GearLibrary library;
    for (const char* id : {"stomp.orange-turbo", "stomp.green-siberian", "stomp.bakersfield-fuzz"})
    {
        double sumDb = 0.0;
        for (double frequency : {110.0, 330.0, 880.0})
        {
            const double gainDb = recipeGainDb(library, id, frequency, 0.25f);
            std::cout << "    " << id << " " << frequency << " Hz: " << gainDb << " dB\n";
            sumDb += gainDb;
        }
        check(std::fabs(sumDb / 3.0) < 3.0, std::string(id) + ": media a ate 3 dB da entrada");
    }
}

void testFuzzControlsKeepWeakNotesAndHeadroom()
{
    std::cout << "fuzzes deixam passar notas fracas e conservam margem na saida\n";
    constexpr double rate = 48000.0;
    constexpr double twoPi = 6.283185307179586;
    const GearLibrary library;
    const auto signal = [&](const char* id, float amplitude, const std::vector<double>& frequencies) {
        const auto& model = *library.find(id);
        ModuleChain chain;
        preset::apply(library.buildSignalChain({id}, "Fuzz"), chain);
        for (const GearControl& knob : model.controls)
        {
            const float value = knob.label == "Fuzz" ? knob.minValue : knob.maxValue;
            chain.moduleAt(knob.moduleOffset).findParameter(knob.parameterId)->setValue(value);
        }

        WavFile input;
        input.sampleRate = rate;
        auto& samples = input.channels.emplace_back();
        for (const double hz : frequencies)
            for (int n = 0; n < 12000; ++n)
                samples.push_back(amplitude * static_cast<float>(std::sin(twoPi * hz * n / rate)));
        return offline::processFile(input, chain, 128);
    };

    for (const char* id : {"stomp.round-fuzz", "stomp.bakersfield-fuzz"})
    {
        const auto output = signal(id, 0.01f, {330.0});
        const auto& samples = output.channels.front();
        double energy = 0.0;
        for (std::size_t n = samples.size() / 2; n < samples.size(); ++n)
            energy += static_cast<double>(samples[n]) * samples[n];
        const double rms = std::sqrt(energy / static_cast<double>(samples.size() / 2));
        check(rms > 0.005, std::string(id) + ": nota fraca preservada no extremo de Bias ou Depth");
    }

    for (const char* id : {"stomp.green-siberian", "stomp.bakersfield-fuzz"})
    {
        const auto output = signal(id, 0.9f, {82.0, 330.0, 880.0, 1600.0, 3200.0, 6400.0});
        const auto& samples = output.channels.front();
        check(std::all_of(samples.begin(), samples.end(), [](float x) { return std::abs(x) < 1.0f; }),
              std::string(id) + ": Volume maximo dentro da escala com sinal forte, sem limiter");
    }
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

        const bool isBrit = amp->id.rfind("amp.brit-", 0) == 0;
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
    testSignalChainKeepsAnyOrderAndDuplicates();
    testEmptySignalChainAndUnknownId();
    testTapeFinishRoundsPeaksAndDarkensTheTop();
    testGreenScreamerSymmetricSuppressesTheSecondHarmonic();
    testGreenScreamer808RetainsBassAndFavorsMids();
    testGreenScreamer808ActiveToneMatchesCircuit();
    testStandaloneToneStackKeepsTheLevel();
    testCompressorPedalKeepsTheLevelAndSqueezes();
    testMetalCompressorEvensTheChugs();
    testFruscianteDrivesKeepTheLevel();
    testFuzzControlsKeepWeakNotesAndHeadroom();
    testScanFindsCabinetsAndMicrophones();
    testScanOfMissingDirectoryFindsNothing();
    testInferRigRecoversTheRigOfASavedChain();
    testInferRigPicksTheClosestOfTwoLookalikes();
    testInferRigRejectsAChainThatIsNotARig();
    testOnlyBritAmpsHavePresence();

    return reportResults();
}
