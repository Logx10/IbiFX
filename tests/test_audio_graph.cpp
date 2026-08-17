// Testes do AudioGraph.
//
// Cobrem estrutura (nós, conexões, remoção), ordenação topológica, rejeição
// de ciclos e o roteamento que a cadeia linear não consegue fazer: dividir um
// sinal em dois caminhos e somá-los de volta.
//
// A infra de verificação vive em test_helpers.h.

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "AudioGraph.h"
#include "Delay.h"
#include "GainProcessor.h"
#include "test_helpers.h"

namespace
{
std::unique_ptr<GainProcessor> makeGain(float value)
{
    auto gain = std::make_unique<GainProcessor>();
    gain->setGain(value);
    return gain;
}

// Posição de um nó na ordem de processamento; -1 se não estiver nela.
int positionOf(const std::vector<AudioGraph::NodeId>& order, AudioGraph::NodeId id)
{
    const auto it = std::find(order.begin(), order.end(), id);

    if (it == order.end())
    {
        return -1;
    }

    return static_cast<int>(std::distance(order.begin(), it));
}
}

// ---------------------------------------------------------------------

// Um grafo recém-criado está vazio.
void testStartsEmpty()
{
    std::cout << "grafo comeca vazio\n";

    AudioGraph graph;

    check(graph.empty(), "empty() e verdadeiro");
    check(graph.nodeCount() == 0, "nodeCount() e zero");
}

// Cada nó recebe um identificador próprio.
void testAddNodeReturnsDistinctIds()
{
    std::cout << "cada no recebe um id proprio\n";

    AudioGraph graph;

    const auto a = graph.addNode(makeGain(1.0f));
    const auto b = graph.addNode(makeGain(1.0f));

    check(a != b, "os ids sao diferentes");
    check(graph.nodeCount() == 2, "o grafo tem 2 nos");
    check(graph.hasNode(a) && graph.hasNode(b), "os dois nos existem");
}

// O id sobrevive à remoção de outros nós.
//
// Importa porque presets e mapeamentos MIDI vão guardar esse número.
void testIdsSurviveRemovalOfOtherNodes()
{
    std::cout << "o id sobrevive a remocao de outros\n";

    AudioGraph graph;

    const auto a = graph.addNode(makeGain(2.0f));
    const auto b = graph.addNode(makeGain(3.0f));
    const auto c = graph.addNode(makeGain(4.0f));

    graph.removeNode(b);

    check(graph.hasNode(a), "o primeiro continua existindo");
    check(!graph.hasNode(b), "o removido sumiu");
    check(graph.hasNode(c), "o terceiro continua com o mesmo id");
}

// Remover um nó apaga as conexões que o envolviam.
//
// Sem isso restariam conexões apontando para o vazio, e o processamento
// leria de um nó inexistente.
void testRemovingNodeClearsItsConnections()
{
    std::cout << "remover um no limpa as conexoes dele\n";

    AudioGraph graph;

    const auto a = graph.addNode(makeGain(2.0f));
    const auto b = graph.addNode(makeGain(2.0f));
    const auto c = graph.addNode(makeGain(2.0f));

    graph.connect(a, b);
    graph.connect(b, c);

    graph.removeNode(b);

    check(graph.nodeCount() == 2, "restaram 2 nos");

    // Se a conexão b -> c tivesse sobrado, a ordem ainda esperaria por b.
    const auto order = graph.processingOrder();
    check(order.size() == 2, "a ordem tem os 2 nos restantes");
}

// A ordem de processamento respeita as dependências.
void testProcessingOrderRespectsDependencies()
{
    std::cout << "a ordem respeita as dependencias\n";

    AudioGraph graph;

    const auto primeiro = graph.addNode(makeGain(1.0f));
    const auto segundo = graph.addNode(makeGain(1.0f));
    const auto terceiro = graph.addNode(makeGain(1.0f));

    // Conectados fora de ordem de propósito: a ordenação não pode depender
    // da sequência em que as conexões foram criadas.
    graph.connect(segundo, terceiro);
    graph.connect(primeiro, segundo);

    const auto order = graph.processingOrder();

    check(order.size() == 3, "todos os 3 nos entraram na ordem");
    check(positionOf(order, primeiro) < positionOf(order, segundo), "o primeiro vem antes do segundo");
    check(positionOf(order, segundo) < positionOf(order, terceiro), "o segundo vem antes do terceiro");
}

