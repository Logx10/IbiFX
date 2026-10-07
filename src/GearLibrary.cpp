#include "GearLibrary.h"

#include "ModuleChain.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace
{
using ParameterList = std::vector<std::pair<std::string, float>>;

Preset::ModuleState module(const std::string& type, ParameterList parameters = {})
{
    Preset::ModuleState state;
    state.type = type;
    state.parameters = std::move(parameters);
    return state;
}

GearControl control(std::string label, std::size_t moduleOffset, std::string parameterId,
                    float minValue, float maxValue)
{
    GearControl result;
    result.label = std::move(label);
    result.moduleOffset = moduleOffset;
    result.parameterId = std::move(parameterId);
    result.minValue = minValue;
    result.maxValue = maxValue;
    return result;
}

GearModel gear(std::string id,
               std::string name,
               GearCategory category,
               std::string character,
               std::string description,
               std::vector<Preset::ModuleState> modules)
{
    GearModel model;
    model.id = std::move(id);
    model.name = std::move(name);
    model.category = category;
    model.character = std::move(character);
    model.description = std::move(description);
    model.modules = std::move(modules);
    return model;
}

// Os pedais: cada um é a curva de saturação (ou o efeito) que mais se parece
// com o circuito original, mais o filtro e o volume em volta dela.
void addStomps(std::vector<GearModel>& models)
{
    models.push_back(gear("stomp.gate", "Noise Gate", GearCategory::Stomp, "Dynamics",
        "Corta o chiado entre as notas. Vai antes de qualquer ganho.",
        {module("NoiseGate", {{"threshold", 0.02f}, {"release", 0.15f}})}));

    models.push_back(gear("stomp.orange-squeeze", "Orange Squeeze", GearCategory::Stomp, "Dynamics",
        "Compressor de pedal: nivela a palhetada e alonga o sustain.",
        {module("Compressor", {{"threshold", -24.0f}, {"ratio", 4.0f},
                               {"attack", 0.005f}, {"release", 0.2f}})}));

    models.push_back(gear("stomp.clean-boost", "Clean Boost", GearCategory::Stomp, "Boost",
        "Só volume, sem distorção — empurra o ampli para saturar mais.",
        {module("Gain", {{"gain", 2.0f}})}));

    models.push_back(gear("stomp.green-screamer", "Green Screamer", GearCategory::Stomp, "Overdrive",
        "Overdrive de médio saliente: corta o grave antes de clipar, assimétrico.",
        {module("HighPass", {{"frequency", 400.0f}}),
         module("AsymmetricClipper", {{"drive", 4.0f}, {"bias", 0.15f}}),
         module("Gain", {{"gain", 1.2f}})}));
    models.back().controls = {control("Drive", 1, "drive", 0.5f, 12.0f),
                              control("Low Cut", 0, "frequency", 100.0f, 900.0f),
                              control("Level", 2, "gain", 0.0f, 3.0f)};

    models.push_back(gear("stomp.yellow-drive", "Yellow Drive", GearCategory::Stomp, "Overdrive",
        "Overdrive transparente e simétrico, de faixa cheia.",
        {module("SoftClipper", {{"drive", 3.0f}}),
         module("Gain", {{"gain", 1.0f}})}));
    models.back().controls = {control("Drive", 0, "drive", 0.5f, 10.0f),
                              control("Level", 1, "gain", 0.0f, 2.5f)};

    models.push_back(gear("stomp.black-rodent", "Black Rodent", GearCategory::Stomp, "Distortion",
        "Distorção de clipagem dura, áspera e agressiva.",
        {module("HighPass", {{"frequency", 150.0f}}),
         module("Gain", {{"gain", 4.0f}}),
         module("Clipper", {{"threshold", 0.4f}}),
         module("Gain", {{"gain", 2.0f}})}));
    models.back().controls = {control("Distortion", 1, "gain", 1.0f, 8.0f),
                              control("Low Cut", 0, "frequency", 40.0f, 600.0f),
                              control("Level", 3, "gain", 0.0f, 4.0f)};

    models.push_back(gear("stomp.round-fuzz", "Round Fuzz", GearCategory::Stomp, "Fuzz",
        "Fuzz de transistor: ganho enorme e assimetria forte, quase onda quadrada.",
        {module("Gain", {{"gain", 6.0f}}),
         module("AsymmetricClipper", {{"drive", 20.0f}, {"bias", 0.5f}})}));
    models.back().controls = {control("Fuzz", 0, "gain", 1.0f, 8.0f),
                              control("Bias", 1, "bias", 0.0f, 0.9f)};

    // Um módulo só: o painel (Rate, Depth, Mix) sai automático das faixas
    // do próprio Chorus — ver o construtor de GearLibrary.
    models.push_back(gear("stomp.blue-chorus", "Blue Chorus", GearCategory::Stomp, "Modulation",
        "Chorus clássico dos anos 80: uma cópia levemente desafinada que ondula junto com o som.",
        {module("Chorus", {{"rate", 0.8f}, {"depth", 0.5f}, {"mix", 0.5f}})}));
}

// Os amplis: a MESMA estrutura (filtro de entrada -> ganho -> Preamp ->
// ToneStack -> volume -> PowerAmp), com valores diferentes. O que separa um
// "limpo americano" de um "high gain moderno" é quanto ganho entra no
// Preamp, onde o ToneStack está girado e quanto o PowerAmp respira (sag).
void addAmps(std::vector<GearModel>& models)
{
    models.push_back(gear("amp.american-clean", "American Clean", GearCategory::Amp, "Clean",
        "Limpo cristalino com agudo brilhante e muito headroom.",
        {module("HighPass", {{"frequency", 70.0f}}),
         module("Gain", {{"gain", 2.0f}}),
         module("Preamp", {{"drive", 0.8f}, {"interstage", 11000.0f}}),
         module("ToneStack", {{"bass", 0.6f}, {"mid", 0.35f}, {"treble", 0.7f}}),
         module("Gain", {{"gain", 3.0f}}),
         module("PowerAmp", {{"drive", 0.8f}, {"sag", 0.1f}}),
         module("Gain", {{"gain", 1.0f}})}));

    // O ToneStack do IbiFX É o do '59 Bassman (ver ToneStack.h) — este é o
    // modelo mais fiel ao circuito que a gente simula.
    models.push_back(gear("amp.tweed-59", "Tweed '59", GearCategory::Amp, "Crunch",
        "Crunch quente e comprimido que 'respira' com a palhetada (muito sag).",
        {module("HighPass", {{"frequency", 80.0f}}),
         module("Gain", {{"gain", 1.5f}}),
         module("Preamp", {{"drive", 1.2f}, {"interstage", 6000.0f}}),
         module("ToneStack", {{"bass", 0.5f}, {"mid", 0.6f}, {"treble", 0.55f}}),
         module("Gain", {{"gain", 2.5f}}),
         module("PowerAmp", {{"drive", 2.0f}, {"sag", 0.7f}}),
         module("Gain", {{"gain", 0.27f}})}));

    models.push_back(gear("amp.brit-plexi", "Brit Plexi", GearCategory::Amp, "Crunch",
        "Crunch clássico de rock dos anos 60/70, aberto e com o power amp trabalhando.",
        {module("HighPass", {{"frequency", 100.0f}}),
         module("Gain", {{"gain", 2.0f}}),
         module("Preamp", {{"drive", 1.8f}, {"interstage", 9000.0f}}),
         module("ToneStack", {{"bass", 0.45f}, {"mid", 0.7f}, {"treble", 0.6f}}),
         module("Gain", {{"gain", 2.5f}}),
         module("PowerAmp", {{"drive", 2.2f}, {"sag", 0.5f}, {"presence", 0.5f}}),
         module("Gain", {{"gain", 0.2f}})}));

    models.push_back(gear("amp.brit-800", "Brit 800", GearCategory::Amp, "High Gain",
        "Hard rock dos anos 80: médio para frente, grave apertado.",
        {module("HighPass", {{"frequency", 120.0f}}),
         module("Gain", {{"gain", 3.0f}}),
         module("Preamp", {{"drive", 2.8f}, {"interstage", 7500.0f}}),
         module("ToneStack", {{"bass", 0.45f}, {"mid", 0.75f}, {"treble", 0.6f}}),
         module("Gain", {{"gain", 2.0f}}),
         module("PowerAmp", {{"drive", 1.5f}, {"sag", 0.3f}, {"presence", 0.5f}}),
         module("Gain", {{"gain", 0.2f}})}));

    models.push_back(gear("amp.modern-hi-gain", "Modern Hi-Gain", GearCategory::Amp, "High Gain",
        "Metal moderno: saturação densa, médio cavado, grave firme.",
        {module("HighPass", {{"frequency", 140.0f}}),
         module("Gain", {{"gain", 4.0f}}),
         module("Preamp", {{"drive", 4.5f}, {"interstage", 6500.0f}}),
         module("ToneStack", {{"bass", 0.65f}, {"mid", 0.3f}, {"treble", 0.65f}}),
         module("Gain", {{"gain", 2.5f}}),
         module("PowerAmp", {{"drive", 1.2f}, {"sag", 0.2f}}),
         module("Gain", {{"gain", 0.2f}})}));

    // Todos os amplis têm a mesma estrutura, então o mesmo painel — como
    // o de um ampli real: Gain, Bass, Middle, Treble, Sag e Master. O teto
    // do Master é 2,5x o volume calibrado da receita, para o padrão cair
    // perto do "4" na escala de 0 a 10.
    //
    // Os Brit ganham Presence entre Treble e Sag, onde ele fica num painel
    // Marshall de verdade. Os outros não: o knob não existe nos amplis que
    // eles lembram, e a receita deles deixa o presence em 0 (neutro).
    for (GearModel& model : models)
    {
        if (model.category != GearCategory::Amp)
            continue;

        const float master = model.modules[6].parameters[0].second;
        model.controls = {control("Gain", 2, "drive", 0.1f, 5.0f),
                          control("Bass", 3, "bass", 0.0f, 1.0f),
                          control("Middle", 3, "mid", 0.0f, 1.0f),
                          control("Treble", 3, "treble", 0.0f, 1.0f)};

        if (model.id == "amp.brit-plexi" || model.id == "amp.brit-800")
            model.controls.push_back(control("Presence", 5, "presence", 0.0f, 1.0f));

        model.controls.push_back(control("Sag", 5, "sag", 0.0f, 1.0f));
        model.controls.push_back(control("Master", 6, "gain", 0.0f, master * 2.5f));
    }
}

// O rack de estúdio: os efeitos de ambiência, depois do microfone.
void addRack(std::vector<GearModel>& models)
{
    models.push_back(gear("rack.slapback", "Slapback", GearCategory::Rack, "Delay",
        "Uma repetição curta, estilo rockabilly.",
        {module("Delay", {{"time", 0.09f}, {"feedback", 0.1f}, {"mix", 0.35f}})}));

    models.push_back(gear("rack.digital-delay", "Digital Delay", GearCategory::Rack, "Delay",
        "Repetições limpas e definidas.",
        {module("Delay", {{"time", 0.25f}, {"feedback", 0.3f}, {"mix", 0.25f}})}));

    models.push_back(gear("rack.long-echo", "Long Echo", GearCategory::Rack, "Delay",
        "Eco longo para solos, com muitas repetições.",
        {module("Delay", {{"time", 0.45f}, {"feedback", 0.5f}, {"mix", 0.3f}})}));

    models.push_back(gear("rack.room", "Room Reverb", GearCategory::Rack, "Reverb",
        "Sala pequena: dá ar sem afastar a guitarra.",
        {module("Reverb", {{"decay", 0.4f}, {"damping", 0.6f}, {"mix", 0.2f}})}));

    models.push_back(gear("rack.hall", "Hall Reverb", GearCategory::Rack, "Reverb",
        "Salão grande, cauda longa e brilhante.",
        {module("Reverb", {{"decay", 0.85f}, {"damping", 0.3f}, {"mix", 0.35f}})}));
}

// Um knob por parâmetro do módulo, na faixa e com o rótulo do próprio
// Parameter. Constrói o módulo de verdade (via preset::apply) para ler
// essas faixas, em vez de copiá-las à mão para cá e deixá-las divergir.
std::vector<GearControl> controlsFromModule(const Preset::ModuleState& state)
{
    Preset single;
    single.modules = {state};

    ModuleChain chain;
    preset::apply(single, chain);

    std::vector<GearControl> result;
    const AudioModule& audioModule = chain.moduleAt(0);
    for (std::size_t p = 0; p < audioModule.parameterCount(); ++p)
    {
        const Parameter& parameter = audioModule.parameterAt(p);
        result.push_back(control(parameter.label(), 0, parameter.id(),
                                 parameter.minValue(), parameter.maxValue()));
    }
    return result;
}

bool isWavFile(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".wav";
}

// "4x12 Brit" -> "4x12-brit", para montar um id estável a partir do nome
// da pasta/arquivo.
std::string slug(const std::string& text)
{
    std::string result;
    for (unsigned char c : text)
    {
        if (std::isalnum(c))
            result += static_cast<char>(std::tolower(c));
        else if (!result.empty() && result.back() != '-')
            result += '-';
    }
    while (!result.empty() && result.back() == '-')
        result.pop_back();
    return result;
}
}

