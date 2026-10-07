// Testes do contrato AudioModule.
//
// Os outros arquivos testam o que cada módulo calcula. Este testa o
// mecanismo que os une: a chamada virtual chega na implementação certa,
// uma cadeia heterogênea processa na ordem correta, e destruir por ponteiro
// para a base é seguro.
//
// A infra de verificação vive em test_helpers.h.

#include <iostream>
#include <memory>
#include <utility>
#include <stdexcept>
#include <string>
#include <vector>

#include "AudioModule.h"
#include "Clipper.h"
#include "GainProcessor.h"
#include "SoftClipper.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------

// A chamada por ponteiro para a base executa a versão do objeto real.
//
// É o coração do polimorfismo: o ponteiro é AudioModule*, mas quem roda é o
// process() do GainProcessor. Sem virtual, nada disso funcionaria.
void testVirtualCallReachesDerived()
{
    std::cout << "chamada virtual chega na implementacao certa\n";

    GainProcessor gain;
    gain.setGain(2.0f);

    AudioModule* module = &gain;

    std::vector<float> buffer = {0.5f, -0.25f};
    module->process(buffer);

    checkClose(buffer[0], 1.0f, "0.5 com ganho 2.0 vira 1.0");
    checkClose(buffer[1], -0.5f, "-0.25 com ganho 2.0 vira -0.5");

    check(std::string(module->name()) == "Gain", "name() devolve Gain");
}

// Uma cadeia com módulos de tipos diferentes, processada num laço só.
//
// O laço não sabe quem são os módulos. É exatamente o que o main() faz, e o
// que a Fase 4 vai transformar em cadeia montada em tempo de execução.
void testHeterogeneousChain()
{
    std::cout << "cadeia heterogenea num laco so\n";

    GainProcessor gain;
    gain.setGain(4.0f);

    Clipper clipper;
    clipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    clipper.setThreshold(1.0f);

    std::vector<AudioModule*> chain = {&gain, &clipper};

    std::vector<float> buffer = {0.1f, 0.5f, -0.9f};

    for (AudioModule* module : chain)
    {
        module->process(buffer);
    }

    checkClose(buffer[0], 0.4f, "0.1 passa pelo ganho e nao e cortada");
    checkClose(buffer[1], 1.0f, "0.5 vira 2.0 no ganho e e cortada em 1.0");
    checkClose(buffer[2], -1.0f, "-0.9 vira -3.6 no ganho e e cortada em -1.0");
}

// A ordem da cadeia muda o resultado.
//
// Mesmos dois módulos, mesmos parâmetros, ordens opostas. Registra em teste
// aquilo que o demo mostra na tela.
void testChainOrderMatters()
{
    std::cout << "a ordem da cadeia muda o resultado\n";

    GainProcessor gain;
    gain.setGain(4.0f);

    Clipper clipper;
    clipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    clipper.setThreshold(1.0f);

    std::vector<AudioModule*> driveOrder = {&gain, &clipper};
    std::vector<AudioModule*> inverseOrder = {&clipper, &gain};

    std::vector<float> comCorteDepois = {0.5f};
    std::vector<float> comCorteAntes = {0.5f};

    for (AudioModule* module : driveOrder)
    {
        module->process(comCorteDepois);
    }

    for (AudioModule* module : inverseOrder)
    {
        module->process(comCorteAntes);
    }

    checkClose(comCorteDepois[0], 1.0f, "gain -> clipper: 0.5 vira 1.0 (cortada)");
    checkClose(comCorteAntes[0], 2.0f, "clipper -> gain: 0.5 vira 2.0 (estourada)");

    check(comCorteDepois[0] != comCorteAntes[0], "as duas ordens dao resultados diferentes");
}

// Cada módulo devolve o seu próprio nome pela mesma chamada.
void testEachModuleReportsItsName()
{
    std::cout << "cada modulo devolve o proprio nome\n";

    GainProcessor gain;
    Clipper clipper;
    clipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    SoftClipper softClipper;
    softClipper.setOversampling(false);  // curva pura, sem o atraso do filtro

    std::vector<AudioModule*> chain = {&gain, &clipper, &softClipper};

    check(std::string(chain[0]->name()) == "Gain", "primeiro e Gain");
    check(std::string(chain[1]->name()) == "Clipper", "segundo e Clipper");
    check(std::string(chain[2]->name()) == "SoftClipper", "terceiro e SoftClipper");
}

// Destruir por ponteiro para a base roda o destrutor do derivado.
//
// É o que o destrutor virtual garante. Sem ele isto seria comportamento
// indefinido — hoje inofensivo, porque nenhum módulo aloca nada, mas o
// Delay vai alocar.
void testDeletingThroughBasePointerIsSafe()
{
    std::cout << "destruicao por ponteiro para a base\n";

    auto softClipper = std::make_unique<SoftClipper>();
    softClipper->setOversampling(false);  // curva pura, sem o atraso do filtro
    std::unique_ptr<AudioModule> module = std::move(softClipper);

    std::vector<float> buffer = {0.5f};
    module->process(buffer);

    checkClose(buffer[0], 0.462117165f, "SoftClipper criado na heap processa normalmente");

    module.reset();

    check(module == nullptr, "destruido sem vazamento (o unique_ptr cuidou disso)");
}

