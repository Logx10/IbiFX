// Testes do ConvolutionEngine.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "ConvolutionEngine.h"
#include "test_helpers.h"

namespace
{
// Ruído pseudoaleatório determinístico (gerador congruente linear), entre
// -1 e 1 — o mesmo sinal toda vez que o teste roda.
std::vector<float> noise(std::size_t count, unsigned seed)
{
    std::vector<float> samples(count);
    for (float& sample : samples)
    {
        seed = seed * 1664525u + 1013904223u;
        sample = static_cast<float>(seed >> 8) / static_cast<float>(1u << 23) - 1.0f;
    }
    return samples;
}

// A conta da definição, y[n] = Σ h[k] · x[n-k], sem nenhum truque — a
// referência contra a qual a versão por FFT é comparada.
std::vector<float> directConvolution(const std::vector<float>& ir, const std::vector<float>& input)
{
    std::vector<float> output(input.size(), 0.0f);
    for (std::size_t n = 0; n < input.size(); ++n)
    {
        double sum = 0.0;
        for (std::size_t k = 0; k < ir.size() && k <= n; ++k)
            sum += static_cast<double>(ir[k]) * static_cast<double>(input[n - k]);
        output[n] = static_cast<float>(sum);
    }
    return output;
}

// Maior diferença entre o engine e a conta direta, para uma IR de irSize
// taps — o erro relativo ao maior valor da saída de referência.
float worstErrorAgainstDirect(std::size_t irSize, std::size_t inputSize)
{
    std::vector<float> ir = noise(irSize, 7u);
    // Decaimento, como uma IR de verdade — senão a saída cresce com o
    // tamanho e o erro relativo fica artificialmente pequeno.
    for (std::size_t i = 0; i < ir.size(); ++i)
        ir[i] *= std::exp(-static_cast<float>(i) / static_cast<float>(irSize / 4 + 1));

    const std::vector<float> input = noise(inputSize, 99u);
    const std::vector<float> expected = directConvolution(ir, input);

    ConvolutionEngine engine;
    engine.setImpulseResponse(ir);

    float worst = 0.0f;
    float peak = 0.0f;
    for (std::size_t n = 0; n < input.size(); ++n)
    {
        worst = std::max(worst, std::fabs(engine.processSample(input[n]) - expected[n]));
        peak = std::max(peak, std::fabs(expected[n]));
    }
    return worst / peak;
}
}

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

// A cauda via FFT dá o mesmo resultado da conta direta, amostra por
// amostra e sem atraso, em IRs de vários tamanhos — inclusive nas
// fronteiras das partes (128 = só cabeça, 129 = uma parte de 1 tap só,
// 256, e um tamanho que não divide por 128).
void testFftTailMatchesDirectConvolution()
{
    std::cout << "cauda via FFT bate com a conta direta, sem latencia\n";

    for (std::size_t irSize : {128u, 129u, 256u, 1000u, 4801u})
    {
        const float error = worstErrorAgainstDirect(irSize, 3000);
        check(error < 1e-5f, "IR de " + std::to_string(irSize) + " taps: erro relativo < 1e-5");
    }
}

// Um impulso devolve a própria IR, também na parte que passa pela FFT.
void testImpulseReturnsLongImpulseResponse()
{
    std::cout << "impulso devolve a IR longa inteira\n";

    std::vector<float> ir = noise(700, 3u);

    ConvolutionEngine engine;
    engine.setImpulseResponse(ir);

    float worst = 0.0f;
    for (std::size_t n = 0; n < 900; ++n)
    {
        const float expected = n < ir.size() ? ir[n] : 0.0f;
        worst = std::max(worst, std::fabs(engine.processSample(n == 0 ? 1.0f : 0.0f) - expected));
    }
    check(worst < 1e-5f, "saida = taps da IR, e zero depois dela");
}

// reset() também zera a cauda: depois dele, nada do sinal antigo vaza —
// nem do bloco pela metade, nem dos espectros guardados na FDL.
void testResetClearsTheTail()
{
    std::cout << "reset zera tambem a cauda\n";

    ConvolutionEngine engine;
    engine.setImpulseResponse(noise(1000, 5u));

    for (float sample : noise(450, 11u))
        engine.processSample(sample);

    engine.reset();

    float worst = 0.0f;
    for (int n = 0; n < 1200; ++n)
        worst = std::max(worst, std::fabs(engine.processSample(0.0f)));

    check(worst == 0.0f, "so silencio depois do reset");
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
    testFftTailMatchesDirectConvolution();
    testImpulseReturnsLongImpulseResponse();
    testResetClearsTheTail();

    return reportResults();
}