const char* gearCategoryName(GearCategory category)
{
    switch (category)
    {
    case GearCategory::Stomp: return "Stomp";
    case GearCategory::Amp: return "Amp";
    case GearCategory::Cabinet: return "Cabinet";
    case GearCategory::Rack: return "Rack";
    }
    return "?";
}

GearLibrary::GearLibrary()
{
    addStomps(m_models);
    addAmps(m_models);
    addRack(m_models);

    for (GearModel& model : m_models)
    {
        if (model.controls.empty() && model.modules.size() == 1)
            model.controls = controlsFromModule(model.modules[0]);
    }
}

std::size_t GearLibrary::scanImpulseResponses(const std::string& directory)
{
    namespace fs = std::filesystem;

    std::erase_if(m_models, [](const GearModel& model) {
        return model.category == GearCategory::Cabinet;
    });

    std::error_code error;
    if (!fs::is_directory(directory, error))
        return 0;

    // Ordenado por caminho: directory_iterator não garante ordem, e o
    // navegador não deveria embaralhar a lista a cada execução.
    std::vector<fs::path> cabinetDirs;
    for (const auto& entry : fs::directory_iterator(directory, error))
    {
        if (entry.is_directory())
            cabinetDirs.push_back(entry.path());
    }
    std::sort(cabinetDirs.begin(), cabinetDirs.end());

    std::size_t found = 0;
    for (const fs::path& cabinetDir : cabinetDirs)
    {
        std::vector<fs::path> irFiles;
        for (const auto& entry : fs::directory_iterator(cabinetDir, error))
        {
            if (entry.is_regular_file() && isWavFile(entry.path()))
                irFiles.push_back(entry.path());
        }
        std::sort(irFiles.begin(), irFiles.end());

        const std::string cabinetName = cabinetDir.filename().string();
        for (const fs::path& irFile : irFiles)
        {
            const std::string microphone = irFile.stem().string();

            Preset::ModuleState cabinet = module("Cabinet", {{"mix", 1.0f}});
            cabinet.irPath = irFile.string();

            GearModel model = gear("cab." + slug(cabinetName) + "." + slug(microphone),
                                   cabinetName, GearCategory::Cabinet, "Cabinet",
                                   "Impulse response: " + irFile.filename().string(),
                                   {cabinet});
            model.microphone = microphone;

            // Sem controlsFromModule(): isso carregaria a IR do disco só
            // para descobrir a faixa do mix, que já se sabe ser 0..1.
            model.controls = {control("Mix", 0, "mix", 0.0f, 1.0f)};

            m_models.push_back(std::move(model));
            ++found;
        }
    }

    return found;
}

