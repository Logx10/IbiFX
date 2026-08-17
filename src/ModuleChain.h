#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "AudioModule.h"
#include "CommandQueue.h"

// ModuleChain — uma cadeia linear de módulos, montada em tempo de execução.
//
// O QUE MUDA EM RELAÇÃO AO QUE HAVIA ANTES
// O demo já processava cadeias, mas elas eram escritas no código e fixas na
// compilação. Aqui a cadeia vira dado: pode-se adicionar, remover, reordenar
// e desligar módulos enquanto o programa roda. É o que transforma três
// efeitos soltos numa pedaleira.
//
// POSSE DOS MÓDULOS
// A cadeia é DONA dos módulos que recebe. Guardá-los como std::unique_ptr
// deixa isso explícito no tipo: um unique_ptr não pode ser copiado, só
// movido, então é impossível dois lugares acharem que possuem o mesmo
// módulo. Quando a cadeia morre, os módulos morrem junto — sem delete
// escrito à mão, sem chance de esquecer.
//
// É por isso que add() recebe por valor e exige std::move() do chamador: o
// std::move não move nada sozinho, ele apenas marca "pode roubar o conteúdo
// deste objeto". A transferência de posse fica visível na linha da chamada,
// em vez de escondida na assinatura.
//
// BYPASS FICA NA CADEIA, NÃO NO MÓDULO
// Cada posição guarda um sinalizador de bypass ao lado do módulo. Bypass é
// uma decisão de roteamento — "pule este ponto da cadeia" —, não um cálculo
// de DSP, então não pertence ao AudioModule. Isso também mantém o contrato
// mínimo e evita obrigar todo módulo futuro a reimplementar a mesma lógica.
//
// DOIS DOMÍNIOS, DUAS REGRAS
// process() é a única função pensada para a thread de áudio: percorre um
// vetor e chama process() de cada módulo, sem alocar nada.
// add(), remove(), move() e clear() pertencem ao domínio de controle —
// alocam, realocam e lançam exceção em índice inválido.
//
// COMO FALAR COM A CADEIA DE OUTRA THREAD
// add(), remove() e move() continuam sendo chamadas do domínio de controle e
// NÃO podem ser usadas enquanto a thread de áudio processa: elas realocam o
// vetor, e percorrer memória liberada é falha grave, não valor errado.
//
// Para mudanças durante o som existe a fila de comandos. O controle deposita
// com pushCommand(); o process() retira e aplica tudo no começo do bloco,
// antes de tocar em qualquer amostra. Ninguém espera por ninguém, e a
// estrutura só muda em um ponto conhecido do ciclo.
//
// Isso cobre ajustar parâmetro, ligar bypass e resetar — o que basta para
// tocar. Trocar a montagem da cadeia com o áudio rodando exigirá construir a
// cadeia nova fora e trocá-la por ponteiro atômico, e isso fica para quando
// houver um dispositivo de áudio de verdade do outro lado.
class ModuleChain
{
public:
    ModuleChain() = default;

    // NEM COPIÁVEL NEM MOVÍVEL, DE PROPÓSITO
    // A fila de comandos contém índices atômicos que a thread de áudio pode
    // estar lendo neste instante. Mover a cadeia mudaria o endereço deles no
    // meio da leitura — falha silenciosa e difícil de rastrear.
    //
    // O compilador já removeria a cópia por causa do unique_ptr; declarar as
    // quatro operações torna a intenção explícita e faz o erro aparecer com
    // uma mensagem clara em vez de "construtor implicitamente removido".
    //
    // Consequência prática: uma função não pode DEVOLVER uma cadeia por
    // valor. Ela recebe uma por referência e a preenche.
    ModuleChain(const ModuleChain&) = delete;
    ModuleChain& operator=(const ModuleChain&) = delete;
    ModuleChain(ModuleChain&&) = delete;
    ModuleChain& operator=(ModuleChain&&) = delete;

    // Acrescenta um módulo ao fim da cadeia e assume a posse dele.
    void add(std::unique_ptr<AudioModule> module);

    // Remove o módulo da posição indicada, destruindo-o.
    void remove(std::size_t index);

    // Move um módulo de uma posição para outra, deslocando os demais.
    void move(std::size_t from, std::size_t to);

    // Esvazia a cadeia, destruindo todos os módulos.
    void clear();

    // Quantidade de módulos na cadeia.
    std::size_t size() const;

    // Verdadeiro quando a cadeia não tem módulo nenhum.
    bool empty() const;

    // Acesso ao módulo de uma posição, para consultar name() e ajustar
    // parâmetros. A versão const serve a quem só precisa ler.
    AudioModule& moduleAt(std::size_t index);
    const AudioModule& moduleAt(std::size_t index) const;

    // Liga ou desliga o bypass de uma posição.
    void setBypassed(std::size_t index, bool bypassed);

    // Informa se a posição está em bypass.
    bool isBypassed(std::size_t index) const;

    // Repassa o sample rate e o tamanho de bloco a todos os módulos, dando a
    // cada um a chance de alocar o que precisar. Deve ser chamada antes do
    // primeiro process(), e de novo sempre que o dispositivo de áudio mudar.
    void prepare(double sampleRate, int blockSize);

    // Descarta o estado acumulado de todos os módulos, inclusive os que estão
    // em bypass — um eco parado não deve ressurgir ao religar o módulo.
    void reset();

    // Deposita um comando para ser aplicado no próximo bloco.
    //
    // Chamada do domínio de controle. Devolve false se a fila estiver cheia,
    // sem bloquear — bloquear aqui poderia travar quem chama, e a decisão do
    // que fazer (tentar de novo, descartar, avisar) é de quem chama.
    bool pushCommand(const Command& command);

    // Comandos pendentes, ainda não aplicados.
    std::size_t pendingCommandCount() const;

    // Aplica os comandos pendentes e passa o buffer pelos módulos ativos.
    void process(std::vector<float>& buffer);

private:
    // Um módulo mais o estado que a cadeia mantém sobre ele.
    struct Slot
    {
        std::unique_ptr<AudioModule> module;
        bool bypassed = false;
    };

    // Aplica um comando à cadeia. Chamada só de dentro do process().
    void applyCommand(const Command& command);

    std::vector<Slot> m_slots;
    CommandQueue m_commands;
};