// Nós sem ligação entre si podem sair em qualquer ordem, mas todos saem.
void testIndependentNodesAllAppearInOrder()
{
    std::cout << "nos independentes\n";

    AudioGraph graph;

    graph.addNode(makeGain(1.0f));
    graph.addNode(makeGain(1.0f));
    graph.addNode(makeGain(1.0f));

    check(graph.processingOrder().size() == 3, "os 3 nos entram na ordem");
}

// Uma conexão que fecharia um ciclo é rejeitada.
//
// Ciclo é realimentação sem atraso: para processar A é preciso B, e para
// processar B é preciso A. Não existe ordem válida, e em áudio isso estoura.
void testCycleIsRejected()
{
    std::cout << "ciclo e rejeitado\n";

    AudioGraph graph;

    const auto a = graph.addNode(makeGain(1.0f));
    const auto b = graph.addNode(makeGain(1.0f));
    const auto c = graph.addNode(makeGain(1.0f));

    graph.connect(a, b);
    graph.connect(b, c);

    check(graph.wouldCreateCycle(c, a), "c -> a fecharia um ciclo");

    bool lancou = false;
    try
    {
        graph.connect(c, a);
    }
    catch (const std::invalid_argument&)
    {
        lancou = true;
    }

    check(lancou, "connect recusou a conexao");
    check(graph.processingOrder().size() == 3, "o grafo continua valido apos a recusa");
}

// Um nó não pode alimentar a si mesmo.
void testSelfConnectionIsRejected()
{
    std::cout << "no nao alimenta a si mesmo\n";

    AudioGraph graph;
    const auto a = graph.addNode(makeGain(1.0f));

    check(graph.wouldCreateCycle(a, a), "a -> a e ciclo");

    bool lancou = false;
    try
    {
        graph.connect(a, a);
    }
    catch (const std::invalid_argument&)
    {
        lancou = true;
    }

    check(lancou, "conexao consigo mesmo lanca");
}

// Conexão duplicada é recusada.
void testDuplicateConnectionIsRejected()
{
    std::cout << "conexao duplicada\n";

    AudioGraph graph;

    const auto a = graph.addNode(makeGain(1.0f));
    const auto b = graph.addNode(makeGain(1.0f));

    graph.connect(a, b);

    bool lancou = false;
    try
    {
        graph.connect(a, b);
    }
    catch (const std::invalid_argument&)
    {
        lancou = true;
    }

    check(lancou, "a mesma conexao duas vezes lanca");
}

// Id inexistente lança em todas as operações estruturais.
void testInvalidIdsThrow()
{
    std::cout << "id inexistente lanca\n";

    AudioGraph graph;
    const auto a = graph.addNode(makeGain(1.0f));

    int lancamentos = 0;

    try { graph.connect(a, 999); } catch (const std::out_of_range&) { ++lancamentos; }
    try { graph.removeNode(999); } catch (const std::out_of_range&) { ++lancamentos; }
    try { graph.moduleAt(999); } catch (const std::out_of_range&) { ++lancamentos; }
    try { graph.connectFromInput(999); } catch (const std::out_of_range&) { ++lancamentos; }

    check(lancamentos == 4, "as 4 operacoes lancaram out_of_range");
}

// Uma cadeia linear no grafo se comporta como o ModuleChain.
void testLinearChainThroughGraph()
{
    std::cout << "cadeia linear pelo grafo\n";

    AudioGraph graph;

    const auto dobra = graph.addNode(makeGain(2.0f));
    const auto triplica = graph.addNode(makeGain(3.0f));

    graph.connectFromInput(dobra);
    graph.connect(dobra, triplica);
    graph.connectToOutput(triplica);

    graph.prepare(1000.0, 8);

    std::vector<float> buffer = {1.0f, 0.5f};
    graph.process(buffer);

    checkClose(buffer[0], 6.0f, "1.0 passou por x2 e x3 e virou 6.0");
    checkClose(buffer[1], 3.0f, "0.5 virou 3.0");
}

// O QUE A CADEIA LINEAR NÃO CONSEGUE FAZER.
//
// O sinal se divide em dois caminhos independentes e volta a se somar. Numa
// fila isso seria impossível: o segundo caminho receberia a saída do
// primeiro em vez do sinal original.
void testParallelPathsAreSummed()
{
    std::cout << "caminhos paralelos sao somados\n";

    AudioGraph graph;

    const auto caminhoA = graph.addNode(makeGain(2.0f));
    const auto caminhoB = graph.addNode(makeGain(3.0f));

    graph.connectFromInput(caminhoA);
    graph.connectFromInput(caminhoB);
    graph.connectToOutput(caminhoA);
    graph.connectToOutput(caminhoB);

    graph.prepare(1000.0, 8);

    std::vector<float> buffer = {1.0f, 0.5f};
    graph.process(buffer);

    // Cada caminho recebeu o sinal ORIGINAL, não a saída do outro.
    checkClose(buffer[0], 5.0f, "1.0 virou 2.0 + 3.0 = 5.0");
    checkClose(buffer[1], 2.5f, "0.5 virou 1.0 + 1.5 = 2.5");
}

