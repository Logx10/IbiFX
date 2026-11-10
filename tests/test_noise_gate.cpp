// Testes do NoiseGate.
//
// Sample rate de 1000 Hz em quase todos os testes, de propósito: não é um
// valor real de áudio, mas torna o número de amostras por segundo redondo
// (1 amostra = 1 ms), então dá para prever o gain exato depois de N amostras
// à mão e comparar contra o que o código calcula — em vez de só checar "abriu
// mais" ou "fechou mais", sem saber se é a curva certa.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <cmath>
#include <iostream>
#include <vector>

#include "NoiseGate.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------

// Sem sinal nenhum acima do threshold, o gate nasce fechado e continua
// fechado: silêncio na entrada é silêncio na saída, e não deveria "abrir por
// engano" com entrada zero.
void testSilenceStaysClosed()
{
    std::cout << "silencio mantem o gate fechado\n";

    std::vector<float> buffer(10, 0.0f);

    NoiseGate gate;
    gate.prepare(1000.0, 64);
    gate.process(buffer);

    for (float sample : buffer)
    {
        checkClose(sample, 0.0f, "amostra continua 0.0");
    }
}

// A primeira amostra de um sinal alto não sai em volume total — o gate ainda
// está no meio do ataque. O valor exato é input * attackCoeff, porque o gain
// parte de 0.0 e dá um único passo do filtro de um polo.
void testFirstLoudSampleIsAttenuatedByAttack()
{
    std::cout << "a primeira amostra alta ainda esta abrindo\n";

    const double sampleRate = 1000.0;
    const float attackSeconds = 0.002f;  // igual ao kAttackSeconds do .cpp
    const float attackCoeff = 1.0f - std::exp(-1.0f / (attackSeconds * static_cast<float>(sampleRate)));

    std::vector<float> buffer = {0.5f};

    NoiseGate gate;
    gate.prepare(sampleRate, 64);
    gate.process(buffer);

    checkClose(buffer[0], 0.5f * attackCoeff, "primeira amostra = entrada * coeficiente de ataque");
}

// Depois de amostras suficientes de sinal constante acima do threshold, o
// gate converge para totalmente aberto — a saída se aproxima da entrada.
void testSustainedLoudSignalFullyOpensGate()
{
    std::cout << "sinal sustentado acima do threshold abre o gate\n";

    std::vector<float> buffer(200, 0.5f);

    NoiseGate gate;
    gate.prepare(1000.0, 64);
    gate.process(buffer);

    check(std::fabs(buffer.back() - 0.5f) < 1e-4f, "depois de 200 amostras, saida quase igual a entrada");
}

// O gate fecha depois que o sinal cai abaixo do threshold, seguindo a curva
// exponencial do release: gain(n) = gain0 * exp(-n / (release * sampleRate)).
//
// gain0 não é 1.0 aqui: desde que o envelope passou a decidir quando fechar
// (não mais a amostra crua), a transição de alto para fraco não fecha o
// gate no mesmo instante — o ENVELOPE ainda carrega o nível alto anterior e
// precisa de algumas constantes de tempo própria para cair abaixo do
// threshold antes do gate começar a liberar de verdade (ver o comentário
// "A decisão é tomada em cima de um envelope" em NoiseGate.h — é
// exatamente esse atraso que corrige o chiado/corte dentro da nota). Por
// isso o teste deixa uma fase de "assentar" o envelope ANTES de medir a
// curva, e mede o gain de partida em vez de assumir 1.0. A fase de
// assentar também precisa engolir o HOLD (50 ms = 50 amostras a 1000 Hz),
// que mantém o gate aberto antes de o release sequer começar.
void testGateClosesFollowingReleaseCurve()
{
    std::cout << "o gate fecha seguindo a curva de release, uma vez que o envelope ja assentou\n";

    const double sampleRate = 1000.0;
    const float releaseSeconds = 0.01f;  // 10 ms = 10 amostras a 1000 Hz

    NoiseGate gate;
    gate.setRelease(releaseSeconds);
    gate.prepare(sampleRate, 64);

    // Abre o gate primeiro, com bastante sinal alto para ele convergir perto
    // de 1.0 antes do teste de verdade começar.
    std::vector<float> abrindo(200, 0.5f);
    gate.process(abrindo);
    check(std::fabs(abrindo.back() - 0.5f) < 1e-4f, "gate abriu antes do teste de release");

    // Sinal fraco, abaixo do threshold padrão (0.005). A entrada não pode ser
    // 0 (a saída seria sempre 0 e não revelaria o gain), nem alta o
    // bastante para o gate continuar interpretando como sinal — 0.001 fica
    // bem abaixo do threshold e ainda deixa o gain visível na saída.
    const float entradaFraca = 0.001f;

    // 130 amostras: ~30 para o envelope descer do nível alto anterior até
    // abaixo do limiar de fechar (constante de tempo de ~5.5 amostras a
    // 1000 Hz), 50 do hold segurando o gate aberto, e o resto já é a curva
    // de release do GAIN rodando — que é o que se mede a seguir.
    std::vector<float> assentando(130, entradaFraca);
    gate.process(assentando);
    const float gainInicial = assentando.back() / entradaFraca;
    check(gainInicial > 1e-3f && gainInicial < 1.0f,
          "gain de partida da medicao esta num meio-termo mensuravel, nem 1.0 nem ja zerado");

    std::vector<float> fechando(20, entradaFraca);
    gate.process(fechando);

    const float n = static_cast<float>(fechando.size());
    const float gainEsperado = gainInicial * std::exp(-n / (releaseSeconds * static_cast<float>(sampleRate)));
    const float gainObtido = fechando.back() / entradaFraca;

    check(std::fabs(gainObtido - gainEsperado) < 1e-4f,
          "gain apos 20 amostras bate com a curva exponencial a partir do gain de partida medido");
}

