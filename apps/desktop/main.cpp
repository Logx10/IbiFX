#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "ChainArgs.h"
#include "DesktopUI.h"
#include "LiveEngine.h"
#include "PedalboardChain.h"
#include "Preset.h"
#include "PresetManager.h"

// Janela da Fase 12 (AI_GUIDELINES §36): o pedalboard ligado ao LiveEngine
// de verdade, via Dear ImGui + SDL3 (ADR 0001).
//
// ACEITA AS MESMAS OPÇÕES DE CADEIA DO CLI
// --cabinet, --tonestack, --preamp, --poweramp, --gain... são
// interpretadas pelo mesmo parseSettings() de ChainArgs.h que --live e
// --ui já usam — não uma cópia da lista de flags. O DesktopUI não precisou
// mudar uma linha: ele já desenha qualquer módulo que a cadeia tiver,
// genericamente, então ToneStack/Preamp/PowerAmp/Cabinet viram pedais na
// janela assim que entram na cadeia, do mesmo jeito que Gain ou Delay.
//
// --stomp/--amp/--cab/--rack montam um rig da GearLibrary, e a janela abre
// com ele já ativo no navegador de equipamentos.
//
// --preset CAMINHO carrega um preset pronto no lugar das flags acima,
// igual ao processamento de arquivo. --save-preset não se aplica aqui —
// salvar faz sentido depois de ajustar os knobs com a janela aberta, não
// antes dela existir; isso fica para quando a Fase 12 ganhar um seletor
// de preset na própria UI.
int main(int argc, char** argv)
{
    bool useNullDevice = false;
    std::vector<std::string> chainArgs;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--null")
            useNullDevice = true;
        else
            chainArgs.push_back(arg);
    }

    try
    {
        LiveEngine engine;
        const CliOptions options = parseSettings(chainArgs);

        // Preset, rig ou flags — ver a precedência em ChainArgs.h.
        std::cout << buildChainFromOptions(options, engine.chain()) << "\n";

        if (!useNullDevice)
        {
            std::cout << "AVISO: se a entrada e a saida forem os dispositivos embutidos,\n"
                      << "       o som realimenta e vira microfonia. Use fones de ouvido.\n";
        }

        DesktopUI ui(engine, options.irsDirectory);

        // Com --amp/--stomp..., a janela abre com esse rig ativo no
        // navegador. O --preset vence o rig (ver ChainArgs.h), então aí não.
        if (options.usesRig() && options.loadPresetPath.empty())
        {
            GearLibrary library;
            library.scanImpulseResponses(options.irsDirectory);
            ui.adoptRig(resolveRig(options, library));
        }

        return ui.run(useNullDevice ? AudioDevice::Mode::Null : AudioDevice::Mode::Duplex,
                      48000.0, 128);
    }
    catch (const std::exception& error)
    {
        std::cerr << "erro: " << error.what() << "\n";
        return 1;
    }
}
