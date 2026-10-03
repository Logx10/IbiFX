#include <iostream>
#include <string>

#include "DesktopUI.h"
#include "LiveEngine.h"
#include "PedalboardChain.h"

// Janela da Fase 12 (AI_GUIDELINES §36): o pedalboard inteiro — gate,
// compressor, filtro, drive, delay, reverb, limiter — ligado ao LiveEngine
// de verdade, via Dear ImGui + SDL3 (ADR 0001).
//
// buildDefaultChain() é a mesma função que o CLI usa (`ibifx --ui` e
// `ibifx --live`), em PedalboardChain.h — não uma cópia: os dois
// frontends descrevem o mesmo pedalboard padrão, e duplicar a lista de
// módulos arriscaria os dois divergirem sem ninguém notar.
int main(int argc, char** argv)
{
    bool useNullDevice = false;

    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--null")
            useNullDevice = true;
    }

    LiveEngine engine;
    buildDefaultChain(engine.chain());

    if (!useNullDevice)
    {
        std::cout << "AVISO: se a entrada e a saida forem os dispositivos embutidos,\n"
                  << "       o som realimenta e vira microfonia. Use fones de ouvido.\n";
    }

    DesktopUI ui(engine);

    return ui.run(useNullDevice ? AudioDevice::Mode::Null : AudioDevice::Mode::Duplex,
                  48000.0, 128);
}
