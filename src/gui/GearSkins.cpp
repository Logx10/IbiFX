#include "GearSkins.h"

namespace
{
// Medidas tiradas de cada body.png com `tools/gear_assets.py holes`. O
// diâmetro dos knobs e LEDs é um pouco maior que o buraco medido, para a
// peça cobrir a borda dele como num pedal de verdade.
std::vector<GearSkin> buildSkins()
{
    std::vector<GearSkin> skins;

    GearSkin gate;
    gate.gearId = "stomp.gate";
    gate.knobDiameter = 0.155f;
    gate.knobs = {{"threshold", {0.4031f, 0.2119f}},
                  {"release", {0.7146f, 0.2127f}}};
    gate.ledDiameter = 0.058f;
    gate.statusLed = {0.5021f, 0.0582f};
    gate.unlitLeds = {{0.1823f, 0.2224f}};   // REDUCTION
    gate.hasFootswitch = true;
    gate.footswitch = {0.4990f, 0.8112f};
    gate.footswitchWidth = 0.30f;
    skins.push_back(gate);

    return skins;
}
}

const GearSkin* findGearSkin(const std::string& gearId)
{
    static const std::vector<GearSkin> skins = buildSkins();

    for (const GearSkin& skin : skins)
    {
        if (gearId == skin.gearId)
            return &skin;
    }
    return nullptr;
}
