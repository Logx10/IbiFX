// Testes do DiLooper.
//
// Sample rate de 1000 Hz, como no test_compressor: 1 amostra = 1 ms, e as
// folgas viram números redondos — fade de 5 amostras, pré-roll de 20,
// pós-roll de 200.
//
// O "riff" é 100 amostras de silêncio, 300 de sinal e 500 de silêncio. O
// corte automático deve deixar o take de 80 (100 - 20 de pré-roll) a 600
// (400 + 200 de pós-roll): 520 amostras.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <cmath>
#include <iostream>
#include <vector>

#include "DiLooper.h"
#include "test_helpers.h"

namespace
{
constexpr double kRate = 1000.0;

// O valor do "riff" na amostra i: silêncio, sinal reconhecível, silêncio.
float riff(std::size_t i)
{
    if (i < 100 || i >= 400)
        return 0.0f;
    return 0.2f + 0.001f * static_cast<float>(i - 100);
}

std::vector<float> block(std::size_t count, float value)
{
    return std::vector<float>(count, value);
}

// Grava o riff inteiro e para: o looper fica com o take de 520 amostras.
void recordRiff(DiLooper& looper)
{
    looper.startRecording();
    std::vector<float> input(900);
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = riff(i);
    looper.process(input.data(), input.size());
    looper.stopRecording();
    std::vector<float> idle = block(1, 0.0f);
    looper.process(idle.data(), idle.size());
}
}

// Gravar não muda o que você ouve: a guitarra continua passando.
void testRecordingPassesTheGuitarThrough()
{
    std::cout << "gravando, a guitarra continua passando\n";

    DiLooper looper;
    looper.prepare(kRate);
    looper.startRecording();

    std::vector<float> input = block(64, 0.3f);
    looper.process(input.data(), input.size());

    check(looper.state() == DiLooper::State::Recording, "estado e Recording");
    checkClose(input[10], 0.3f, "o bloco sai como entrou");
}

// O silêncio antes e depois é cortado, com as folgas.
void testSilenceIsTrimmed()
{
    std::cout << "silencio do comeco e do fim e cortado\n";

    DiLooper looper;
    looper.prepare(kRate);
    recordRiff(looper);

    check(looper.state() == DiLooper::State::Ready, "take pronto");
    checkClose(static_cast<float>(looper.takeSeconds()), 0.520f, "take de 520 ms (80 a 600)");
    checkClose(static_cast<float>(looper.recordedSeconds()), 0.900f, "900 ms gravados");
}

// Tocando, o take ENTRA NO LUGAR da guitarra: o que estava no bloco é
// descartado. Longe das emendas o ganho é 1 e a amostra é a gravada.
void testPlaybackReplacesTheGuitar()
{
    std::cout << "tocando, o take substitui a guitarra\n";

    DiLooper looper;
    looper.prepare(kRate);
    recordRiff(looper);
    looper.play();

    std::vector<float> live = block(520, 0.9f);
    looper.process(live.data(), live.size());

    check(looper.state() == DiLooper::State::Playing, "estado e Playing");
    checkClose(live[0], 0.0f, "comeca no pre-roll (silencio), nao na guitarra ao vivo");
    checkClose(live[20], riff(100), "amostra 20 do take = amostra 100 gravada");
    checkClose(live[250], riff(330), "amostra 250 do take = amostra 330 gravada");
}

// Fade nas emendas: a primeira amostra entra em 1/5, não de supetão.
void testEdgesAreFaded()
{
    std::cout << "fade de 5 ms nas emendas\n";

    DiLooper looper;
    looper.prepare(kRate);
    looper.startRecording();
    std::vector<float> loud = block(400, 0.5f);
    looper.process(loud.data(), loud.size());
    looper.play();

    // Take: amostras 0..399 (pré-roll cortado em 0, pós-roll até o fim).
    std::vector<float> out = block(400, 0.0f);
    looper.process(out.data(), out.size());

    checkClose(out[0], 0.5f * 1.0f / 5.0f, "primeira amostra em 1/5");
    checkClose(out[4], 0.5f, "depois de 5 amostras, ganho cheio");
    checkClose(out[399], 0.5f * 1.0f / 5.0f, "ultima amostra em 1/5");
}

// Em loop, a segunda volta é idêntica à primeira.
void testLoopRepeatsTheTake()
{
    std::cout << "em loop, a segunda volta e igual a primeira\n";

    DiLooper looper;
    looper.prepare(kRate);
    recordRiff(looper);
    looper.play();

    std::vector<float> out = block(1040, 0.9f);
    looper.process(out.data(), out.size());

    float worst = 0.0f;
    for (std::size_t i = 0; i < 520; ++i)
        worst = std::max(worst, std::fabs(out[i] - out[i + 520]));
    check(worst == 0.0f, "volta 2 = volta 1, amostra por amostra");
    check(looper.state() == DiLooper::State::Playing, "continua tocando");
}