// No threshold exato, a amostra NÃO conta como "acima" — só > abre o gate,
// não >=. Com o gate fechado, uma amostra exatamente no threshold deve
// continuar fazendo o gate caminhar para fechado, não para aberto.
void testExactlyAtThresholdDoesNotOpen()
{
    std::cout << "exatamente no threshold nao conta como sinal\n";

    NoiseGate gate;
    gate.setThreshold(0.1f);
    gate.prepare(1000.0, 64);

    std::vector<float> buffer(50, 0.1f);
    gate.process(buffer);

    checkClose(buffer.back(), 0.0f, "sinal no threshold nao abre o gate, fica em 0.0");
}

// reset() devolve o gate ao estado fechado, mesmo depois de ter aberto —
// senão o som anterior "vazaria" pelo gate já aberto ao trocar de preset.
void testResetClosesTheGate()
{
    std::cout << "reset fecha o gate\n";

    const double sampleRate = 1000.0;
    const float attackSeconds = 0.002f;
    const float attackCoeff = 1.0f - std::exp(-1.0f / (attackSeconds * static_cast<float>(sampleRate)));

    NoiseGate gate;
    gate.prepare(sampleRate, 64);

    std::vector<float> abrindo(200, 0.5f);
    gate.process(abrindo);
    check(std::fabs(abrindo.back() - 0.5f) < 1e-4f, "gate abriu");

    gate.reset();

    std::vector<float> depoisDoReset = {0.5f};
    gate.process(depoisDoReset);

    checkClose(depoisDoReset[0], 0.5f * attackCoeff,
               "depois do reset, a primeira amostra volta a se comportar como um ataque do zero");
}

// Os getters devolvem o que os setters guardaram.
void testThresholdAndReleaseRoundTrip()
{
    std::cout << "setters e getters\n";

    NoiseGate gate;

    checkClose(gate.threshold(), 0.005f, "threshold padrao e 0.005");
    checkClose(gate.release(), 0.15f, "release padrao e 0.15");

    gate.setThreshold(0.1f);
    gate.setRelease(0.3f);

    checkClose(gate.threshold(), 0.1f, "threshold vira 0.1");
    checkClose(gate.release(), 0.3f, "release vira 0.3");
}

// Uma palhetada leve pode chegar com pico de apenas 0.01 antes do ganho.
// O envelope retificado fica abaixo do pico, então o antigo threshold 0.02
// mantinha o gate fechado mesmo depois de a nota estabilizar.
void testQuietNoteOpensWithDefaultThreshold()
{
    std::cout << "nota suave abre o gate no valor padrao\n";

    constexpr double sampleRate = 48000.0;
    constexpr double twoPi = 6.283185307179586;
    std::vector<float> input(static_cast<std::size_t>(sampleRate * 0.2));
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = 0.01f * std::sin(twoPi * 220.0 * static_cast<double>(i) / sampleRate);

    NoiseGate gate;
    gate.prepare(sampleRate, 128);
    auto output = input;
    gate.process(output);

    double inputEnergy = 0.0;
    double outputEnergy = 0.0;
    for (std::size_t i = input.size() / 2; i < input.size(); ++i)
    {
        inputEnergy += static_cast<double>(input[i]) * input[i];
        outputEnergy += static_cast<double>(output[i]) * output[i];
    }
    check(outputEnergy > 0.9 * inputEnergy,
          "nota de pico 0.01 passa sem exigir uma palhetada forte");
}

