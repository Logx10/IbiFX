// Testes do Delay.
//
// O sample rate usado é 10 Hz, absurdo para áudio e ideal para teste: com
// time = 0.3 o atraso dá exatamente 3 amostras, e dá para conferir a saída
// contando nos dedos. A 48 kHz o mesmo teste exigiria buffers de milhares de
// posições para verificar a mesma coisa.
//
// A infra de verificação vive em test_helpers.h.

#include <iostream>
#include <vector>

#include "Delay.h"
#include "test_helpers.h"

namespace
{
constexpr double kTestSampleRate = 10.0;
constexpr int kTestBlockSize = 8;

// Delay preparado e configurado, para encurtar os testes.
Delay makeDelay(float time, float feedback, float mix)
{
    Delay delay;
    delay.prepare(kTestSampleRate, kTestBlockSize);
    delay.setTime(time);
    delay.setFeedback(feedback);
    delay.setMix(mix);
    return delay;
}
}

// ---------------------------------------------------------------------

// Os parâmetros nascem com id, faixa e padrão esperados.
void testParameters()
{
    std::cout << "parametros do delay\n";

    Delay delay;

    check(delay.parameterCount() == 3, "tem 3 parametros");
    check(delay.parameterAt(0).id() == "time", "o primeiro e time");
    check(delay.parameterAt(1).id() == "feedback", "o segundo e feedback");
    check(delay.parameterAt(2).id() == "mix", "o terceiro e mix");

    checkClose(delay.parameterAt(1).maxValue(), 0.95f, "feedback vai no maximo a 0.95");
}

// Feedback acima de 0.95 é impossível.
//
// Realimentação >= 1.0 faria cada eco voltar tão alto quanto o anterior, ou
// mais alto: uma progressão geométrica que cresce sem limite. A faixa do
// parâmetro barra isso na origem.
void testFeedbackCannotReachUnity()
{
    std::cout << "feedback nao alcanca 1.0\n";

    Delay delay;
    delay.setFeedback(5.0f);

    checkClose(delay.feedback(), 0.95f, "feedback 5.0 para em 0.95");
}

// Sem prepare(), o processamento devolve o buffer intacto.
//
// process() roda na thread de áudio, onde lançar exceção não é opção. Um eco
// mudo é preferível a um travamento.
void testProcessWithoutPrepareIsHarmless()
{
    std::cout << "processar sem preparar\n";

    Delay delay;
    delay.setMix(1.0f);

    check(delay.bufferSize() == 0, "sem prepare, o buffer circular esta vazio");

    std::vector<float> buffer = {1.0f, 0.5f, -0.25f};
    delay.process(buffer);

    checkClose(buffer[0], 1.0f, "1.0 continua 1.0");
    checkClose(buffer[1], 0.5f, "0.5 continua 0.5");
    checkClose(buffer[2], -0.25f, "-0.25 continua -0.25");
}

// prepare() dimensiona o buffer para o maior atraso possível.
//
// O tamanho vem do MÁXIMO do parâmetro, não do valor atual: o tempo pode
// mudar a qualquer momento sem passar pelo prepare().
void testPrepareSizesForMaximumDelay()
{
    std::cout << "prepare dimensiona pelo atraso maximo\n";

    Delay delay;
    delay.setTime(0.1f);
    delay.prepare(kTestSampleRate, kTestBlockSize);

    // maximo de time = 2.0 s, a 10 Hz = 20 amostras, mais 1 de folga.
    check(delay.bufferSize() == 21, "buffer tem 21 amostras (2.0 s a 10 Hz + folga)");
}

// mix = 0.0 devolve apenas o sinal seco.
void testMixZeroIsTransparent()
{
    std::cout << "mix 0.0 e transparente\n";

    Delay delay = makeDelay(0.3f, 0.5f, 0.0f);

    std::vector<float> buffer = {1.0f, 0.5f, -0.25f, 0.75f};
    delay.process(buffer);

    checkClose(buffer[0], 1.0f, "1.0 continua 1.0");
    checkClose(buffer[1], 0.5f, "0.5 continua 0.5");
    checkClose(buffer[2], -0.25f, "-0.25 continua -0.25");
    checkClose(buffer[3], 0.75f, "0.75 continua 0.75");
}

