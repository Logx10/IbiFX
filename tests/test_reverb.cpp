// Testes do Reverb.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <cmath>
#include <iostream>
#include <numeric>
#include <vector>

#include "Reverb.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------

// Silêncio continua silêncio, sem NaN nem infinito escondido no loop de
// feedback dos combs.
void testSilenceStaysSilent()
{
    std::cout << "silencio continua silencio, sem NaN\n";

    std::vector<float> buffer(20, 0.0f);

    Reverb reverb;
    reverb.prepare(44100.0, 64);
    reverb.process(buffer);

    for (float sample : buffer)
    {
        check(std::isfinite(sample), "amostra e finita");
        checkClose(sample, 0.0f, "0.0 continua 0.0");
    }
}

// Com mix 0.0, a saída é exatamente o sinal seco — nenhuma cauda deveria
// escapar quando o controle diz "0% de reverb".
void testZeroMixIsFullyDry()
{
    std::cout << "mix 0.0 e completamente seco\n";

    std::vector<float> buffer = {1.0f, 0.5f, -0.3f, 0.0f, 0.8f};
    const std::vector<float> original = buffer;

    Reverb reverb;
    reverb.setMix(0.0f);
    reverb.prepare(44100.0, 64);
    reverb.process(buffer);

    for (std::size_t i = 0; i < buffer.size(); ++i)
    {
        checkClose(buffer[i], original[i], "amostra continua igual a entrada com mix 0.0");
    }
}

// A primeira amostra de um impulso, com os buffers dos combs ainda vazios
// (zerados), não tem nada vindo do loop de feedback ainda — o "molhado" é
// exatamente 0.0 nesse instante. Com mix 0.3, a saída é só a parte seca:
// 1.0 * (1 - 0.3) = 0.7.
void testFirstSampleHasNoWetYet()
{
    std::cout << "primeira amostra do impulso ainda nao tem cauda\n";

    std::vector<float> buffer = {1.0f};

    Reverb reverb;
    reverb.prepare(44100.0, 64);  // mix padrao 0.3
    reverb.process(buffer);

    checkClose(buffer[0], 0.7f, "1.0 * (1 - 0.3), sem contribuicao do reverb ainda");
}

// Um impulso produz uma CAUDA: energia aparecendo bem depois do impulso
// original, quando os combs terminam de dar a volta nos seus buffers
// circulares e devolvem o que gravaram.
void testImpulseProducesATail()
{
    std::cout << "um impulso produz cauda depois dele\n";

    // 1400 amostras cobre o maior comb (1356 amostras a 44100 Hz).
    std::vector<float> buffer(1400, 0.0f);
    buffer[0] = 1.0f;

    Reverb reverb;
    reverb.prepare(44100.0, 64);
    reverb.process(buffer);

    // Soma o valor absoluto de tudo DEPOIS do impulso original. Se o reverb
    // não fizesse nada, essa soma seria exatamente 0.
    float energiaDaCauda = 0.0f;

    for (std::size_t i = 1; i < buffer.size(); ++i)
    {
        energiaDaCauda += std::fabs(buffer[i]);
    }

    check(energiaDaCauda > 0.01f, "existe energia audivel depois do impulso original");
}

// reset() apaga a cauda: depois de um impulso e de um reset, um bloco de
// silêncio tem que continuar silêncio — nada de eco "vazando" de antes do
// reset.
void testResetClearsTheTail()
{
    std::cout << "reset apaga a cauda\n";

    Reverb reverb;
    reverb.prepare(44100.0, 64);

    std::vector<float> impulso(1400, 0.0f);
    impulso[0] = 1.0f;
    reverb.process(impulso);

    reverb.reset();

    std::vector<float> silencio(1400, 0.0f);
    reverb.process(silencio);

    for (float sample : silencio)
    {
        checkClose(sample, 0.0f, "sem cauda depois do reset");
    }
}

