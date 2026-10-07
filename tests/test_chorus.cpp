// Testes do Chorus.
//
// O chorus é um atraso que oscila, então os testes medem ATRASO: um impulso
// entra, e a posição em que ele sai diz qual era o atraso naquele instante.
// Com depth 0, o atraso é fixo; com depth > 0, impulsos em instantes
// diferentes saem com atrasos diferentes — é a prova de que o LFO está
// modulando de verdade.
//
// A infra de verificação vive em test_helpers.h.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

#include "Chorus.h"
#include "test_helpers.h"

namespace
{
constexpr double kSampleRate = 48000.0;

void prepareChorus(Chorus& chorus, float rate, float depth, float mix)
{
    chorus.setRate(rate);
    chorus.setDepth(depth);
    chorus.setMix(mix);
    chorus.prepare(kSampleRate, 128);
    chorus.reset();
}

// Índice da amostra de maior módulo em [from, to).
std::size_t peakIndex(const std::vector<float>& buffer, std::size_t from, std::size_t to)
{
    std::size_t best = from;
    for (std::size_t i = from; i < to; ++i)
    {
        if (std::fabs(buffer[i]) > std::fabs(buffer[best]))
            best = i;
    }
    return best;
}
}

// ---------------------------------------------------------------------

void testZeroMixIsDry()
{
    std::cout << "mix 0 devolve o sinal intocado\n";

    Chorus chorus;
    prepareChorus(chorus, 1.0f, 1.0f, 0.0f);

    std::vector<float> buffer = {0.5f, -0.3f, 0.8f, 0.0f, -1.0f};
    const std::vector<float> original = buffer;
    chorus.process(buffer);

    bool same = true;
    for (std::size_t i = 0; i < buffer.size(); ++i)
        same = same && buffer[i] == original[i];
    check(same, "saida = entrada, amostra a amostra");
}

// Sem modulação, o chorus é um atraso fixo de 5 ms: a 48 kHz, um impulso
// sai 240 amostras depois.
void testZeroDepthIsAFixedFiveMillisecondDelay()
{
    std::cout << "depth 0: atraso fixo de 5 ms\n";

    Chorus chorus;
    prepareChorus(chorus, 1.0f, 0.0f, 1.0f);

    std::vector<float> buffer(1000, 0.0f);
    buffer[0] = 1.0f;
    chorus.process(buffer);

    check(peakIndex(buffer, 0, buffer.size()) == 240, "o impulso sai na amostra 240");
    checkClose(buffer[240], 1.0f, "inteiro, sem perder nada (atraso inteiro, sem fracao)");
}

// O que faz um chorus ser chorus: o atraso muda com o tempo. Impulsos a
// cada 50 ms, ao longo de um ciclo inteiro do LFO a 2 Hz, saem com atrasos
// que cobrem boa parte da faixa de 5 a 12 ms.
void testDepthModulatesTheDelay()
{
    std::cout << "depth 1: o atraso varia entre 5 e 12 ms\n";

    Chorus chorus;
    prepareChorus(chorus, 2.0f, 1.0f, 1.0f);

    const std::size_t spacing = 2400;  // 50 ms
    std::vector<float> buffer(spacing * 12, 0.0f);
    for (std::size_t i = 0; i < buffer.size(); i += spacing)
        buffer[i] = 1.0f;

    chorus.process(buffer);

    double shortest = 1e9;
    double longest = 0.0;
    for (std::size_t start = 0; start + spacing <= buffer.size(); start += spacing)
    {
        const double delayMs = static_cast<double>(peakIndex(buffer, start, start + spacing) - start) / kSampleRate * 1000.0;
        shortest = std::min(shortest, delayMs);
        longest = std::max(longest, delayMs);
    }

    std::cout << "    atraso medido: de " << shortest << " a " << longest << " ms\n";

    check(shortest >= 4.9, "nunca menos que 5 ms");
    check(longest <= 12.1, "nunca mais que 12 ms");
    check(longest - shortest > 5.0, "varre mais de 5 ms da faixa");
}

// Um seno de amplitude 1 com mix 0,5 não estoura: original e cópia somam,
// mas cada uma entra com metade do peso.
void testOutputStaysBounded()
{
    std::cout << "saida fica perto de [-1, +1]\n";

    Chorus chorus;
    prepareChorus(chorus, 3.0f, 1.0f, 0.5f);

    std::vector<float> buffer(48000);
    for (std::size_t i = 0; i < buffer.size(); ++i)
        buffer[i] = static_cast<float>(std::sin(6.283185307179586 * 440.0 * static_cast<double>(i) / kSampleRate));

    chorus.process(buffer);

    float peak = 0.0f;
    for (float sample : buffer)
        peak = std::max(peak, std::fabs(sample));

    check(peak <= 1.05f, "pico ate 1.05 (folga da interpolacao)");
}

void testParametersRoundTrip()
{
    std::cout << "setters e getters\n";

    Chorus chorus;
    checkClose(chorus.rate(), 0.8f, "rate padrao 0.8 Hz");
    checkClose(chorus.depth(), 0.5f, "depth padrao 0.5");
    checkClose(chorus.mix(), 0.5f, "mix padrao 0.5");

    chorus.setRate(2.5f);
    chorus.setDepth(0.2f);
    chorus.setMix(0.9f);
    checkClose(chorus.rate(), 2.5f, "rate vira 2.5");
    checkClose(chorus.depth(), 0.2f, "depth vira 0.2");
    checkClose(chorus.mix(), 0.9f, "mix vira 0.9");
}

void testWithoutPrepareIsPassthrough()
{
    std::cout << "sem prepare(), passa o sinal intocado\n";

    Chorus chorus;
    std::vector<float> buffer = {0.25f, -0.5f};
    chorus.process(buffer);

    checkClose(buffer[0], 0.25f, "0.25 continua 0.25");
    checkClose(buffer[1], -0.5f, "-0.5 continua -0.5");
    check(chorus.bufferSize() == 0, "nenhum buffer alocado");
}

int main()
{
    std::cout << "\n=== testes do Chorus ===\n\n";

    testZeroMixIsDry();
    testZeroDepthIsAFixedFiveMillisecondDelay();
    testDepthModulatesTheDelay();
    testOutputStaysBounded();
    testParametersRoundTrip();
    testWithoutPrepareIsPassthrough();

    return reportResults();
}
