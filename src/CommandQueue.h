#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

// CommandQueue — passagem de comandos do domínio de controle para a thread
// de áudio, sem bloqueio.
//
// O PROBLEMA
// Um parâmetro isolado já está resolvido: Parameter guarda um atomic<float>,
// e escrever nele de outra thread é seguro. Mas nem toda mudança cabe num
// número.
//
// Adicionar um módulo, remover, reordenar ou ligar um bypass altera a
// ESTRUTURA. Se a thread de áudio estiver no meio do laço quando o vetor de
// módulos realocar, ela passa a percorrer memória liberada — e isso não é um
// valor errado por um bloco, é falha grave.
//
// POR QUE NÃO UM MUTEX
// A saída óbvia seria um cadeado: a thread de áudio trava antes de processar
// e destrava depois. Só que travar pode BLOQUEAR — se a thread de controle
// estiver segurando o cadeado, a de áudio espera. E esperar é a única coisa
// que a thread de áudio não pode fazer: ela tem 2,67 ms para entregar o
// bloco, e quem chega atrasado produz estalo.
//
// Pior: o escalonador do sistema pode suspender a thread de controle
// exatamente enquanto ela segura o cadeado. A thread de áudio então espera
// não por microssegundos, mas por uma fatia inteira de escalonamento. É o
// problema clássico da inversão de prioridade.
//
// A SOLUÇÃO: UMA FILA DE MÃO ÚNICA
// A thread de controle DEPOSITA comandos; a thread de áudio RETIRA e aplica,
// no começo do bloco. Ninguém espera por ninguém.
//
// A fila é um vetor circular de tamanho fixo, alocado uma vez. Dois índices
// atômicos marcam onde escrever e onde ler:
//
//     [ . . C C C . . . ]
//           ↑     ↑
//         leitura escrita
//
// UM PRODUTOR, UM CONSUMIDOR
// Esta implementação assume exatamente uma thread depositando e exatamente
// uma retirando. Essa restrição é o que permite o código ser tão simples:
// cada índice tem um único escritor, então não há disputa sobre nenhum deles.
//
// Filas com vários produtores existem e são bem mais complicadas. Não
// precisamos de uma: há uma thread de áudio, e as mudanças de controle podem
// ser serializadas antes de entrar aqui.
//
// A ORDEM DE MEMÓRIA, EM UMA FRASE
// O produtor grava o comando e SÓ ENTÃO publica o novo índice de escrita, com
// `release`. O consumidor lê o índice com `acquire` e, ao vê-lo, tem a
// garantia de que o comando já está gravado. Sem esse par, o processador ou o
// compilador poderiam reordenar as duas operações, e o consumidor leria um
// comando pela metade.
//
// FILA CHEIA NÃO BLOQUEIA
// push() devolve false quando não há espaço, em vez de esperar. Cabe a quem
// chama decidir — tentar de novo, descartar, ou avisar. Bloquear aqui
// devolveria justamente o problema que a fila existe para evitar.
struct Command
{
    enum class Type
    {
        SetParameter,             // ajusta um parâmetro de um módulo, em sua faixa própria
        SetParameterNormalized,   // ajusta um parâmetro por uma posição de 0 a 1
        SetBypass,                // liga ou desliga o bypass de uma posição
        Reset                     // descarta o estado acumulado
    };

    Type type = Type::Reset;

    // Posição do módulo na cadeia.
    std::size_t moduleIndex = 0;

    // Posição do parâmetro dentro do módulo, para SetParameter e
    // SetParameterNormalized.
    std::size_t parameterIndex = 0;

    // SetParameter: valor na faixa própria do parâmetro.
    // SetParameterNormalized: posição de 0 a 1 dentro dessa faixa — a
    // "moeda comum" que Parameter.h já previa para controladores que não
    // conhecem a faixa real (um CC de MIDI entrega 0 a 127, nunca "-8 a
    // 8" do ganho; normalizar aqui é quem traduz).
    // SetBypass: 0 ou 1.
    float value = 0.0f;
};

class CommandQueue
{
public:
    // Aloca espaço para `capacity` comandos. A alocação acontece aqui, uma
    // vez — nunca durante push() ou pop().
    explicit CommandQueue(std::size_t capacity = 256);

    // Deposita um comando. Chamada pela thread de controle.
    // Devolve false se a fila estiver cheia, sem bloquear.
    bool push(const Command& command);

    // Retira o próximo comando. Chamada pela thread de áudio.
    // Devolve false se não houver nada, sem bloquear.
    bool pop(Command& outCommand);

    // Verdadeiro se não há comando pendente.
    bool empty() const;

    // Quantos comandos cabem ao mesmo tempo.
    std::size_t capacity() const;

    // Quantos comandos estão pendentes agora.
    std::size_t size() const;

private:
    // Uma posição fica sempre vazia para distinguir cheia de vazia: com os
    // dois índices iguais a fila está vazia, e nunca cheia.
    std::vector<Command> m_slots;

    std::atomic<std::size_t> m_writeIndex{0};
    std::atomic<std::size_t> m_readIndex{0};
};