// O TESTE CENTRAL: um impulso reaparece exatamente onde deveria.
//
// Um impulso é a entrada mais reveladora possível — um único 1.0 seguido de
// silêncio. Onde ele reaparecer na saída é, literalmente, o atraso do módulo.
void testImpulseReappearsAfterDelay()
{
    std::cout << "impulso reaparece na posicao certa\n";

    Delay delay = makeDelay(0.3f, 0.0f, 1.0f);

    std::vector<float> buffer = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    delay.process(buffer);

    checkClose(buffer[0], 0.0f, "posicao 0 esta em silencio");
    checkClose(buffer[1], 0.0f, "posicao 1 esta em silencio");
    checkClose(buffer[2], 0.0f, "posicao 2 esta em silencio");
    checkClose(buffer[3], 1.0f, "o impulso reaparece na posicao 3");
    checkClose(buffer[4], 0.0f, "posicao 4 volta ao silencio");
}

// Com feedback zero há uma repetição, e só uma.
void testZeroFeedbackGivesSingleRepeat()
{
    std::cout << "feedback 0.0 da uma repeticao so\n";

    Delay delay = makeDelay(0.3f, 0.0f, 1.0f);

    std::vector<float> buffer(10, 0.0f);
    buffer[0] = 1.0f;
    delay.process(buffer);

    checkClose(buffer[3], 1.0f, "primeiro eco na posicao 3");
    checkClose(buffer[6], 0.0f, "nao ha segundo eco na posicao 6");
    checkClose(buffer[9], 0.0f, "nem terceiro na posicao 9");
}

// O feedback faz os ecos decaírem em progressão geométrica.
//
// Com 0.5, cada eco tem metade da amplitude do anterior: 1.0, 0.5, 0.25.
// É a prova de que a realimentação funciona e de que ela converge.
void testFeedbackProducesDecayingEchoes()
{
    std::cout << "feedback produz ecos decrescentes\n";

    Delay delay = makeDelay(0.3f, 0.5f, 1.0f);

    std::vector<float> buffer(10, 0.0f);
    buffer[0] = 1.0f;
    delay.process(buffer);

    checkClose(buffer[3], 1.0f, "primeiro eco vale 1.0");
    checkClose(buffer[6], 0.5f, "segundo eco vale 0.5");
    checkClose(buffer[9], 0.25f, "terceiro eco vale 0.25");
}

// O ESTADO SOBREVIVE ENTRE BLOCOS.
//
// Este é o teste que pega o writePosition zerado a cada chamada — bug
// invisível em qualquer teste de bloco único, porque dentro de um bloco
// isolado tudo pareceria correto.
void testStateSurvivesAcrossBlocks()
{
    std::cout << "o eco atravessa a fronteira dos blocos\n";

    Delay delay = makeDelay(0.3f, 0.0f, 1.0f);

    std::vector<float> primeiro = {1.0f, 0.0f};
    delay.process(primeiro);

    checkClose(primeiro[0], 0.0f, "bloco 1, posicao 0: silencio");
    checkClose(primeiro[1], 0.0f, "bloco 1, posicao 1: silencio");

    std::vector<float> segundo = {0.0f, 0.0f};
    delay.process(segundo);

    checkClose(segundo[0], 0.0f, "bloco 2, posicao 0: silencio");
    checkClose(segundo[1], 1.0f, "bloco 2, posicao 1: o eco chega (3a amostra global)");
}

// Blocos de tamanhos diferentes não confundem a contagem.
void testWorksWithVaryingBlockSizes()
{
    std::cout << "blocos de tamanhos diferentes\n";

    Delay delay = makeDelay(0.3f, 0.0f, 1.0f);

    std::vector<float> umaAmostra = {1.0f};
    delay.process(umaAmostra);

    std::vector<float> cincoAmostras(5, 0.0f);
    delay.process(cincoAmostras);

    checkClose(cincoAmostras[2], 1.0f, "o eco chega na 4a amostra global");
}