const std::vector<GearModel>& GearLibrary::models() const
{
    return m_models;
}

std::vector<const GearModel*> GearLibrary::modelsIn(GearCategory category) const
{
    std::vector<const GearModel*> result;
    for (const GearModel& model : m_models)
    {
        if (model.category == category)
            result.push_back(&model);
    }
    return result;
}

const GearModel* GearLibrary::find(const std::string& id) const
{
    for (const GearModel& model : m_models)
    {
        if (model.id == id)
            return &model;
    }
    return nullptr;
}

std::vector<const GearModel*> GearLibrary::rigModels(const Rig& rig) const
{
    std::vector<const GearModel*> result;

    for (const std::string& id : rig.stomps)
        result.push_back(&require(id, GearCategory::Stomp));

    if (!rig.amp.empty())
        result.push_back(&require(rig.amp, GearCategory::Amp));

    if (!rig.cabinet.empty())
        result.push_back(&require(rig.cabinet, GearCategory::Cabinet));

    for (const std::string& id : rig.rack)
        result.push_back(&require(id, GearCategory::Rack));

    return result;
}

Preset GearLibrary::buildPreset(const Rig& rig, const std::string& name) const
{
    Preset result;
    result.name = name;

    for (const GearModel* model : rigModels(rig))
        result.modules.insert(result.modules.end(), model->modules.begin(), model->modules.end());

    result.modules.push_back(module("Limiter"));
    return result;
}

