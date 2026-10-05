#pragma once

#include <string>
#include <vector>

#include "GearLibrary.h"
#include "PedalboardChain.h"

// CliOptions / parseSettings — interpretação de flags de linha de comando
// em ChainSettings, compartilhada por mais de um frontend: o CLI, em
// src/main.cpp (processamento de arquivo, --live, --ui), e a janela
// desktop, em apps/desktop/main.cpp. Os dois aceitam as mesmas opções
// (--cabinet, --tonestack, --gain...) pra montar a MESMA cadeia
// configurável, e duplicar esta lista arriscaria os dois divergirem sem
// ninguém notar — mesmo motivo que já tinha levado ChainSettings/
// buildChain() para PedalboardChain.h.
struct CliOptions
{
    ChainSettings chain;

    // Vazio = nenhum dos dois. Carregar um preset substitui todas as
    // flags de chain na montagem da cadeia; salvar grava a cadeia já
    // montada (vinda das flags ou do preset carregado) antes de
    // processar o arquivo.
    std::string loadPresetPath;
    std::string savePresetPath;

    // --stomp/--amp/--cab/--rack: um rig da GearLibrary, por id. Os ids
    // ficam como foram digitados (curtos ou completos); quem resolve é
    // buildChainFromOptions().
    Rig rig;

    // Onde procurar os cabinets do --cab (--irs PASTA).
    std::string irsDirectory = "irs";

    // true se alguma flag de rig foi usada.
    bool usesRig() const;
};

// Interpreta as opções de linha de comando contidas em args.
//
// Recebe um std::vector, não argc/argv direto: isso permite reaproveitar
// o mesmo parser em mais de um lugar (processamento de arquivo, --live,
// --ui, a janela desktop) mesmo quando cada um precisa filtrar ou
// reordenar os argumentos originais antes — por exemplo, --live tem
// posicionais (segundos, bloco) que não existem aqui.
//
// Lança com mensagem clara em caso de opção desconhecida, valor faltando ou
// número inválido. Falhar aqui é barato; falhar depois, com um parâmetro
// silenciosamente errado, custaria uma sessão de depuração.
CliOptions parseSettings(const std::vector<std::string>& args);

// Atalho para quando as opções já vêm do argv original, sem filtragem
// prévia — o caso do processamento de arquivo.
CliOptions parseSettings(int argc, char** argv, int first);

// Monta a cadeia que as opções pedem, nesta ordem de precedência:
//
//     --preset  >  rig (--stomp/--amp/--cab/--rack)  >  flags clássicas
//
// Quem vence ignora os demais — mesma regra que o --preset já tinha. Um
// lugar só para isso: antes, cada frontend repetia o "if preset... else
// buildChain", e --live/--ui nem olhavam o --preset.
//
// Ids de rig podem vir curtos ("brit-800") ou completos ("amp.brit-800").
// Devolve uma linha curta dizendo de onde a cadeia veio, para o frontend
// mostrar. Lança se o preset, um id ou uma IR não existirem.
std::string buildChainFromOptions(const CliOptions& options, ModuleChain& chain);

// O rig das opções com os ids já resolvidos para a forma completa. Lança
// se algum não existir em library.
Rig resolveRig(const CliOptions& options, const GearLibrary& library);