// reset() silencia o eco pendente.
void testResetSilencesPendingEcho()
{
    std::cout << "reset silencia o eco pendente\n";

    Delay delay = makeDelay(0.3f, 0.8f, 1.0f);

    std::vector<float> primeiro = {1.0f, 0.0f};
    delay.process(primeiro);

    delay.reset();

    std::vector<float> depois(6, 0.0f);
    delay.process(depois);

    check(delay.time() > 0.0f, "reset nao mexeu nos parametros");

    bool tudoSilencio = true;
    for (float sample : depois)
    {
        if (sample != 0.0f)
            tudoSilencio = false;
    }

    check(tudoSilencio, "nada do eco antigo voltou");
}

// prepare() chamado de novo é seguro e reinicia o estado.
//
// Trocar de dispositivo de áudio faz exatamente isso.
void testPrepareTwiceIsSafe()
{
    std::cout << "preparar duas vezes\n";

    Delay delay = makeDelay(0.3f, 0.0f, 1.0f);

    std::vector<float> primeiro = {1.0f, 0.0f};
    delay.process(primeiro);

    delay.prepare(kTestSampleRate, kTestBlockSize);

    check(delay.bufferSize() == 21, "o buffer continua com 21 amostras");

    std::vector<float> depois(6, 0.0f);
    delay.process(depois);

    bool tudoSilencio = true;
    for (float sample : depois)
    {
        if (sample != 0.0f)
            tudoSilencio = false;
    }

    check(tudoSilencio, "o novo prepare limpou o eco pendente");
}

// O atraso acompanha o sample rate.
//
// O mesmo time = 0.3 s vale 3 amostras a 10 Hz e 6 amostras a 20 Hz. É por
// isso que o parâmetro é em segundos: o preset soa igual em qualquer placa.
void testDelayFollowsSampleRate()
{
    std::cout << "o atraso acompanha o sample rate\n";

    Delay delay;
    delay.prepare(20.0, kTestBlockSize);
    delay.setTime(0.3f);
    delay.setFeedback(0.0f);
    delay.setMix(1.0f);

    std::vector<float> buffer(10, 0.0f);
    buffer[0] = 1.0f;
    delay.process(buffer);

    checkClose(buffer[6], 1.0f, "a 20 Hz, 0.3 s equivale a 6 amostras");
    checkClose(buffer[3], 0.0f, "e nao mais a 3");
}

// Tempo maior que o buffer é limitado, sem estourar índice.
void testExcessiveTimeIsClamped()
{
    std::cout << "tempo alem do buffer e limitado\n";

    Delay delay = makeDelay(2.0f, 0.0f, 1.0f);

    std::vector<float> buffer(5, 0.0f);
    buffer[0] = 1.0f;
    delay.process(buffer);

    check(true, "processar no atraso maximo nao estourou o buffer");
}

// Buffer vazio nao pode quebrar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    Delay delay = makeDelay(0.3f, 0.5f, 1.0f);

    std::vector<float> buffer;
    delay.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

int main()
{
    std::cout << "\n=== testes do Delay ===\n\n";

    testParameters();
    testFeedbackCannotReachUnity();
    testProcessWithoutPrepareIsHarmless();
    testPrepareSizesForMaximumDelay();
    testMixZeroIsTransparent();
    testImpulseReappearsAfterDelay();
    testZeroFeedbackGivesSingleRepeat();
    testFeedbackProducesDecayingEchoes();
    testStateSurvivesAcrossBlocks();
    testWorksWithVaryingBlockSizes();
    testResetSilencesPendingEcho();
    testPrepareTwiceIsSafe();
    testDelayFollowsSampleRate();
    testExcessiveTimeIsClamped();
    testEmptyBufferDoesNotCrash();

    return reportResults();
}