namespace
{
// Dois caminhos de IR apontam para o mesmo arquivo? Compara a forma
// normalizada, porque um preset salvo no Windows guarda "irs\X\y.wav" e o
// scan pode montar "irs/X\y.wav" — mesmo arquivo, texto diferente.
bool samePath(const std::string& a, const std::string& b)
{
    std::filesystem::path left = std::filesystem::path(a).lexically_normal();
    std::filesystem::path right = std::filesystem::path(b).lexically_normal();
    return left.make_preferred() == right.make_preferred();
}

// A receita cabe em modules a partir de `at`, com os mesmos tipos na mesma
// ordem (e a mesma IR, se for um cabinet)?
bool recipeFitsAt(const std::vector<Preset::ModuleState>& recipe,
                  const std::vector<Preset::ModuleState>& modules, std::size_t at)
{
    if (recipe.empty() || at + recipe.size() > modules.size())
        return false;

    for (std::size_t i = 0; i < recipe.size(); ++i)
    {
        const Preset::ModuleState& expected = recipe[i];
        const Preset::ModuleState& actual = modules[at + i];

        if (expected.type != actual.type)
            return false;

        if (!expected.irPath.empty() && !samePath(expected.irPath, actual.irPath))
            return false;
    }
    return true;
}

// O quanto os valores salvos se afastaram da receita — desempata receitas
// de mesma estrutura (ver GearLibrary::inferRig()). Distância, não "quantos
// ficaram iguais": quem salvou depois de girar TODOS os knobs não deixou
// nenhum igual, mas continua mais perto da receita de onde partiu. Cada
// diferença é relativa ao tamanho do valor, para uma frequência em hertz
// não pesar mais que um mix de 0 a 1.
float recipeDistance(const std::vector<Preset::ModuleState>& recipe,
                     const std::vector<Preset::ModuleState>& modules, std::size_t at)
{
    float distance = 0.0f;
    for (std::size_t i = 0; i < recipe.size(); ++i)
    {
        for (const auto& [id, value] : recipe[i].parameters)
        {
            for (const auto& [savedId, savedValue] : modules[at + i].parameters)
            {
                if (savedId == id)
                    distance += std::fabs(savedValue - value) / std::max(1.0f, std::fabs(value));
            }
        }
    }
    return distance;
}

// Em que ponto da ordem stomp -> amp -> cab -> rack a busca está. Cada
// estágio só aceita equipamentos dele em diante: um stomp depois do amp não
// é um rig que a GearLibrary monta.
enum class RigStage
{
    Stomps,
    AfterAmp,
    AfterCabinet,
    Rack
};

bool parseRig(const std::vector<GearModel>& catalog, const std::vector<Preset::ModuleState>& modules,
              std::size_t position, RigStage stage, Rig& rig)
{
    // O fim de todo rig é o Limiter que buildPreset() acrescenta.
    if (position + 1 == modules.size() && modules[position].type == "Limiter")
        return true;

    struct Candidate
    {
        const GearModel* model;
        RigStage next;
        float distance;
    };
    std::vector<Candidate> candidates;

    for (const GearModel& model : catalog)
    {
        RigStage next = stage;
        switch (model.category)
        {
        case GearCategory::Stomp:
            if (stage != RigStage::Stomps) continue;
            next = RigStage::Stomps;
            break;
        case GearCategory::Amp:
            if (stage != RigStage::Stomps) continue;
            next = RigStage::AfterAmp;
            break;
        case GearCategory::Cabinet:
            if (stage == RigStage::AfterCabinet || stage == RigStage::Rack) continue;
            next = RigStage::AfterCabinet;
            break;
        case GearCategory::Rack:
            next = RigStage::Rack;
            break;
        }

        if (recipeFitsAt(model.modules, modules, position))
            candidates.push_back({&model, next, recipeDistance(model.modules, modules, position)});
    }

    // Mais parecida primeiro; empate fica na ordem do catálogo.
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });

    for (const Candidate& candidate : candidates)
    {
        const GearModel& model = *candidate.model;

        switch (model.category)
        {
        case GearCategory::Stomp: rig.stomps.push_back(model.id); break;
        case GearCategory::Amp: rig.amp = model.id; break;
        case GearCategory::Cabinet: rig.cabinet = model.id; break;
        case GearCategory::Rack: rig.rack.push_back(model.id); break;
        }

        if (parseRig(catalog, modules, position + model.modules.size(), candidate.next, rig))
            return true;

        // Não fechou mais adiante — desfaz e tenta a próxima.
        switch (model.category)
        {
        case GearCategory::Stomp: rig.stomps.pop_back(); break;
        case GearCategory::Amp: rig.amp.clear(); break;
        case GearCategory::Cabinet: rig.cabinet.clear(); break;
        case GearCategory::Rack: rig.rack.pop_back(); break;
        }
    }

    return false;
}
}

bool GearLibrary::matchesRig(const Preset& preset, const Rig& rig) const
{
    Preset built;
    try
    {
        built = buildPreset(rig, preset.name);
    }
    catch (const std::exception&)
    {
        return false;
    }

    return built.modules.size() == preset.modules.size()
        && recipeFitsAt(built.modules, preset.modules, 0);
}

bool GearLibrary::inferRig(const Preset& preset, Rig& rig) const
{
    Rig found;
    if (!parseRig(m_models, preset.modules, 0, RigStage::Stomps, found) || found.empty())
        return false;

    rig = found;
    return true;
}

const GearModel& GearLibrary::require(const std::string& id, GearCategory expected) const
{
    const GearModel* model = find(id);
    if (model == nullptr)
        throw std::runtime_error("equipamento desconhecido: " + id);

    if (model->category != expected)
    {
        throw std::runtime_error("equipamento " + id + " e um " + gearCategoryName(model->category)
                                 + ", nao um " + gearCategoryName(expected));
    }

    return *model;
}
