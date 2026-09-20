// Testes do ConvolutionEngine.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <iostream>
#include <string>
#include <vector>

#include "ConvolutionEngine.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------

// Sem impulse response carregada, o motor é passthrough puro.
void testWithoutImpulseResponseIsPassthrough()
{
    std::cout << "sem IR carregada, e passthrough\n";

    ConvolutionEngine engine;

    checkClose(engine.processSample(0.5f), 0.5f, "0.5 sai 0.5 sem IR");
    checkClose(engine.processSample(-0.3f), -0.3f, "-0.3 sai -0.3 sem IR");
}

// IR = {1.0} é a identidade: convolução com um único tap de ganho 1 não
// muda nada.
void testIdentityImpulseResponse()
{
    std::cout << "IR {1.0} e a identidade\n";

    ConvolutionEngine engine;
    engine.setImpulseResponse({1.0f});

    checkClose(engine.processSample(0.5f), 0.5f, "0.5 sai 0.5");
    checkClose(engine.processSample(-0.7f), -0.7f, "-0.7 sai -0.7");
}

// IR = {0.0, 1.0} atrasa o sinal em exatamente 1 amostra — o primeiro tap
// (ganho 0) zera a amostra atual, o segundo (ganho 1) repete a anterior.
void testSingleSampleDelayImpulseResponse()
{
    std::cout << "IR {0.0, 1.0} atrasa 1 amostra\n";

    ConvolutionEngine engine;
    engine.setImpulseResponse({0.0f, 1.0f});

    checkClose(engine.processSample(0.3f), 0.0f, "primeira amostra: sem historico ainda, sai 0.0");
    checkClose(engine.processSample(0.6f), 0.3f, "segunda amostra: repete a primeira (0.3)");
    checkClose(engine.processSample(0.9f), 0.6f, "terceira amostra: repete a segunda (0.6)");
}

// Um impulso (1, 0, 0, 0...) convolvido com qualquer IR devolve a própria
// IR na saída — é a definição de impulse response: a resposta do sistema a
// um impulso É a IR.
void testImpulseInputReturnsTheImpulseResponseItself()
{
    std::cout << "um impulso na entrada devolve a propria IR na saida\n";

    ConvolutionEngine engine;
    engine.setImpulseResponse({1.0f, 0.5f, 0.25f, 0.125f});

    checkClose(engine.processSample(1.0f), 1.0f, "amostra 0: 1.0 (primeiro tap)");
    checkClose(engine.processSample(0.0f), 0.5f, "amostra 1: 0.5 (segundo tap)");
    checkClose(engine.processSample(0.0f), 0.25f, "amostra 2: 0.25 (terceiro tap)");
    checkClose(engine.processSample(0.0f), 0.125f, "amostra 3: 0.125 (quarto tap)");
    checkClose(engine.processSample(0.0f), 0.0f, "amostra 4: a IR acabou, volta a 0");
}

// reset() apaga o histórico de entrada, sem descartar a IR carregada.
void testResetClearsHistoryNotImpulseResponse()
{
    std::cout << "reset apaga o historico, mantem a IR\n";

    ConvolutionEngine engine;
    engine.setImpulseResponse({0.0f, 1.0f});  // atraso de 1 amostra

    engine.processSample(0.7f);
    engine.reset();

    // Depois do reset, o historico esta zerado de novo: a proxima amostra
    // "atrasada" que sai e 0.0, nao o 0.7 de antes do reset.
    checkClose(engine.processSample(0.9f), 0.0f, "historico foi zerado pelo reset");
    checkClose(engine.processSample(0.1f), 0.9f, "mas a IR continua a mesma (atraso de 1)");
}

// Uma convolução calculada à mão, com uma IR de 3 taps e uma sequência de
// entrada conhecida — a prova de que a soma ponderada está certa, não só
// os casos especiais (identidade, impulso).
void testKnownConvolutionSequence()
{
    std::cout << "sequencia de convolucao calculada a mao\n";

    ConvolutionEngine engine;
    engine.setImpulseResponse({0.5f, 0.25f, 0.125f});

    const std::vector<float> entrada = {1.0f, 2.0f, 3.0f, 0.0f, 0.0f};

    // y[n] = 0.5*x[n] + 0.25*x[n-1] + 0.125*x[n-2]
    const std::vector<float> esperado = {
        0.5f * 1.0f,                                    // y[0]
        0.5f * 2.0f + 0.25f * 1.0f,                      // y[1]
        0.5f * 3.0f + 0.25f * 2.0f + 0.125f * 1.0f,      // y[2]
        0.5f * 0.0f + 0.25f * 3.0f + 0.125f * 2.0f,      // y[3]
        0.5f * 0.0f + 0.25f * 0.0f + 0.125f * 3.0f,      // y[4]
    };

    for (std::size_t i = 0; i < entrada.size(); ++i)
    {
        checkClose(engine.processSample(entrada[i]), esperado[i],
                   "amostra " + std::to_string(i) + " bate com a soma ponderada calculada a mao");
    }
}

int main()
{
    std::cout << "\n=== testes do ConvolutionEngine ===\n\n";

    testWithoutImpulseResponseIsPassthrough();
    testIdentityImpulseResponse();
    testSingleSampleDelayImpulseResponse();
    testImpulseInputReturnsTheImpulseResponseItself();
    testResetClearsHistoryNotImpulseResponse();
    testKnownConvolutionSequence();

    return reportResults();
}
