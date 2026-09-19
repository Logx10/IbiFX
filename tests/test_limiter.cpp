// Testes do Limiter.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <cmath>
#include <iostream>
#include <vector>

#include "Limiter.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------

// Abaixo do threshold, a amostra sai idêntica — um limitador não deve
// colorir o que já está dentro da faixa segura.
void testSignalBelowThresholdIsUntouched()
{
    std::cout << "sinal abaixo do threshold passa intocado\n";

    std::vector<float> buffer = {0.3f, -0.3f, 0.5f};

    Limiter limiter;
    limiter.setThreshold(0.8f);
    limiter.process(buffer);

    checkClose(buffer[0], 0.3f, "0.3 continua 0.3");
    checkClose(buffer[1], -0.3f, "-0.3 continua -0.3");
    checkClose(buffer[2], 0.5f, "0.5 continua 0.5");
}

// Exatamente no threshold ainda é "dentro" — o <= no código decide isso.
void testSignalExactlyAtThresholdIsUntouched()
{
    std::cout << "sinal exatamente no threshold passa intocado\n";

    std::vector<float> buffer = {0.8f, -0.8f};

    Limiter limiter;
    limiter.setThreshold(0.8f);
    limiter.process(buffer);

    checkClose(buffer[0], 0.8f, "0.8 continua 0.8 no threshold 0.8");
    checkClose(buffer[1], -0.8f, "-0.8 continua -0.8 no threshold 0.8");
}

// O valor exato da curva acima do threshold, calculado à mão.
//
// threshold 0.5, entrada 0.8: excesso = 0.3, headroom = 0.5.
// saida = 0.5 + 0.5 * tanh(0.3 / 0.5) = 0.5 + 0.5 * tanh(0.6).
void testCurveValueAboveThreshold()
{
    std::cout << "valor exato da curva acima do threshold\n";

    std::vector<float> buffer = {0.8f};

    Limiter limiter;
    limiter.setThreshold(0.5f);
    limiter.process(buffer);

    checkClose(buffer[0], 0.768524783f, "0.8 com threshold 0.5 vira 0.76852");
}

// Por mais absurda que seja a entrada, a saída nunca passa de 1.0 — é
// propriedade da tanh, não de um `if` que corta.
void testOutputNeverExceedsOne()
{
    std::cout << "saida nunca ultrapassa 1.0\n";

    std::vector<float> buffer = {2.0f, -2.0f, 50.0f, -50.0f, 1000.0f};

    Limiter limiter;
    limiter.setThreshold(0.9f);
    limiter.process(buffer);

    for (float sample : buffer)
    {
        check(sample <= 1.0f && sample >= -1.0f, "amostra extrema ficou dentro de [-1, 1]");
    }
}

// A curva é ímpar: f(-x) == -f(x). Sem isso, o limitador introduziria um
// offset DC diferente para picos positivos e negativos.
void testCurveIsSymmetric()
{
    std::cout << "curva simetrica\n";

    std::vector<float> buffer = {0.95f, -0.95f, 3.0f, -3.0f};

    Limiter limiter;
    limiter.setThreshold(0.7f);
    limiter.process(buffer);

    checkClose(buffer[0], -buffer[1], "f(0.95) == -f(-0.95)");
    checkClose(buffer[2], -buffer[3], "f(3.0) == -f(-3.0)");
}

// Silêncio continua silêncio.
void testSilenceStaysSilent()
{
    std::cout << "silencio continua silencio\n";

    std::vector<float> buffer = {0.0f, 0.0f};

    Limiter limiter;
    limiter.process(buffer);

    checkClose(buffer[0], 0.0f, "0.0 continua 0.0");
    checkClose(buffer[1], 0.0f, "0.0 continua 0.0 (2a amostra)");
}

// Reproduz o cenário real que motivou este módulo: uma nota já saturada
// (perto de 1.0) somada a uma cauda de delay ainda tocando (feedback), como
// acontece em Delay::process. Sem limitador essa soma passaria de 1.0.
void testClampsOverlappingNoteAndDelayTail()
{
    std::cout << "contem a soma de uma nota nova com a cauda do delay\n";

    // 0.98 (SoftClipper quase saturado) + 0.3 (cauda ainda ecoando) = 1.28,
    // bem acima do teto — exatamente o que a segunda nota do teste real
    // produzia antes deste modulo existir.
    std::vector<float> buffer = {0.98f + 0.3f};

    Limiter limiter;
    limiter.process(buffer);

    check(buffer[0] <= 1.0f, "a soma de 1.28 foi contida em ate 1.0");
    check(buffer[0] > 0.9f, "ainda ficou alto — nao virou silencio, so nao estourou");
}

// O getter devolve o que o setter guardou.
void testThresholdRoundTrip()
{
    std::cout << "setter e getter\n";

    Limiter limiter;

    checkClose(limiter.threshold(), 0.9f, "threshold padrao e 0.9");

    limiter.setThreshold(0.6f);
    checkClose(limiter.threshold(), 0.6f, "threshold vira 0.6");
}

// Buffer vazio não pode quebrar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    std::vector<float> buffer;

    Limiter limiter;
    limiter.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

int main()
{
    std::cout << "\n=== testes do Limiter ===\n\n";

    testSignalBelowThresholdIsUntouched();
    testSignalExactlyAtThresholdIsUntouched();
    testCurveValueAboveThreshold();
    testOutputNeverExceedsOne();
    testCurveIsSymmetric();
    testSilenceStaysSilent();
    testClampsOverlappingNoteAndDelayTail();
    testThresholdRoundTrip();
    testEmptyBufferDoesNotCrash();

    return reportResults();
}