// Um nó que recebe de vários soma as entradas.
void testMultipleSourcesAreMixed()
{
    std::cout << "varias entradas sao somadas num no\n";

    AudioGraph graph;

    const auto caminhoA = graph.addNode(makeGain(2.0f));
    const auto caminhoB = graph.addNode(makeGain(4.0f));
    const auto mixer = graph.addNode(makeGain(1.0f));

    graph.connectFromInput(caminhoA);
    graph.connectFromInput(caminhoB);
    graph.connect(caminhoA, mixer);
    graph.connect(caminhoB, mixer);
    graph.connectToOutput(mixer);

    graph.prepare(1000.0, 8);

    std::vector<float> buffer = {1.0f};
    graph.process(buffer);

    checkClose(buffer[0], 6.0f, "o mixer recebeu 2.0 + 4.0");
}

// A saída de um nó sobrevive intacta até o segundo consumidor lê-la.
//
// É o motivo de cada nó ter seu próprio buffer: se o módulo escrevesse sobre
// um buffer compartilhado, o segundo caminho leria o resultado do primeiro.
void testForkedOutputIsNotConsumedByFirstReader()
{
    std::cout << "saida bifurcada sobrevive ao primeiro leitor\n";

    AudioGraph graph;

    const auto fonte = graph.addNode(makeGain(2.0f));
    const auto ramoA = graph.addNode(makeGain(1.0f));
    const auto ramoB = graph.addNode(makeGain(1.0f));

    graph.connectFromInput(fonte);
    graph.connect(fonte, ramoA);
    graph.connect(fonte, ramoB);
    graph.connectToOutput(ramoA);
    graph.connectToOutput(ramoB);

    graph.prepare(1000.0, 8);

    std::vector<float> buffer = {1.0f};
    graph.process(buffer);

    // Os dois ramos receberam 2.0 cada, e a saída soma os dois.
    checkClose(buffer[0], 4.0f, "os dois ramos leram 2.0 e a saida deu 4.0");
}

// Sem prepare(), o processamento não altera nada.
void testProcessWithoutPrepareIsHarmless()
{
    std::cout << "processar sem preparar\n";

    AudioGraph graph;
    const auto a = graph.addNode(makeGain(5.0f));
    graph.connectFromInput(a);
    graph.connectToOutput(a);

    std::vector<float> buffer = {1.0f, 0.5f};
    graph.process(buffer);

    checkClose(buffer[0], 1.0f, "1.0 continua 1.0");
    checkClose(buffer[1], 0.5f, "0.5 continua 0.5");
}

// Grafo vazio deixa o buffer intacto.
void testEmptyGraphIsTransparent()
{
    std::cout << "grafo vazio nao altera o buffer\n";

    AudioGraph graph;
    graph.prepare(1000.0, 8);

    std::vector<float> buffer = {0.3f, -0.7f};
    graph.process(buffer);

    checkClose(buffer[0], 0.3f, "0.3 continua 0.3");
    checkClose(buffer[1], -0.7f, "-0.7 continua -0.7");
}

// Nó sem ligação com a saída não contribui.
void testNodeNotConnectedToOutputIsSilent()
{
    std::cout << "no desligado da saida nao contribui\n";

    AudioGraph graph;

    const auto usado = graph.addNode(makeGain(2.0f));
    const auto solto = graph.addNode(makeGain(100.0f));

    graph.connectFromInput(usado);
    graph.connectFromInput(solto);
    graph.connectToOutput(usado);

    graph.prepare(1000.0, 8);

    std::vector<float> buffer = {1.0f};
    graph.process(buffer);

    checkClose(buffer[0], 2.0f, "so o no ligado a saida apareceu");
}

// Buffer maior que o bloco preparado é fatiado, sem alocar nem errar.
void testBufferLargerThanBlockSizeIsSliced()
{
    std::cout << "buffer maior que o bloco e fatiado\n";

    AudioGraph graph;

    const auto dobra = graph.addNode(makeGain(2.0f));
    graph.connectFromInput(dobra);
    graph.connectToOutput(dobra);

    graph.prepare(1000.0, 4);

    // 10 não é múltiplo de 4: o último pedaço fica incompleto.
    std::vector<float> buffer(10, 1.0f);
    graph.process(buffer);

    bool todosDobrados = true;
    for (float sample : buffer)
    {
        if (std::fabs(sample - 2.0f) > 1e-6f)
            todosDobrados = false;
    }

    check(todosDobrados, "as 10 amostras foram processadas");
}

