#include "Preset.h"

#include <memory>
#include <stdexcept>

#include "ModuleChain.h"

#include "AsymmetricClipper.h"
#include "Cabinet.h"
#include "Chorus.h"
#include "Clipper.h"
#include "Compressor.h"
#include "Delay.h"
#include "GainProcessor.h"
#include "HighPassFilter.h"
#include "Limiter.h"
#include "NoiseGate.h"
#include "Preamp.h"
#include "PowerAmp.h"
#include "Reverb.h"
#include "SoftClipper.h"
#include "ToneStack.h"

namespace
{
// Reconstrói um módulo a partir do tipo salvo — o mesmo texto que name()
// devolve. É a única função do projeto que precisa conhecer todos os tipos
// concretos de uma vez, e por isso fica isolada aqui: um módulo novo exige
// mexer nesta lista, mas em nenhuma outra parte do preset.
std::unique_ptr<AudioModule> createModule(const std::string& type)
{
    if (type == "Gain") return std::make_unique<GainProcessor>();
    if (type == "Clipper") return std::make_unique<Clipper>();
    if (type == "SoftClipper") return std::make_unique<SoftClipper>();
    if (type == "AsymmetricClipper") return std::make_unique<AsymmetricClipper>();
    if (type == "NoiseGate") return std::make_unique<NoiseGate>();
    if (type == "Compressor") return std::make_unique<Compressor>();
    if (type == "HighPass") return std::make_unique<HighPassFilter>();
    if (type == "Delay") return std::make_unique<Delay>();
    if (type == "Reverb") return std::make_unique<Reverb>();
    if (type == "Chorus") return std::make_unique<Chorus>();
    if (type == "Preamp") return std::make_unique<Preamp>();
    if (type == "ToneStack") return std::make_unique<ToneStack>();
    if (type == "PowerAmp") return std::make_unique<PowerAmp>();
    if (type == "Cabinet") return std::make_unique<Cabinet>();
    if (type == "Limiter") return std::make_unique<Limiter>();

    throw std::runtime_error("preset cita um tipo de modulo desconhecido: " + type);
}
}

namespace preset
{
Preset capture(const std::string& name, const ModuleChain& chain)
{
    Preset result;
    result.name = name;

    for (std::size_t i = 0; i < chain.size(); ++i)
    {
        const AudioModule& module = chain.moduleAt(i);

        Preset::ModuleState state;
        state.type = module.name();
        state.bypassed = chain.isBypassed(i);

        for (std::size_t p = 0; p < module.parameterCount(); ++p)
        {
            const Parameter& parameter = module.parameterAt(p);
            state.parameters.emplace_back(parameter.id(), parameter.value());
        }

        // Cabinet é o único módulo cujo estado relevante não é um
        // Parameter — ver o comentário em Preset.h.
        if (const auto* cabinet = dynamic_cast<const Cabinet*>(&module))
            state.irPath = cabinet->irPath();

        result.modules.push_back(std::move(state));
    }

    return result;
}

void apply(const Preset& preset, ModuleChain& chain)
{
    chain.clear();

    for (const Preset::ModuleState& state : preset.modules)
    {
        auto module = createModule(state.type);

        for (const auto& [id, value] : state.parameters)
        {
            Parameter* parameter = module->findParameter(id);

            // Um parâmetro salvo que o módulo atual não reconhece é
            // ignorado, não é erro: é o mesmo espírito de Parameter::
            // setValue() ajustar pra borda em vez de rejeitar — preset
            // antigo lido por um módulo que ganhou ou perdeu parâmetros não
            // deveria impedir o resto de carregar.
            if (parameter != nullptr)
                parameter->setValue(value);
        }

        if (auto* cabinet = dynamic_cast<Cabinet*>(module.get());
            cabinet != nullptr && !state.irPath.empty())
        {
            cabinet->loadImpulseResponseFile(state.irPath);
        }

        chain.add(std::move(module));
        chain.setBypassed(chain.size() - 1, state.bypassed);
    }
}
}