// Decay maior deveria produzir uma cauda com mais energia remanescente mais
// tarde — é o parâmetro fazendo o que promete.
//
// PRECISA DE VÁRIOS CICLOS DO COMB PARA APARECER
// O feedback só afeta o que é escrito de volta no buffer, não a amostra que
// está sendo lida agora — então o PRIMEIRO eco de cada comb (lido em n =
// comprimento do comb) tem a mesma amplitude não importa o decay: é o
// impulso original, intocado, voltando pela primeira vez. É só a partir do
// SEGUNDO eco (n = 2 * comprimento) que o feedback realmente entra em cena.
// Por isso o buffer aqui precisa ser bem maior que o comb mais longo (1356
// amostras) — o suficiente para vários ciclos se acumularem e a diferença
// entre os dois decays ficar visível.
void testHigherDecayProducesLongerTail()
{
    std::cout << "decay maior produz cauda mais forte\n";

    constexpr std::size_t kBufferSize = 8000;

    auto energiaTardia = [](float decayValue) -> float
    {
        std::vector<float> buffer(kBufferSize, 0.0f);
        buffer[0] = 1.0f;

        Reverb reverb;
        reverb.setDecay(decayValue);
        reverb.prepare(44100.0, 64);
        reverb.process(buffer);

        // Só o último quarto do buffer: depois de vários ciclos, é onde um
        // decay baixo já morreu e um decay alto ainda está tocando.
        float soma = 0.0f;

        for (std::size_t i = 3 * kBufferSize / 4; i < kBufferSize; ++i)
        {
            soma += std::fabs(buffer[i]);
        }

        return soma;
    };

    const float energiaBaixa = energiaTardia(0.2f);
    const float energiaAlta = energiaTardia(0.9f);

    check(energiaAlta > energiaBaixa, "decay 0.9 tem mais energia tardia que decay 0.2");
}

// Mesmo no decay máximo permitido pelo parâmetro (0.98, nunca 1.0 — feedback
// >= 1 nunca decairia), o sinal não pode "explodir": os combs têm
// amortecimento, e a soma é dividida pela quantidade de combs.
void testStaysBoundedAtMaximumDecay()
{
    std::cout << "nao explode no decay maximo\n";

    std::vector<float> buffer(5000, 0.5f);

    Reverb reverb;
    reverb.setDecay(0.98f);
    reverb.setDamping(0.1f);  // pouco amortecimento: o pior caso pra estabilidade
    reverb.prepare(44100.0, 64);
    reverb.process(buffer);

    for (float sample : buffer)
    {
        check(std::isfinite(sample), "amostra e finita");
        check(std::fabs(sample) < 10.0f, "amostra nao explodiu (ficou bem abaixo de 10x a entrada)");
    }
}

// Os getters devolvem o que os setters guardaram.
void testParameterRoundTrip()
{
    std::cout << "setters e getters\n";

    Reverb reverb;

    checkClose(reverb.decay(), 0.5f, "decay padrao e 0.5");
    checkClose(reverb.damping(), 0.5f, "damping padrao e 0.5");
    checkClose(reverb.mix(), 0.3f, "mix padrao e 0.3");

    reverb.setDecay(0.7f);
    reverb.setDamping(0.2f);
    reverb.setMix(0.6f);

    checkClose(reverb.decay(), 0.7f, "decay vira 0.7");
    checkClose(reverb.damping(), 0.2f, "damping vira 0.2");
    checkClose(reverb.mix(), 0.6f, "mix vira 0.6");
}

// Buffer vazio não pode quebrar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    std::vector<float> buffer;

    Reverb reverb;
    reverb.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

// process() sem prepare() não deveria travar nem estourar — os buffers dos
// combs/allpass nascem vazios, e processComb/processAllpass devolvem a
// entrada sem tocar nela quando o buffer está vazio.
void testProcessWithoutPrepareDoesNotCrash()
{
    std::cout << "process sem prepare nao quebra\n";

    std::vector<float> buffer = {0.5f, -0.5f, 0.2f};

    Reverb reverb;
    reverb.process(buffer);

    for (float sample : buffer)
    {
        check(std::isfinite(sample), "amostra e finita mesmo sem prepare");
    }
}

int main()
{
    std::cout << "\n=== testes do Reverb ===\n\n";

    testSilenceStaysSilent();
    testZeroMixIsFullyDry();
    testFirstSampleHasNoWetYet();
    testImpulseProducesATail();
    testResetClearsTheTail();
    testHigherDecayProducesLongerTail();
    testStaysBoundedAtMaximumDecay();
    testParameterRoundTrip();
    testEmptyBufferDoesNotCrash();
    testProcessWithoutPrepareDoesNotCrash();

    return reportResults();
}
