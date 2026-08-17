// Testes do Clipper.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <iostream>
#include <vector>

#include "Clipper.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------

// Sinal que já está dentro da faixa não pode ser tocado.
//
// O teste do "não estrague o que estava bom". Um clipper que altera áudio
// normal está quebrado, mesmo que corte os picos corretamente.
void testSignalInsideRangeIsUntouched()
{
    std::cout << "sinal dentro da faixa\n";

    std::vector<float> buffer = {0.0f, 0.5f, -0.5f, 0.999f};

    Clipper clipper;
    clipper.setThreshold(1.0f);
    clipper.process(buffer);

    checkClose(buffer[0], 0.0f, "0.0 continua 0.0");
    checkClose(buffer[1], 0.5f, "0.5 continua 0.5");
    checkClose(buffer[2], -0.5f, "-0.5 continua -0.5");
    checkClose(buffer[3], 0.999f, "0.999 continua 0.999");
}

// Tudo acima do teto vira o teto — independente de quanto passou.
//
// Repare que 1.5, 2.0 e 10.0 terminam idênticos. Essa perda de informação é
// o efeito: os três picos, antes diferentes, viram o mesmo platô. É por isso
// que clipping distorce em vez de apenas baixar o volume.
void testClipsAboveThreshold()
{
    std::cout << "corta acima do teto\n";

    std::vector<float> buffer = {1.5f, 2.0f, 10.0f};

    Clipper clipper;
    clipper.setThreshold(1.0f);
    clipper.process(buffer);

    checkClose(buffer[0], 1.0f, "1.5 vira 1.0");
    checkClose(buffer[1], 1.0f, "2.0 vira 1.0");
    checkClose(buffer[2], 1.0f, "10.0 vira 1.0");
}

// O mesmo, do lado negativo.
//
// Um erro de sinal na implementação — um menos esquecido — passa despercebido
// no teste anterior e aparece aqui.
void testClipsBelowNegativeThreshold()
{
    std::cout << "corta abaixo do piso\n";

    std::vector<float> buffer = {-1.5f, -3.0f, -10.0f};

    Clipper clipper;
    clipper.setThreshold(1.0f);
    clipper.process(buffer);

    checkClose(buffer[0], -1.0f, "-1.5 vira -1.0");
    checkClose(buffer[1], -1.0f, "-3.0 vira -1.0");
    checkClose(buffer[2], -1.0f, "-10.0 vira -1.0");
}

// A amostra exatamente no limite.
//
// O teste mais valioso da bateria. É onde mora o erro clássico de trocar >
// por >=, e é o tipo de divergência que o ouvido não pega mas o teste sim.
// 1.0 é um valor perfeitamente válido: não deve ser cortado, deve passar.
void testValueExactlyAtThresholdIsKept()
{
    std::cout << "fronteira exata\n";

    std::vector<float> buffer = {1.0f, -1.0f};

    Clipper clipper;
    clipper.setThreshold(1.0f);
    clipper.process(buffer);

    checkClose(buffer[0], 1.0f, "1.0 no teto continua 1.0");
    checkClose(buffer[1], -1.0f, "-1.0 no piso continua -1.0");
}

// O threshold precisa ser realmente usado.
//
// Sem este teste, uma implementação que ignorasse m_threshold e cortasse
// sempre em 1.0 passaria em todos os anteriores.
void testCustomThreshold()
{
    std::cout << "threshold customizado (0.5)\n";

    std::vector<float> buffer = {0.8f, 0.3f, -0.8f, -0.3f};

    Clipper clipper;
    clipper.setThreshold(0.5f);
    clipper.process(buffer);

    checkClose(buffer[0], 0.5f, "0.8 vira 0.5");
    checkClose(buffer[1], 0.3f, "0.3 continua 0.3");
    checkClose(buffer[2], -0.5f, "-0.8 vira -0.5");
    checkClose(buffer[3], -0.3f, "-0.3 continua -0.3");
}

// O getter devolve o que o setter guardou.
void testThresholdRoundTrip()
{
    std::cout << "setter e getter\n";

    Clipper clipper;

    checkClose(clipper.threshold(), 1.0f, "threshold padrao e 1.0");

    clipper.setThreshold(0.25f);
    checkClose(clipper.threshold(), 0.25f, "threshold vira 0.25");
}

// Buffer vazio nao pode quebrar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    std::vector<float> buffer;

    Clipper clipper;
    clipper.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

int main()
{
    std::cout << "\n=== testes do Clipper ===\n\n";

    testSignalInsideRangeIsUntouched();
    testClipsAboveThreshold();
    testClipsBelowNegativeThreshold();
    testValueExactlyAtThresholdIsKept();
    testCustomThreshold();
    testThresholdRoundTrip();
    testEmptyBufferDoesNotCrash();

    return reportResults();
}
