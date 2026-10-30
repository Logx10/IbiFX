#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "GearLibrary.h"
#include "PresetManager.h"

namespace
{
void set(Preset& preset, const std::string& gearId, std::size_t moduleOffset,
         const std::string& parameterId, float value)
{
    std::size_t firstModule = 0;
    for (const auto& item : preset.chainItems)
    {
        if (item.gearId == gearId)
        {
            if (moduleOffset >= item.moduleCount)
                break;
            auto& parameters = preset.modules[firstModule + moduleOffset].parameters;
            for (auto& [id, current] : parameters)
            {
                if (id == parameterId)
                {
                    current = value;
                    return;
                }
            }
            break;
        }
        firstModule += item.moduleCount;
    }
    throw std::runtime_error("controle inexistente: " + gearId + " / " + parameterId);
}

void save(const Preset& preset, const std::string& file)
{
    preset::save(preset, "presets/factory/" + file + ".ibifxpreset");
}
}

int main()
{
    std::filesystem::create_directories("presets/factory");
    GearLibrary library;
    library.scanImpulseResponses("irs");

    // Solo quente, inspirado na coda de Hotel California. Eco discreto.
    auto hotel = library.buildSignalChain({"stomp.analog-echo", "amp.tweed-59",
                                           "cab.factory.open-1x12", "rack.room",
                                           "rack.output-limiter"}, "California Dusk Lead");
    set(hotel, "stomp.analog-echo", 0, "time", 0.27f);
    set(hotel, "stomp.analog-echo", 0, "feedback", 0.18f);
    set(hotel, "stomp.analog-echo", 0, "mix", 0.12f);
    set(hotel, "amp.tweed-59", 1, "gain", 1.25f);
    set(hotel, "amp.tweed-59", 2, "drive", 1.05f);
    set(hotel, "amp.tweed-59", 3, "mid", 0.67f);
    set(hotel, "amp.tweed-59", 5, "drive", 1.35f);
    set(hotel, "amp.tweed-59", 6, "gain", 0.22f);
    set(hotel, "rack.room", 0, "mix", 0.13f);
    save(hotel, "01-hotel-california-lead");

    // Clean de funk com leve aspereza no ataque. O wah da gravação não é
    // modelado pelo IbiFX; este preset privilegia a parte tocável sem ele.
    auto psycho = library.buildSignalChain({"amp.american-clean",
                                            "cab.factory.open-1x12", "rack.room",
                                            "rack.output-limiter"}, "Psycho Funk Clean");
    set(psycho, "amp.american-clean", 1, "gain", 1.5f);
    set(psycho, "amp.american-clean", 2, "drive", 0.75f);
    set(psycho, "amp.american-clean", 3, "bass", 0.42f);
    set(psycho, "amp.american-clean", 3, "mid", 0.52f);
    set(psycho, "amp.american-clean", 3, "treble", 0.68f);
    set(psycho, "amp.american-clean", 4, "gain", 2.5f);
    set(psycho, "amp.american-clean", 6, "gain", 0.70f);
    set(psycho, "rack.room", 0, "mix", 0.07f);
    save(psycho, "02-sir-psycho-sexy-clean");

    // Crunch aberto de amp, sem pedal de distorção nem ambiência longa.
    auto voltage = library.buildSignalChain({"amp.brit-plexi",
                                             "cab.factory.brit-4x12",
                                             "rack.output-limiter"}, "Voltage 75 Crunch");
    set(voltage, "amp.brit-plexi", 1, "gain", 1.35f);
    set(voltage, "amp.brit-plexi", 2, "drive", 1.25f);
    set(voltage, "amp.brit-plexi", 3, "mid", 0.74f);
    set(voltage, "amp.brit-plexi", 5, "drive", 1.40f);
    set(voltage, "amp.brit-plexi", 5, "presence", 0.20f);
    set(voltage, "amp.brit-plexi", 6, "gain", 0.13f);
    save(voltage, "03-high-voltage-crunch");

    // Lead médio e aveludado, com eco e sala depois do cabinet.
    auto estranged = library.buildSignalChain({"amp.brit-plexi",
                                               "cab.factory.brit-4x12",
                                               "rack.digital-delay", "rack.hall",
                                               "rack.output-limiter"}, "Estranged Singing Lead");
    set(estranged, "amp.brit-plexi", 1, "gain", 1.55f);
    set(estranged, "amp.brit-plexi", 2, "drive", 1.75f);
    set(estranged, "amp.brit-plexi", 3, "mid", 0.72f);
    set(estranged, "amp.brit-plexi", 3, "treble", 0.44f);
    set(estranged, "amp.brit-plexi", 5, "presence", 0.16f);
    set(estranged, "amp.brit-plexi", 6, "gain", 0.14f);
    set(estranged, "rack.digital-delay", 0, "time", 0.34f);
    set(estranged, "rack.digital-delay", 0, "feedback", 0.27f);
    set(estranged, "rack.digital-delay", 0, "mix", 0.16f);
    set(estranged, "rack.hall", 0, "decay", 0.66f);
    set(estranged, "rack.hall", 0, "mix", 0.13f);
    save(estranged, "04-estranged-lead");

    // Metal de base: médios presentes, graves controlados e ambiência mínima.
    auto youth = library.buildSignalChain({"amp.modern-hi-gain",
                                           "cab.factory.brit-4x12", "rack.room",
                                           "rack.output-limiter"}, "Youthanasia Tight Rhythm");
    set(youth, "amp.modern-hi-gain", 1, "gain", 2.0f);
    set(youth, "amp.modern-hi-gain", 2, "drive", 2.45f);
    set(youth, "amp.modern-hi-gain", 3, "bass", 0.48f);
    set(youth, "amp.modern-hi-gain", 3, "mid", 0.48f);
    set(youth, "amp.modern-hi-gain", 3, "treble", 0.48f);
    set(youth, "amp.modern-hi-gain", 5, "drive", 0.95f);
    set(youth, "amp.modern-hi-gain", 6, "gain", 0.13f);
    set(youth, "rack.room", 0, "mix", 0.06f);
    save(youth, "05-youthanasia-rhythm");

    // Lead instrumental com sustain e delay curto, sem scoop exagerado.
    auto reflective = library.buildSignalChain({"amp.brit-800",
                                                "cab.factory.brit-4x12",
                                                "rack.digital-delay", "rack.room",
                                                "rack.output-limiter"}, "Reflective Fusion Lead");
    set(reflective, "amp.brit-800", 1, "gain", 1.55f);
    set(reflective, "amp.brit-800", 2, "drive", 2.25f);
    set(reflective, "amp.brit-800", 3, "mid", 0.72f);
    set(reflective, "amp.brit-800", 3, "treble", 0.52f);
    set(reflective, "amp.brit-800", 5, "presence", 0.20f);
    set(reflective, "amp.brit-800", 6, "gain", 0.14f);
    set(reflective, "rack.digital-delay", 0, "time", 0.29f);
    set(reflective, "rack.digital-delay", 0, "feedback", 0.25f);
    set(reflective, "rack.digital-delay", 0, "mix", 0.18f);
    set(reflective, "rack.room", 0, "mix", 0.13f);
    save(reflective, "06-reflective-lead");
}