// Sem loop, ao acabar o take a guitarra ao vivo volta.
void testWithoutLoopTheGuitarComesBack()
{
    std::cout << "sem loop, a guitarra volta no fim do take\n";

    DiLooper looper;
    looper.prepare(kRate);
    looper.setLoop(false);
    recordRiff(looper);
    looper.play();

    std::vector<float> out = block(600, 0.9f);
    looper.process(out.data(), out.size());

    check(looper.state() == DiLooper::State::Ready, "parou sozinho");
    checkClose(out[550], 0.9f, "depois do take, a guitarra ao vivo");
}

// Espaço tocando: desce em fade e devolve a guitarra.
void testToggleStopsWithAFade()
{
    std::cout << "espaco tocando para com fade\n";

    DiLooper looper;
    looper.prepare(kRate);
    recordRiff(looper);
    looper.play();
    std::vector<float> first = block(200, 0.9f);
    looper.process(first.data(), first.size());

    looper.togglePlay();
    std::vector<float> out = block(20, 0.9f);
    looper.process(out.data(), out.size());

    check(looper.state() == DiLooper::State::Ready, "parou");
    check(std::fabs(out[0]) < std::fabs(riff(300)), "primeira amostra ja abaixo do take cheio");
    checkClose(out[4], 0.0f, "fim do fade em silencio");
    checkClose(out[10], 0.9f, "depois do fade, a guitarra ao vivo");
}

// Espaço gravando: fecha o take e já toca — grava o riff, aperta espaço.
void testToggleWhileRecordingPlaysTheTake()
{
    std::cout << "espaco gravando fecha o take e toca\n";

    DiLooper looper;
    looper.prepare(kRate);
    looper.startRecording();
    std::vector<float> input(900);
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = riff(i);
    looper.process(input.data(), input.size());

    looper.togglePlay();
    std::vector<float> out = block(64, 0.9f);
    looper.process(out.data(), out.size());

    check(looper.state() == DiLooper::State::Playing, "tocando");
    checkClose(out[20], riff(100), "do comeco do take");
}

// Gravar só silêncio não deixa um take mudo para tocar.
void testSilentRecordingLeavesNothing()
{
    std::cout << "gravar so silencio nao gera take\n";

    DiLooper looper;
    looper.prepare(kRate);
    looper.startRecording();
    std::vector<float> quiet = block(500, 0.001f);
    looper.process(quiet.data(), quiet.size());
    looper.stopRecording();
    looper.play();
    std::vector<float> out = block(10, 0.9f);
    looper.process(out.data(), out.size());

    check(looper.state() == DiLooper::State::Empty, "continua vazio");
    checkClose(out[5], 0.9f, "e a guitarra passa");
}

// O buffer cheio fecha o take sozinho, sem escrever fora dele.
void testFullBufferClosesTheTake()
{
    std::cout << "buffer cheio fecha o take sozinho\n";

    DiLooper looper;
    looper.prepare(kRate);
    looper.startRecording();

    const auto capacity = static_cast<std::size_t>(DiLooper::kMaxSeconds * kRate);
    std::vector<float> loud = block(1000, 0.5f);
    for (std::size_t written = 0; written <= capacity; written += loud.size())
        looper.process(loud.data(), loud.size());

    check(looper.state() == DiLooper::State::Ready, "fechou o take");
    checkClose(static_cast<float>(looper.recordedSeconds()), static_cast<float>(DiLooper::kMaxSeconds),
               "exatamente kMaxSeconds gravados");
}

// Reabrir o dispositivo na mesma taxa preserva o riff; noutra taxa, não.
void testPrepareKeepsTheTakeAtTheSameRate()
{
    std::cout << "prepare na mesma taxa preserva o take\n";

    DiLooper looper;
    looper.prepare(kRate);
    recordRiff(looper);

    looper.prepare(kRate);
    check(looper.state() == DiLooper::State::Ready, "mesma taxa: take mantido");

    looper.prepare(2000.0);
    check(looper.state() == DiLooper::State::Empty, "outra taxa: take descartado");
}

int main()
{
    std::cout << "\n=== testes do DiLooper ===\n\n";

    testRecordingPassesTheGuitarThrough();
    testSilenceIsTrimmed();
    testPlaybackReplacesTheGuitar();
    testEdgesAreFaded();
    testLoopRepeatsTheTake();
    testWithoutLoopTheGuitarComesBack();
    testToggleStopsWithAFade();
    testToggleWhileRecordingPlaysTheTake();
    testSilentRecordingLeavesNothing();
    testFullBufferClosesTheTake();
    testPrepareKeepsTheTakeAtTheSameRate();

    return reportResults();
}
