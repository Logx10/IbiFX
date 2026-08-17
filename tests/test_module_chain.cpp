// Testes do ModuleChain.
//
// Cobrem as operações de estrutura (adicionar, remover, reordenar, bypass),
// o processamento na ordem correta e os erros de índice.
//
// A infra de verificação vive em test_helpers.h.

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "Clipper.h"
#include "GainProcessor.h"
#include "ModuleChain.h"
#include "SoftClipper.h"
#include "test_helpers.h"

// Cria um GainProcessor já configurado, para encurtar os testes.
std::unique_ptr<GainProcessor> makeGain(float value)
{
    auto gain = std::make_unique<GainProcessor>();
    gain->setGain(value);
    return gain;
}

// Cria um Clipper já configurado.
std::unique_ptr<Clipper> makeClipper(float threshold)
{
    auto clipper = std::make_unique<Clipper>();
    clipper->setThreshold(threshold);
    return clipper;
}

// ---------------------------------------------------------------------

// Uma cadeia recém-criada não tem módulo nenhum.
void testStartsEmpty()
{
    std::cout << "cadeia comeca vazia\n";

    ModuleChain chain;

    check(chain.empty(), "empty() e verdadeiro");
    check(chain.size() == 0, "size() e zero");
}

// Adicionar transfere a posse e faz a cadeia crescer.
void testAddTakesOwnership()
{
    std::cout << "adicionar modulos\n";

    ModuleChain chain;
    chain.add(makeGain(2.0f));
    chain.add(makeClipper(1.0f));

    check(chain.size() == 2, "cadeia tem 2 modulos");
    check(!chain.empty(), "empty() e falso");
    check(std::string(chain.moduleAt(0).name()) == "Gain", "posicao 0 e o Gain");
    check(std::string(chain.moduleAt(1).name()) == "Clipper", "posicao 1 e o Clipper");
}

// O buffer passa por todos os módulos, na ordem da cadeia.
void testProcessesInOrder()
{
    std::cout << "processa na ordem da cadeia\n";

    ModuleChain chain;
    chain.add(makeGain(4.0f));
    chain.add(makeClipper(1.0f));

    std::vector<float> buffer = {0.1f, 0.5f, -0.9f};
    chain.process(buffer);

    checkClose(buffer[0], 0.4f, "0.1 vira 0.4 e nao e cortada");
    checkClose(buffer[1], 1.0f, "0.5 vira 2.0 e e cortada em 1.0");
    checkClose(buffer[2], -1.0f, "-0.9 vira -3.6 e e cortada em -1.0");
}

// Uma cadeia vazia deixa o buffer intacto.
void testEmptyChainIsTransparent()
{
    std::cout << "cadeia vazia nao altera o buffer\n";

    ModuleChain chain;

    std::vector<float> buffer = {0.3f, -0.7f};
    chain.process(buffer);

    checkClose(buffer[0], 0.3f, "0.3 continua 0.3");
    checkClose(buffer[1], -0.7f, "-0.7 continua -0.7");
}

// Remover tira o módulo e encurta a cadeia.
void testRemove()
{
    std::cout << "remover modulo\n";

    ModuleChain chain;
    chain.add(makeGain(2.0f));
    chain.add(makeClipper(1.0f));
    chain.add(std::make_unique<SoftClipper>());

    chain.remove(1);

    check(chain.size() == 2, "cadeia ficou com 2 modulos");
    check(std::string(chain.moduleAt(0).name()) == "Gain", "posicao 0 continua Gain");
    check(std::string(chain.moduleAt(1).name()) == "SoftClipper", "SoftClipper subiu para a posicao 1");
}

// Reordenar muda a posição sem destruir nada.
void testMoveReorders()
{
    std::cout << "reordenar modulos\n";

    ModuleChain chain;
    chain.add(makeGain(2.0f));
    chain.add(makeClipper(1.0f));
    chain.add(std::make_unique<SoftClipper>());

    chain.move(0, 2);

    check(chain.size() == 3, "cadeia continua com 3 modulos");
    check(std::string(chain.moduleAt(0).name()) == "Clipper", "Clipper virou o primeiro");
    check(std::string(chain.moduleAt(1).name()) == "SoftClipper", "SoftClipper virou o segundo");
    check(std::string(chain.moduleAt(2).name()) == "Gain", "Gain foi para o fim");
}

