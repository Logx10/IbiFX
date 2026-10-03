#include <iostream>
#include <memory>
#include <string>

#include "DesktopUI.h"
#include "GainProcessor.h"
#include "Limiter.h"
#include "LiveEngine.h"

// Primeira janela da Fase 12 (AI_GUIDELINES §36): um knob de ganho e um
// medidor ligados ao LiveEngine, via Dear ImGui + SDL3 (ADR 0001). A cadeia
// aqui é deliberadamente mínima — Limiter entra por ser convenção do
// projeto em toda cadeia real (protege contra estouro), não por ser "mais
// um pedal" demonstrado. DesktopUI já é genérica e escala pra qualquer
// cadeia; montar o pedalboard inteiro fica para o próximo passo da Fase 12.
int main(int argc, char** argv)
{
    bool useNullDevice = false;

    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--null")
            useNullDevice = true;
    }

    LiveEngine engine;
    engine.chain().add(std::make_unique<GainProcessor>());
    engine.chain().add(std::make_unique<Limiter>());

    if (!useNullDevice)
    {
        std::cout << "AVISO: se a entrada e a saida forem os dispositivos embutidos,\n"
                  << "       o som realimenta e vira microfonia. Use fones de ouvido.\n";
    }

    DesktopUI ui(engine);

    return ui.run(useNullDevice ? AudioDevice::Mode::Null : AudioDevice::Mode::Duplex,
                  48000.0, 128);
}