// A histerese: um sinal na FAIXA entre o limiar de fechar (metade do de
// abrir) e o de abrir mantém o estado atual, nos dois sentidos — um gate
// aberto fica aberto, um fechado fica fechado. É o que impede o "chatter"
// de uma nota decaindo devagar pelo limiar (ver "HISTERESE" em NoiseGate.h).
void testHysteresisKeepsStateInsideTheBand()
{
    std::cout << "na faixa de histerese, o estado atual e mantido\n";

    const double sampleRate = 1000.0;

    // threshold 0.1 -> limiar de fechar 0.05. A entrada 0.07 fica no meio
    // da faixa: acima de fechar, abaixo de abrir.
    const float dentroDaFaixa = 0.07f;

    // Lado 1: gate ABERTO continua aberto dentro da faixa. Sem histerese,
    // 0.07 < 0.1 mandaria o gain para 0 e a nota decaindo seria cortada.
    NoiseGate aberto;
    aberto.setThreshold(0.1f);
    aberto.prepare(sampleRate, 64);

    std::vector<float> abrindo(200, 0.5f);
    aberto.process(abrindo);

    std::vector<float> decaindo(400, dentroDaFaixa);
    aberto.process(decaindo);

    check(decaindo.back() / dentroDaFaixa > 0.99f,
          "sinal na faixa de histerese nao fecha um gate aberto");

    // Lado 2: gate FECHADO continua fechado dentro da faixa — a histerese
    // não pode virar um threshold efetivo mais baixo pra abrir.
    NoiseGate fechado;
    fechado.setThreshold(0.1f);
    fechado.prepare(sampleRate, 64);

    std::vector<float> tentando(400, dentroDaFaixa);
    fechado.process(tentando);

    checkClose(tentando.back(), 0.0f, "sinal na faixa de histerese nao abre um gate fechado");
}

// O hold: depois que o sinal some, o gate fica TOTALMENTE aberto por 50 ms
// antes de o release começar — o fim de uma nota staccato não é comido (ver
// "HOLD" em NoiseGate.h). O teste usa um release curto de propósito: se o
// release começasse na hora, o gain já teria desabado dentro da janela do
// hold.
void testHoldKeepsGateOpenBeforeReleaseStarts()
{
    std::cout << "o hold segura o gate aberto antes do release comecar\n";

    const double sampleRate = 1000.0;  // 50 ms de hold = 50 amostras

    NoiseGate gate;
    gate.setRelease(0.01f);  // 10 amostras — bem mais curto que o hold
    gate.prepare(sampleRate, 64);

    std::vector<float> abrindo(200, 0.5f);
    gate.process(abrindo);

    // 40 amostras de quase-silêncio: o envelope cruza o limiar de fechar em
    // ~30, então o hold está só começando — o gain tem que continuar ~1.
    // Sem o hold, o release (10 amostras) já teria derrubado o gain para
    // menos da metade.
    const float entradaFraca = 0.001f;
    std::vector<float> dentroDoHold(40, entradaFraca);
    gate.process(dentroDoHold);

    check(dentroDoHold.back() / entradaFraca > 0.9f,
          "dentro da janela do hold, o gate continua aberto");

    // Mais 90 amostras: o hold (50) esgota e o release corre ~40 amostras —
    // 4 constantes de tempo, gain abaixo de 5%. O hold segura, mas acaba.
    std::vector<float> depoisDoHold(90, entradaFraca);
    gate.process(depoisDoHold);

    check(depoisDoHold.back() / entradaFraca < 0.05f,
          "esgotado o hold, o release fecha o gate normalmente");
}

// Buffer vazio não pode quebrar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    std::vector<float> buffer;

    NoiseGate gate;
    gate.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

int main()
{
    std::cout << "\n=== testes do NoiseGate ===\n\n";

    testSilenceStaysClosed();
    testFirstLoudSampleIsAttenuatedByAttack();
    testSustainedLoudSignalFullyOpensGate();
    testGateClosesFollowingReleaseCurve();
    testExactlyAtThresholdDoesNotOpen();
    testResetClosesTheGate();
    testThresholdAndReleaseRoundTrip();
    testQuietNoteOpensWithDefaultThreshold();
    testHysteresisKeepsStateInsideTheBand();
    testHoldKeepsGateOpenBeforeReleaseStarts();
    testEmptyBufferDoesNotCrash();

    return reportResults();
}
