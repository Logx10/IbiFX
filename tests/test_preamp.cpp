// Testes do Preamp.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <cmath>
#include <iostream>
#include <vector>

#include "Preamp.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------

// Silêncio continua silêncio — tanh(0) é 0 em todo estágio, e 0 * qualquer
// coisa continua 0, então a cascata inteira preserva silêncio.
void testSilenceStaysSilent()
{
    std::cout << "silencio continua silencio\n";

    std::vector<float> buffer = {0.0f, 0.0f, 0.0f};

    Preamp preamp;
    preamp.setDrive(3.0f);
    preamp.process(buffer);

    checkClose(buffer[0], 0.0f, "0.0 continua 0.0");
    checkClose(buffer[1], 0.0f, "0.0 continua 0.0 (2a amostra)");
    checkClose(buffer[2], 0.0f, "0.0 continua 0.0 (3a amostra)");
}

// A saída nunca escapa de [-1, +1] — o último estágio é sempre uma tanh
// pura, sem reexpansão depois dele. Testado com entradas absurdas.
void testOutputNeverLeavesRange()
{
    std::cout << "saida nunca sai de [-1, +1]\n";

    std::vector<float> buffer = {50.0f, -50.0f, 1000.0f, -1000.0f};

    Preamp preamp;
    preamp.setDrive(5.0f);
    preamp.process(buffer);

    for (float sample : buffer)
    {
        check(sample <= 1.0f && sample >= -1.0f, "amostra extrema ficou dentro da faixa");
    }
}

// A cascata de 3 estágios produz um resultado bem diferente de um único
// estágio tanh(drive*x) — prova de que empilhar estágios não é decoração,
// muda o resultado de verdade.
void testCascadeDiffersFromSingleStage()
{
    std::cout << "a cascata difere de um unico estagio\n";

    const float drive = 1.0f;
    const float entrada = 0.5f;

    std::vector<float> buffer = {entrada};

    Preamp preamp;
    preamp.setDrive(drive);
    preamp.process(buffer);

    const float umEstagioSo = std::tanh(drive * entrada);

    check(std::fabs(buffer[0] - umEstagioSo) > 0.1f,
          "a cascata de 3 estagios diverge bastante de tanh(drive*x) sozinho");

    // A cascata também satura MAIS, porque cada estágio reexpande o sinal
    // do anterior antes de saturar de novo.
    check(std::fabs(buffer[0]) > std::fabs(umEstagioSo),
          "a cascata chega mais perto do teto que um estagio so, para o mesmo drive");
}

// Um sinal forte fica relativamente MAIS comprimido que um fraco: a razão
// saída/entrada cai conforme a entrada cresce — a assinatura de "responder
// ao toque" de um preamp com múltiplos estágios de ganho.
void testLouderSignalIsRelativelyMoreCompressed()
{
    std::cout << "sinal forte fica relativamente mais comprimido que fraco\n";

    std::vector<float> fraco = {0.05f};
    std::vector<float> forte = {0.5f};

    Preamp preamp;
    preamp.setDrive(1.0f);

    preamp.process(fraco);

    Preamp outroPreamp;  // instancia nova: nenhum estado de rampa compartilhado
    outroPreamp.setDrive(1.0f);
    outroPreamp.process(forte);

    const float razaoFraco = fraco[0] / 0.05f;
    const float razaoForte = forte[0] / 0.5f;

    check(razaoForte < razaoFraco,
          "a razao saida/entrada do sinal forte e menor que a do sinal fraco");
}

// Drive maior distorce mais — o parâmetro faz o que promete, igual em todo
// módulo de distorção do projeto.
void testHigherDriveSaturatesMore()
{
    std::cout << "drive maior satura mais\n";

    std::vector<float> suave = {0.3f};
    std::vector<float> forte = {0.3f};

    Preamp preampSuave;
    preampSuave.setDrive(0.5f);
    preampSuave.process(suave);

    Preamp preampForte;
    preampForte.setDrive(3.0f);
    preampForte.process(forte);

    check(forte[0] > suave[0], "drive 3.0 leva a amostra mais perto do teto que drive 0.5");
}

// Os getters devolvem o que os setters guardaram.
void testParameterRoundTrip()
{
    std::cout << "setter e getter\n";

    Preamp preamp;

    checkClose(preamp.drive(), 1.0f, "drive padrao e 1.0");

    preamp.setDrive(2.5f);
    checkClose(preamp.drive(), 2.5f, "drive vira 2.5");
}

// Buffer vazio não pode quebrar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    std::vector<float> buffer;

    Preamp preamp;
    preamp.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

int main()
{
    std::cout << "\n=== testes do Preamp ===\n\n";

    testSilenceStaysSilent();
    testOutputNeverLeavesRange();
    testCascadeDiffersFromSingleStage();
    testLouderSignalIsRelativelyMoreCompressed();
    testHigherDriveSaturatesMore();
    testParameterRoundTrip();
    testEmptyBufferDoesNotCrash();

    return reportResults();
}
