// Testes do ChainArgs — só da parte de rig (--stomp/--amp/--cab/--rack) e
// da precedência de buildChainFromOptions(). As flags clássicas já são
// exercitadas de ponta a ponta pelo CLI.
//
// A infra de verificação vive em test_helpers.h.

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ChainArgs.h"
#include "ModuleChain.h"
#include "Preset.h"
#include "PresetManager.h"
#include "test_helpers.h"

namespace
{
bool parseThrows(const std::vector<std::string>& args)
{
    try
    {
        parseSettings(args);
    }
    catch (const std::runtime_error&)
    {
        return true;
    }
    return false;
}

bool chainHas(const ModuleChain& chain, const std::string& type)
{
    for (std::size_t i = 0; i < chain.size(); ++i)
    {
        if (chain.moduleAt(i).name() == type)
            return true;
    }
    return false;
}
}

// ---------------------------------------------------------------------

void testRigFlagsAreParsedInOrder()
{
    std::cout << "flags de rig sao lidas, stomps e rack na ordem dada\n";

    const CliOptions options = parseSettings(std::vector<std::string>{
        "--stomp", "gate", "--amp", "brit-800", "--stomp", "round-fuzz",
        "--rack", "slapback", "--rack", "hall", "--irs", "minhas-irs"});

    check(options.usesRig(), "usesRig()");
    check(options.rig.stomps == std::vector<std::string>{"gate", "round-fuzz"}, "stomps na ordem");
    check(options.rig.amp == "brit-800", "amp");
    check(options.rig.rack == std::vector<std::string>{"slapback", "hall"}, "rack na ordem");
    check(options.irsDirectory == "minhas-irs", "--irs");
}

void testNoRigFlagsMeansNoRig()
{
    std::cout << "sem flags de rig, usesRig() e falso\n";

    check(!parseSettings(std::vector<std::string>{"--gain", "2"}).usesRig(), "so flags classicas");
}

void testRigFlagWithoutValueThrows()
{
    std::cout << "flag de rig sem valor lanca\n";

    check(parseThrows({"--amp"}), "--amp sem id");
    check(parseThrows({"--stomp", "gate", "--rack"}), "--rack sem id");
}

void testShortAndFullIdsResolve()
{
    std::cout << "ids curtos e completos resolvem para a forma completa\n";

    GearLibrary library;
    const CliOptions options = parseSettings(std::vector<std::string>{
        "--stomp", "stomp.gate", "--amp", "tweed-59", "--rack", "room"});

    const Rig rig = resolveRig(options, library);

    check(rig.stomps.front() == "stomp.gate", "completo continua igual");
    check(rig.amp == "amp.tweed-59", "curto ganha o prefixo");
    check(rig.rack.front() == "rack.room", "rack curto");
}

void testUnknownIdThrows()
{
    std::cout << "id desconhecido lanca\n";

    GearLibrary library;
    bool threw = false;

    try
    {
        resolveRig(parseSettings(std::vector<std::string>{"--amp", "nao-existe"}), library);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    check(threw, "resolveRig lancou");
}

void testRigWinsOverClassicFlags()
{
    std::cout << "rig vence as flags classicas\n";

    // --no-drive na cadeia clássica tiraria a distorção; com rig, ela é
    // ignorada e o Preamp do ampli entra.
    const CliOptions options = parseSettings(std::vector<std::string>{
        "--no-drive", "--amp", "brit-800"});

    ModuleChain chain;
    const std::string origin = buildChainFromOptions(options, chain);

    check(origin.find("rig") != std::string::npos, "relata que veio do rig");
    check(chainHas(chain, "Preamp"), "o ampli entrou");
    check(!chainHas(chain, "Delay"), "o delay da cadeia classica nao entrou");
}

void testPresetWinsOverRig()
{
    std::cout << "preset vence o rig\n";

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "ibifx_test_chain_args.ibifxpreset";

    Preset saved;
    saved.name = "So delay";
    Preset::ModuleState delay;
    delay.type = "Delay";
    saved.modules.push_back(delay);
    preset::save(saved, path.string());

    const CliOptions options = parseSettings(std::vector<std::string>{
        "--amp", "brit-800", "--preset", path.string()});

    ModuleChain chain;
    buildChainFromOptions(options, chain);

    check(chain.size() == 1 && chainHas(chain, "Delay"), "a cadeia e a do preset");

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

int main()
{
    std::cout << "\n=== testes do ChainArgs (rig) ===\n\n";

    testRigFlagsAreParsedInOrder();
    testNoRigFlagsMeansNoRig();
    testRigFlagWithoutValueThrows();
    testShortAndFullIdsResolve();
    testUnknownIdThrows();
    testRigWinsOverClassicFlags();
    testPresetWinsOverRig();

    return reportResults();
}
