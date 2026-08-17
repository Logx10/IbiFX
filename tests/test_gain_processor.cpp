// Testes do GainProcessor.
//
// Este arquivo é um programa comum, com main() e tudo. A única diferença
// é o que ele faz: em vez de processar áudio de verdade, ele executa o
// GainProcessor com entradas conhecidas e confere as saídas.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <iostream>
#include <vector>

#include "GainProcessor.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------

// Ganho 1.0 é o elemento neutro da multiplicação: nada pode mudar.
//
// Parece um teste bobo, e é justamente por isso que ele é bom: se um dia
// alguém "otimizar" o process() e quebrar esse caso, o erro seria sutil e
// audível como uma leve mudança de volume — difícil de rastrear no ouvido,
// trivial de pegar aqui.
void testNeutralGainDoesNotChangeBuffer()
{
    std::cout << "ganho neutro (1.0)\n";

    std::vector<float> buffer = {0.0f, 0.25f, -0.5f, 1.0f};

    GainProcessor processor;
    processor.setGain(1.0f);
    processor.process(buffer);

    checkClose(buffer[0], 0.0f, "0.0 continua 0.0");
    checkClose(buffer[1], 0.25f, "0.25 continua 0.25");
    checkClose(buffer[2], -0.5f, "-0.5 continua -0.5");
    checkClose(buffer[3], 1.0f, "1.0 continua 1.0");
}

// Ganho 0.5 divide tudo pela metade — inclusive os negativos.
void testHalfGain()
{
    std::cout << "ganho 0.5\n";

    std::vector<float> buffer = {0.25f, 0.75f, -1.0f};

    GainProcessor processor;
    processor.setGain(0.5f);
    processor.process(buffer);

    // Este é o valor que apareceu como 0.12 na tela. O teste compara o
    // número de verdade (0.125), não o texto impresso — e por isso não
    // se importa com arredondamento de exibição.
    checkClose(buffer[0], 0.125f, "0.25 vira 0.125");
    checkClose(buffer[1], 0.375f, "0.75 vira 0.375");
    checkClose(buffer[2], -0.5f, "-1.0 vira -0.5");
}

// ---------------------------------------------------------------------

// Ganho 0.0 deve zerar tudo — é o mute.
void testZeroGainSilences()
{
    std::cout << "ganho 0.0 (mute)\n";

    std::vector<float> buffer = {0.0f, 0.25f, -0.5f, 1.0f};

    GainProcessor processor;
    processor.setGain(0.0f);
    processor.process(buffer);

    checkClose(buffer[0], 0.0f, "0.0 vira 0.0");
    checkClose(buffer[1], 0.0f, "0.25 vira 0.0");
    checkClose(buffer[2], 0.0f, "-0.5 vira 0.0");
    checkClose(buffer[3], 0.0f, "1.0 vira 0.0");
}

// Ganho negativo INVERTE A POLARIDADE do sinal.
//
// Isto não é bug, é um recurso real: -1.0 espelha a onda no eixo. Sozinho
// é praticamente inaudível (o ouvido quase não percebe), mas somado ao
// sinal original produz silêncio — é assim que funciona cancelamento de
// fase, e é a causa clássica de "por que meu som sumiu ao misturar dois
// microfones".
void testNegativeGainInvertsPolarity()
{
    std::cout << "ganho negativo (inverte polaridade)\n";

    std::vector<float> buffer = {0.5f, -0.5f, 1.0f, -1.0f};
    GainProcessor processor;
    processor.setGain(-1.0f);
    processor.process(buffer);

    checkClose(buffer[0], -0.5f, "0.5 vira -0.5");
    checkClose(buffer[1], 0.5f, "-0.5 vira 0.5");
    checkClose(buffer[2], -1.0f, "1.0 vira -1.0");
    checkClose(buffer[3], 1.0f, "-1.0 vira 1.0");
}

// Um buffer vazio não pode quebrar o programa.
//
// Por que testar algo que "obviamente" funciona: porque com um for clássico
// (`i < buffer.size()`) e um erro de sinal, tamanho 0 é exatamente onde o
// código estoura. O teste garante que continue seguro se a implementação
// mudar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    std::vector<float> buffer;
    GainProcessor processor;
    processor.setGain(1.0f);
    processor.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

// Ganho acima de 1.0 pode empurrar amostras para fora da faixa [-1, +1].
//
// O GainProcessor NÃO deve impedir isso. Cortar em 1.0 é trabalho de um
// clipper ou limiter, que virá depois — cada módulo faz uma coisa só.
// Este teste registra essa decisão de projeto: se alguém adicionar um
// corte aqui no futuro, o teste falha e a conversa acontece.
void testGainAboveOneCanExceedRange()
{
    std::cout << "ganho acima de 1.0 (sem clipping)\n";

    std::vector<float> buffer = {0.75f, -0.5f};
    GainProcessor processor;
    processor.setGain(2.0f);
    processor.process(buffer);

    checkClose(buffer[0], 1.5f, "0.75 vira 1.5");
    checkClose(buffer[1], -1.0f, "-0.5 vira -1.0");
}

int main()
{
    std::cout << "\n=== testes do GainProcessor ===\n\n";

    testNeutralGainDoesNotChangeBuffer();
    testHalfGain();
    testZeroGainSilences();
    testNegativeGainInvertsPolarity();
    testEmptyBufferDoesNotCrash();
    testGainAboveOneCanExceedRange();

    return reportResults();
}