// Cada módulo expõe seus parâmetros pela mesma interface.
void testModulesExposeParameters()
{
    std::cout << "modulos expoem parametros\n";

    GainProcessor gain;
    Clipper clipper;
    clipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    SoftClipper softClipper;
    softClipper.setOversampling(false);  // curva pura, sem o atraso do filtro

    check(gain.parameterCount() == 1, "Gain tem 1 parametro");
    check(clipper.parameterCount() == 1, "Clipper tem 1 parametro");
    check(softClipper.parameterCount() == 1, "SoftClipper tem 1 parametro");

    check(gain.parameterAt(0).id() == "gain", "o parametro do Gain se chama gain");
    check(clipper.parameterAt(0).id() == "threshold", "o do Clipper se chama threshold");
    check(softClipper.parameterAt(0).id() == "drive", "o do SoftClipper se chama drive");
}

// O setter tipado e o parâmetro são a mesma coisa, não duas cópias.
//
// O parâmetro é o dono do valor; setGain() é só um atalho conveniente. Se
// fossem estados separados, eles divergiriam — e é isso que este teste barra.
void testTypedSetterAndParameterShareState()
{
    std::cout << "setter tipado e parametro sao o mesmo estado\n";

    GainProcessor gain;

    gain.setGain(3.0f);
    checkClose(gain.parameterAt(0).value(), 3.0f, "setGain aparece no parametro");

    gain.parameterAt(0).setValue(-2.0f);
    checkClose(gain.gain(), -2.0f, "escrever no parametro aparece em gain()");
}

// Ajustar um módulo sem conhecer o tipo concreto dele.
//
// É o ponto da fase: quem só tem um AudioModule* e um id de texto consegue
// mudar o som. Preset, MIDI e UI vivem exatamente nessa situação.
void testGenericParameterAccessByName()
{
    std::cout << "ajuste generico por id\n";

    GainProcessor gain;
    AudioModule* module = &gain;

    Parameter* parameter = module->findParameter("gain");
    check(parameter != nullptr, "findParameter encontrou o parametro");

    if (parameter != nullptr)
    {
        parameter->setValue(2.0f);
    }

    std::vector<float> buffer = {0.5f};
    module->process(buffer);

    checkClose(buffer[0], 1.0f, "o ajuste generico mudou o processamento");
}

// Procurar um id inexistente devolve nullptr, sem lançar.
//
// Ausência não é erro: quem procura pode legitimamente não saber se aquele
// módulo tem o parâmetro.
void testFindParameterReturnsNullWhenMissing()
{
    std::cout << "id inexistente devolve nullptr\n";

    Clipper clipper;
    clipper.setOversampling(false);  // curva pura, sem o atraso do filtro

    check(clipper.findParameter("drive") == nullptr, "Clipper nao tem drive");
    check(clipper.findParameter("threshold") != nullptr, "mas tem threshold");
}

// Índice de parâmetro fora da lista lança.
void testParameterIndexOutOfRangeThrows()
{
    std::cout << "indice de parametro invalido lanca\n";

    GainProcessor gain;

    bool lancou = false;
    try
    {
        gain.parameterAt(7);
    }
    catch (const std::out_of_range&)
    {
        lancou = true;
    }

    check(lancou, "parameterAt(7) lanca out_of_range");
}

// resetParameters() devolve todo o módulo ao estado de fábrica.
void testResetParameters()
{
    std::cout << "reset de parametros\n";

    SoftClipper softClipper;
    softClipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    softClipper.setDrive(50.0f);

    softClipper.resetParameters();

    checkClose(softClipper.drive(), 1.0f, "drive voltou para o padrao 1.0");
}

// A faixa do parâmetro protege o módulo de valores impossíveis.
//
// O teto negativo do Clipper era um risco documentado; agora a faixa começa
// em 0 e o problema deixa de existir.
void testParameterRangeProtectsTheModule()
{
    std::cout << "a faixa protege o modulo\n";

    Clipper clipper;
    clipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    clipper.setThreshold(-1.0f);

    checkClose(clipper.threshold(), 0.0f, "teto negativo vira 0.0");

    GainProcessor gain;
    gain.setGain(1000.0f);

    checkClose(gain.gain(), 8.0f, "ganho de 1000 para no maximo da faixa");
}

int main()
{
    std::cout << "\n=== testes do AudioModule ===\n\n";

    testVirtualCallReachesDerived();
    testHeterogeneousChain();
    testChainOrderMatters();
    testEachModuleReportsItsName();
    testDeletingThroughBasePointerIsSafe();
    testModulesExposeParameters();
    testTypedSetterAndParameterShareState();
    testGenericParameterAccessByName();
    testFindParameterReturnsNullWhenMissing();
    testParameterIndexOutOfRangeThrows();
    testResetParameters();
    testParameterRangeProtectsTheModule();

    return reportResults();
}
