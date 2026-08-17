#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "AudioModule.h"

// AudioGraph — roteamento em grafo, no lugar da fila única do ModuleChain.
//
// O QUE A CADEIA NÃO CONSEGUE FAZER
// O ModuleChain é uma fila: cada módulo recebe a saída do anterior. Isso
// cobre a maior parte de uma pedaleira, mas não cobre roteamento paralelo:
//
//                     ┌── Delay ───┐
//                     │            │
//     Amp → Cabinet ──┤            ├── Mixer → saída
//                     │            │
//                     └── Reverb ──┘
//
// Aqui um sinal se divide em dois caminhos e volta a se juntar. Numa fila
// isso é impossível: o reverb receberia a saída do delay em vez do sinal
// limpo. É o roteamento de qualquer efeito em paralelo, de loop de efeitos e
// de mixagem de sinal seco com processado.
//
// NÓS E CONEXÕES
// Cada módulo vira um nó com um identificador. Uma conexão liga a saída de um
// nó à entrada de outro. Um nó pode receber de vários — nesse caso as
// entradas são SOMADAS, que é o que "misturar sinais" significa em áudio.
//
// Dois terminais especiais representam a borda do grafo: a entrada, de onde
// o sinal chega, e a saída, para onde ele vai. Nós conectados à saída são
// somados no buffer final.
//
// ORDEM DE PROCESSAMENTO
// Numa fila a ordem é óbvia. Num grafo, não: um nó só pode ser processado
// depois de todos os que alimentam a entrada dele. Descobrir uma ordem que
// respeite isso chama-se ORDENAÇÃO TOPOLÓGICA.
//
// O algoritmo usado é o de Kahn: começa pelos nós que não dependem de
// ninguém, e a cada nó processado libera os que só esperavam por ele. Se ao
// fim sobrarem nós sem processar, é porque eles dependem uns dos outros em
// círculo.
//
// CICLOS SÃO PROIBIDOS
// Um ciclo — A alimenta B que alimenta A — não tem ordem válida: para
// processar A é preciso B, e para processar B é preciso A. Em áudio isso é
// realimentação sem atraso, que estoura instantaneamente.
//
// A checagem acontece no connect(), que rejeita a conexão ANTES de criá-la.
// Deixar o grafo entrar em estado inválido e detectar depois seria pior:
// o erro apareceria longe da causa.
//
// Note que isso não proíbe realimentação — o Delay realimenta o tempo todo.
// A diferença é que ele o faz DENTRO de si, com atraso de várias amostras.
// O que não pode existir é um laço instantâneo entre nós.
//
// MEMÓRIA E TEMPO REAL
// Cada nó tem seu próprio buffer, alocado no prepare() com o tamanho de bloco
// informado. O process() só lê, escreve e soma — nenhuma alocação. Buffers
// separados são necessários porque um nó que alimenta dois caminhos precisa
// que sua saída sobreviva intacta até o segundo consumidor ler.
//
// Se o buffer recebido for maior que o bloco preparado, o process() o fatia
// em pedaços. É mais barato que realocar, e mantém a promessa de tempo real.
//
// LIMITAÇÃO CONHECIDA: assim como o ModuleChain, esta classe não é segura
// para uso concorrente. Alterar o grafo enquanto ele processa é corrida de
// dados.
class AudioGraph
{
public:
    using NodeId = std::size_t;

    // Acrescenta um módulo como nó e devolve o identificador dele.
    //
    // O id é estável: remover outros nós não o invalida. Isso importa porque
    // presets e mapeamentos MIDI vão guardá-lo.
    NodeId addNode(std::unique_ptr<AudioModule> module);

    // Remove o nó e todas as conexões que o envolvem.
    void removeNode(NodeId id);

    // Liga a saída de um nó à entrada de outro.
    //
    // Lança se algum id não existir, se a conexão já existir ou se ela
    // fecharia um ciclo.
    void connect(NodeId from, NodeId to);

    // Desfaz uma conexão. Lança se ela não existir.
    void disconnect(NodeId from, NodeId to);

    // Liga a entrada do grafo a um nó.
    void connectFromInput(NodeId to);

    // Liga um nó à saída do grafo.
    void connectToOutput(NodeId from);

    // Informa se uma conexão fecharia um ciclo, sem tentar criá-la.
    bool wouldCreateCycle(NodeId from, NodeId to) const;

    // Quantidade de nós.
    std::size_t nodeCount() const;

    // Verdadeiro se o grafo não tem nó nenhum.
    bool empty() const;

    // Verdadeiro se o nó existe.
    bool hasNode(NodeId id) const;

    // Acesso ao módulo de um nó, para consultar name() e ajustar parâmetros.
    AudioModule& moduleAt(NodeId id);
    const AudioModule& moduleAt(NodeId id) const;

    // A ordem em que os nós serão processados, do primeiro ao último.
    //
    // Exposta para depuração e para o demo: entender por que um efeito soa
    // diferente costuma começar por conferir a ordem.
    std::vector<NodeId> processingOrder() const;

    // Aloca os buffers de cada nó e prepara os módulos.
    void prepare(double sampleRate, int blockSize);

    // Descarta o estado acumulado de todos os módulos.
    void reset();

    // Passa o buffer pelo grafo, da entrada à saída.
    void process(std::vector<float>& buffer);

private:
    struct Node
    {
        NodeId id = 0;
        std::unique_ptr<AudioModule> module;
        std::vector<NodeId> sources;   // de quem este nó recebe
        bool fedByInput = false;       // recebe da entrada do grafo
        bool feedsOutput = false;      // alimenta a saída do grafo
        std::vector<float> buffer;     // saída própria, alocada no prepare
    };

    Node* find(NodeId id);
    const Node* find(NodeId id) const;

    // Processa uma fatia de até blockSize amostras.
    void processSlice(float* data, std::size_t count);

    std::vector<Node> m_nodes;
    NodeId m_nextId = 1;
    std::size_t m_blockSize = 0;

    // Buffer de trabalho para entregar ao módulo uma fatia do tamanho exato.
    // Reservado no prepare(); no process() só muda de tamanho lógico, nunca
    // de capacidade, então não aloca.
    std::vector<float> m_scratch;

    // Recalculada a cada mudança estrutural, e não a cada bloco: ordenar é
    // trabalho do domínio de controle, não da thread de áudio.
    std::vector<NodeId> m_order;
    bool m_orderValid = false;

    void invalidateOrder();
    void rebuildOrder();
};
