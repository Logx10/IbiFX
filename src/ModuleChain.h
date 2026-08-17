#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "AudioModule.h"

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
// LIMITAÇÃO CONHECIDA: esta classe NÃO é segura para uso concorrente.
// Chamar add() enquanto a thread de áudio executa process() é corrida de
// dados, e um realloc do vetor no meio do laço seria falha grave. Hoje tudo
// roda numa thread só e o problema não existe. Quando o áudio em tempo real
// entrar (Fase 7), a comunicação entre os dois domínios precisará de
// mecanismo próprio — fila de comandos ou troca atômica de ponteiro — e essa
// será uma decisão de projeto explícita, não um detalhe resolvido no susto.
class ModuleChain
{
public:
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

    // Passa o buffer por todos os módulos ativos, na ordem da cadeia.
    void process(std::vector<float>& buffer);

private:
    // Um módulo mais o estado que a cadeia mantém sobre ele.
    struct Slot
    {
        std::unique_ptr<AudioModule> module;
        bool bypassed = false;
    };

    std::vector<Slot> m_slots;
};