// Reordenar muda o som, não só a lista.
//
// É o mesmo par gain/clipper das fases anteriores, agora invertido em tempo
// de execução em vez de reescrito no código.
void testReorderingChangesTheResult()
{
    std::cout << "reordenar muda o resultado\n";

    ModuleChain chain;
    chain.add(makeGain(4.0f));
    chain.add(makeClipper(1.0f));

    std::vector<float> cortaDepois = {0.5f};
    chain.process(cortaDepois);

    chain.move(0, 1);

    std::vector<float> cortaAntes = {0.5f};
    chain.process(cortaAntes);

    checkClose(cortaDepois[0], 1.0f, "gain -> clipper: 0.5 vira 1.0 (cortada)");
    checkClose(cortaAntes[0], 2.0f, "clipper -> gain: 0.5 vira 2.0 (estourada)");
}

// Módulo em bypass é pulado, mas continua na cadeia.
void testBypassSkipsModule()
{
    std::cout << "bypass pula o modulo\n";

    ModuleChain chain;
    chain.add(makeGain(4.0f));
    chain.add(makeClipper(1.0f));

    chain.setBypassed(1, true);

    check(chain.isBypassed(1), "posicao 1 esta em bypass");
    check(chain.size() == 2, "o modulo continua na cadeia");

    std::vector<float> buffer = {0.5f};
    chain.process(buffer);

    checkClose(buffer[0], 2.0f, "sem o clipper, 0.5 vira 2.0 e estoura");
}

// Desligar o bypass devolve o módulo à cadeia.
void testBypassCanBeUndone()
{
    std::cout << "desligar o bypass\n";

    ModuleChain chain;
    chain.add(makeGain(4.0f));
    chain.add(makeClipper(1.0f));

    chain.setBypassed(1, true);
    chain.setBypassed(1, false);

    check(!chain.isBypassed(1), "posicao 1 saiu do bypass");

    std::vector<float> buffer = {0.5f};
    chain.process(buffer);

    checkClose(buffer[0], 1.0f, "com o clipper de volta, 0.5 vira 1.0");
}

// Módulos novos entram ativos.
void testModulesStartActive()
{
    std::cout << "modulo novo entra ativo\n";

    ModuleChain chain;
    chain.add(makeGain(2.0f));

    check(!chain.isBypassed(0), "modulo recem-adicionado nao esta em bypass");
}

// clear() esvazia a cadeia.
void testClear()
{
    std::cout << "esvaziar a cadeia\n";

    ModuleChain chain;
    chain.add(makeGain(2.0f));
    chain.add(makeClipper(1.0f));

    chain.clear();

    check(chain.empty(), "cadeia ficou vazia");
    check(chain.size() == 0, "size() voltou a zero");
}

// Índice inválido lança, em vez de falhar em silêncio.
//
// O §71 do AI_GUIDELINES pede erro explícito e diagnosticável. Estas são
// chamadas do domínio de controle, nunca da thread de áudio, então lançar
// exceção aqui não tem implicação de tempo real.
void testInvalidIndexThrows()
{
    std::cout << "indice invalido lanca excecao\n";

    ModuleChain chain;
    chain.add(makeGain(2.0f));

    bool lancouNoRemove = false;
    try
    {
        chain.remove(5);
    }
    catch (const std::out_of_range&)
    {
        lancouNoRemove = true;
    }

    bool lancouNoBypass = false;
    try
    {
        chain.setBypassed(3, true);
    }
    catch (const std::out_of_range&)
    {
        lancouNoBypass = true;
    }

    bool lancouNoMove = false;
    try
    {
        chain.move(0, 9);
    }
    catch (const std::out_of_range&)
    {
        lancouNoMove = true;
    }

    check(lancouNoRemove, "remove com indice invalido lanca out_of_range");
    check(lancouNoBypass, "setBypassed com indice invalido lanca out_of_range");
    check(lancouNoMove, "move com indice invalido lanca out_of_range");
    check(chain.size() == 1, "a cadeia continua intacta apos os erros");
}

// Adicionar um ponteiro nulo é erro de programação, e é sinalizado.
void testAddNullThrows()
{
    std::cout << "adicionar nulo lanca excecao\n";

    ModuleChain chain;

    bool lancou = false;
    try
    {
        chain.add(nullptr);
    }
    catch (const std::invalid_argument&)
    {
        lancou = true;
    }

    check(lancou, "add(nullptr) lanca invalid_argument");
    check(chain.empty(), "a cadeia continua vazia");
}

int main()
{
    std::cout << "\n=== testes do ModuleChain ===\n\n";

    testStartsEmpty();
    testAddTakesOwnership();
    testProcessesInOrder();
    testEmptyChainIsTransparent();
    testRemove();
    testMoveReorders();
    testReorderingChangesTheResult();
    testBypassSkipsModule();
    testBypassCanBeUndone();
    testModulesStartActive();
    testClear();
    testInvalidIndexThrows();
    testAddNullThrows();

    return reportResults();
}