// Um módulo com estado funciona dentro do grafo, atravessando fatias.
void testStatefulModuleInsideGraph()
{
    std::cout << "modulo com estado dentro do grafo\n";

    AudioGraph graph;

    auto echo = std::make_unique<Delay>();
    echo->setTime(0.1f);      // 10 amostras a 100 Hz
    echo->setFeedback(0.0f);
    echo->setMix(1.0f);

    const auto delayNode = graph.addNode(std::move(echo));
    graph.connectFromInput(delayNode);
    graph.connectToOutput(delayNode);

    graph.prepare(100.0, 4);

    std::vector<float> buffer(20, 0.0f);
    buffer[0] = 1.0f;
    graph.process(buffer);

    checkClose(buffer[10], 1.0f, "o eco chega na amostra 10, atravessando 3 fatias");
}

// reset() limpa o estado dos módulos do grafo.
void testResetClearsModuleState()
{
    std::cout << "reset limpa o estado dos modulos\n";

    AudioGraph graph;

    auto echo = std::make_unique<Delay>();
    echo->setTime(0.1f);
    echo->setFeedback(0.5f);
    echo->setMix(1.0f);

    const auto delayNode = graph.addNode(std::move(echo));
    graph.connectFromInput(delayNode);
    graph.connectToOutput(delayNode);

    graph.prepare(100.0, 8);

    std::vector<float> primeiro(8, 0.0f);
    primeiro[0] = 1.0f;
    graph.process(primeiro);

    graph.reset();

    std::vector<float> depois(20, 0.0f);
    graph.process(depois);

    bool silencio = true;
    for (float sample : depois)
    {
        if (sample != 0.0f)
            silencio = false;
    }

    check(silencio, "nada do eco antigo voltou");
}

// Os parâmetros continuam acessíveis pelo grafo.
void testParametersReachableThroughGraph()
{
    std::cout << "parametros acessiveis pelo grafo\n";

    AudioGraph graph;
    const auto id = graph.addNode(makeGain(1.0f));

    graph.connectFromInput(id);
    graph.connectToOutput(id);
    graph.prepare(1000.0, 8);

    Parameter* parameter = graph.moduleAt(id).findParameter("gain");
    check(parameter != nullptr, "achou o parametro gain");

    if (parameter != nullptr)
    {
        parameter->setValue(3.0f);
    }

    // O ajuste vem DEPOIS do prepare(), então o ganho não salta para 3.0:
    // ele sobe pela rampa de 20 ms, que a 1000 Hz dura 20 amostras. Um buffer
    // de 30 amostras cobre a rampa inteira com folga.
    //
    // Este teste falhou na primeira escrita justamente por ignorar isso —
    // esperava 3.0 na primeira amostra e recebeu 1.1, que é o primeiro passo
    // da rampa. O erro estava no teste, não no código.
    std::vector<float> buffer(30, 1.0f);
    graph.process(buffer);

    check(buffer[0] > 1.0f && buffer[0] < 3.0f, "a primeira amostra esta no meio da rampa");
    checkClose(buffer[29], 3.0f, "ao fim da rampa o ganho chegou a 3.0");
    check(std::string(graph.moduleAt(id).name()) == "Gain", "name() acessivel pelo grafo");
}

int main()
{
    std::cout << "\n=== testes do AudioGraph ===\n\n";

    testStartsEmpty();
    testAddNodeReturnsDistinctIds();
    testIdsSurviveRemovalOfOtherNodes();
    testRemovingNodeClearsItsConnections();
    testProcessingOrderRespectsDependencies();
    testIndependentNodesAllAppearInOrder();
    testCycleIsRejected();
    testSelfConnectionIsRejected();
    testDuplicateConnectionIsRejected();
    testInvalidIdsThrow();
    testLinearChainThroughGraph();
    testParallelPathsAreSummed();
    testMultipleSourcesAreMixed();
    testForkedOutputIsNotConsumedByFirstReader();
    testProcessWithoutPrepareIsHarmless();
    testEmptyGraphIsTransparent();
    testNodeNotConnectedToOutputIsSilent();
    testBufferLargerThanBlockSizeIsSliced();
    testStatefulModuleInsideGraph();
    testResetClearsModuleState();
    testParametersReachableThroughGraph();

    return reportResults();
}
