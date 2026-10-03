#pragma once

#include <string>
#include <vector>

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
