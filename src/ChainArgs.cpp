#include "ChainArgs.h"

#include <stdexcept>

CliOptions parseSettings(const std::vector<std::string>& args)
{
    CliOptions settings;

    for (std::size_t i = 0; i < args.size(); ++i)
    {
        const std::string& option = args[i];

        if (option == "--no-gate")
        {
            settings.chain.useGate = false;
            continue;
        }

        if (option == "--no-compressor")
        {
            settings.chain.useCompressor = false;
            continue;
        }

        if (option == "--no-highpass")
        {
            settings.chain.useHighPass = false;
            continue;
        }

        if (option == "--no-drive")
        {
            settings.chain.useDrive = false;
            continue;
        }

        if (option == "--no-delay")
        {
            settings.chain.useDelay = false;
            continue;
        }

        if (option == "--no-reverb")
        {
            settings.chain.useReverb = false;
            continue;
        }

        if (option == "--asymmetric")
        {
            settings.chain.useAsymmetric = true;
            continue;
        }

        if (option == "--preamp")
        {
            settings.chain.usePreamp = true;
            continue;
        }

        if (option == "--tonestack")
        {
            settings.chain.useToneStack = true;
            continue;
        }

        if (option == "--poweramp")
        {
            settings.chain.usePowerAmp = true;
            continue;
        }

        // --cabinet recebe um CAMINHO DE ARQUIVO, nao um numero — precisa
        // ser tratado antes do laco generico abaixo, que tenta ler todo
        // valor seguinte como float.
        if (option == "--cabinet")
        {
            if (i + 1 >= args.size())
            {
                throw std::runtime_error("a opcao --cabinet precisa do caminho de um arquivo .wav");
            }

            settings.chain.cabinetIRPath = args[++i];
            continue;
        }

        // --preset e --save-preset recebem um CAMINHO DE ARQUIVO, pelo
        // mesmo motivo do --cabinet acima: precisam ser tratados antes do
        // laco generico, que tenta ler todo valor seguinte como float.
        if (option == "--preset")
        {
            if (i + 1 >= args.size())
            {
                throw std::runtime_error("a opcao --preset precisa do caminho de um arquivo de preset");
            }

            settings.loadPresetPath = args[++i];
            continue;
        }

        if (option == "--save-preset")
        {
            if (i + 1 >= args.size())
            {
                throw std::runtime_error("a opcao --save-preset precisa do caminho de um arquivo de preset");
            }

            settings.savePresetPath = args[++i];
            continue;
        }

        if (i + 1 >= args.size())
        {
            throw std::runtime_error("a opcao " + option + " precisa de um valor");
        }

        const std::string raw = args[++i];
        float value = 0.0f;

        try
        {
            value = std::stof(raw);
        }
        catch (const std::exception&)
        {
            throw std::runtime_error("valor invalido para " + option + ": '" + raw + "'");
        }

        ChainSettings& chain = settings.chain;

        if (option == "--bias")              chain.bias = value;
        else if (option == "--preamp-drive")   chain.preampDrive = value;
        else if (option == "--tone-bass")      chain.toneBass = value;
        else if (option == "--tone-mid")       chain.toneMid = value;
        else if (option == "--tone-treble")    chain.toneTreble = value;
        else if (option == "--gate-threshold") chain.gateThreshold = value;
        else if (option == "--gate-release") chain.gateRelease = value;
        else if (option == "--comp-threshold") chain.compThreshold = value;
        else if (option == "--comp-ratio")     chain.compRatio = value;
        else if (option == "--comp-attack")    chain.compAttack = value;
        else if (option == "--comp-release")   chain.compRelease = value;
        else if (option == "--highpass")     chain.highPass = value;
        else if (option == "--gain")         chain.gain = value;
        else if (option == "--drive")        chain.drive = value;
        else if (option == "--time")         chain.delayTime = value;
        else if (option == "--feedback")     chain.feedback = value;
        else if (option == "--mix")          chain.mix = value;
        else if (option == "--reverb-decay")    chain.reverbDecay = value;
        else if (option == "--reverb-damping")  chain.reverbDamping = value;
        else if (option == "--reverb-mix")      chain.reverbMix = value;
        else if (option == "--poweramp-drive")  chain.powerAmpDrive = value;
        else if (option == "--poweramp-sag")    chain.powerAmpSag = value;
        else if (option == "--cabinet-mix")     chain.cabinetMix = value;
        else throw std::runtime_error("opcao desconhecida: " + option);
    }

    return settings;
}

// Atalho para quando as opções já vêm do argv original, sem filtragem
// prévia — o caso do processamento de arquivo.
CliOptions parseSettings(int argc, char** argv, int first)
{
    std::vector<std::string> args;

    for (int i = first; i < argc; ++i)
        args.push_back(argv[i]);

    return parseSettings(args);
}
