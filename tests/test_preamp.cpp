// Testes do Preamp.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <cmath>
#include <cstddef>
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
    preamp.setOversampling(false);  // curva pura, sem o atraso do filtro
    preamp.setInterstageFilter(false);
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
    preamp.setOversampling(false);  // curva pura, sem o atraso do filtro
    preamp.setInterstageFilter(false);
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
    preamp.setOversampling(false);  // curva pura, sem o atraso do filtro
    preamp.setInterstageFilter(false);
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
    preamp.setOversampling(false);  // curva pura, sem o atraso do filtro
    preamp.setInterstageFilter(false);
    preamp.setDrive(1.0f);

    preamp.process(fraco);

    Preamp outroPreamp;  // instancia nova: nenhum estado de rampa compartilhado
    outroPreamp.setOversampling(false);  // curva pura, sem o atraso do filtro
    outroPreamp.setInterstageFilter(false);
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
    preampSuave.setOversampling(false);  // curva pura, sem o atraso do filtro
    preampSuave.setInterstageFilter(false);
    preampSuave.setDrive(0.5f);
    preampSuave.process(suave);

    Preamp preampForte;
    preampForte.setOversampling(false);  // curva pura, sem o atraso do filtro
    preampForte.setInterstageFilter(false);
    preampForte.setDrive(3.0f);
    preampForte.process(forte);

    check(forte[0] > suave[0], "drive 3.0 leva a amostra mais perto do teto que drive 0.5");
}

// Os getters devolvem o que os setters guardaram.
void testParameterRoundTrip()
{
    std::cout << "setter e getter\n";

    Preamp preamp;
    preamp.setOversampling(false);  // curva pura, sem o atraso do filtro
    preamp.setInterstageFilter(false);

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
    preamp.setOversampling(false);  // curva pura, sem o atraso do filtro
    preamp.setInterstageFilter(false);
    preamp.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

// ---------------------------------------------------------------------
// Filtro entre estágios

namespace
{
// Energia de um sinal na frequência f, por DFT num ponto só, a partir de
// `skip` amostras (o filtro do oversampling enchendo fica de fora).
double energyAt(const std::vector<float>& signal, double frequency, std::size_t skip)
{
    double real = 0.0;
    double imaginary = 0.0;
    for (std::size_t n = skip; n < signal.size(); ++n)
    {
        const double phase = 2.0 * 3.14159265358979323846 * frequency * static_cast<double>(n) / 48000.0;
        real += signal[n] * std::cos(phase);
        imaginary -= signal[n] * std::sin(phase);
    }
    return real * real + imaginary * imaginary;
}

// Quanto dos harmônicos de f0 está acima de `above` Hz, em dB da energia
// harmônica total.
double highHarmonicsDb(const std::vector<float>& signal, double f0, double above)
{
    double total = 0.0;
    double high = 0.0;
    for (double f = f0; f < 24000.0; f += f0)
    {
        const double e = energyAt(signal, f, 960);
        total += e;
        if (f > above)
            high += e;
    }
    return 10.0 * std::log10(high / total);
}

std::vector<float> saturatedSine(bool interstageFilter)
{
    // 1010 Hz: 4800 amostras analisadas cabem um número inteiro de
    // períodos (sem vazamento), e 48000 não é múltiplo dele.
    std::vector<float> buffer(4800 + 960);
    for (std::size_t n = 0; n < buffer.size(); ++n)
        buffer[n] = 0.5f * static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * 1010.0 * static_cast<double>(n) / 48000.0));

    Preamp preamp;
    preamp.prepare(48000.0, 128);
    preamp.setDrive(3.0f);
    preamp.setInterstageFilter(interstageFilter);
    preamp.reset();
    preamp.process(buffer);
    return buffer;
}
}

// O motivo do filtro: com os três estágios saturando um mi agudo com
// força, a parte dos harmônicos acima de 8 kHz — o chiado áspero — cai
// bastante; o corpo da nota (os harmônicos de baixo) não muda.
void testInterstageFilterTamesHighHarmonics()
{
    std::cout << "filtro entre estagios tira o chiado dos harmonicos agudos\n";

    const std::vector<float> filtered = saturatedSine(true);
    const std::vector<float> unfiltered = saturatedSine(false);

    const double withFilter = highHarmonicsDb(filtered, 1010.0, 8000.0);
    const double withoutFilter = highHarmonicsDb(unfiltered, 1010.0, 8000.0);

    std::cout << "    harmonicos acima de 8 kHz: sem filtro " << withoutFilter
              << " dB, com filtro " << withFilter << " dB\n";

    check(withFilter < withoutFilter - 4.0, "pelo menos 4 dB a menos de energia acima de 8 kHz");

    const double fundamentalChangeDb =
        10.0 * std::log10(energyAt(filtered, 1010.0, 960) / energyAt(unfiltered, 1010.0, 960));
    check(std::fabs(fundamentalChangeDb) < 1.5, "a fundamental continua praticamente igual (+-1.5 dB)");
}

void testInterstageCutoffRoundTrip()
{
    std::cout << "corte entre estagios: padrao e setter\n";

    Preamp preamp;
    checkClose(preamp.interstageCutoff(), 8000.0f, "padrao em 8 kHz");
    preamp.setInterstageCutoff(6000.0f);
    checkClose(preamp.interstageCutoff(), 6000.0f, "vira 6 kHz");
}

// ---------------------------------------------------------------------
// Bias (assimetria de estágio)

// Com bias 0 (o padrão), a curva renormalizada colapsa em tanh(drive*x)
// pura e a cascata bate EXATAMENTE com a conta feita à mão do jeito antigo
// — mesma garantia de neutralidade do presence no PowerAmp: quem nunca
// mexer no bias ouve o módulo de sempre.
void testZeroBiasMatchesTheOldSymmetricCascade()
{
    std::cout << "bias 0 e identico a cascata simetrica de antes\n";

    const float drive = 1.0f;
    const float entrada = 0.5f;

    std::vector<float> buffer = {entrada};

    Preamp preamp;
    preamp.setOversampling(false);  // curva pura, sem o atraso do filtro
    preamp.setInterstageFilter(false);
    preamp.setDrive(drive);
    preamp.process(buffer);

    // A cascata de antes, à mão: tanh -> x2 -> tanh -> x2 -> tanh.
    float esperado = entrada;
    esperado = std::tanh(drive * esperado) * 2.0f;
    esperado = std::tanh(drive * esperado) * 2.0f;
    esperado = std::tanh(drive * esperado);

    checkClose(buffer[0], esperado, "cascata com bias 0 = cascata simetrica calculada a mao");
    checkClose(preamp.bias(), 0.0f, "bias padrao e 0");
}

// O motivo do bias existir: com ele, os dois semiciclos saem DIFERENTES — a
// assinatura de uma não-linearidade assimétrica, que é o que gera os
// harmônicos pares (ver "O BIAS" em Preamp.h). Uma tanh simétrica devolve
// |saida| igual para +x e -x, sempre.
void testBiasMakesHalfCyclesAsymmetric()
{
    std::cout << "bias deixa os semiciclos assimetricos\n";

    auto processa = [](float entrada) -> float
    {
        std::vector<float> buffer = {entrada};

        Preamp preamp;
        preamp.setOversampling(false);  // curva pura, sem o atraso do filtro
        preamp.setInterstageFilter(false);
        preamp.setDrive(1.0f);
        preamp.setBias(0.2f);
        preamp.process(buffer);
        return buffer[0];
    };

    const float positivo = processa(0.3f);
    const float negativo = processa(-0.3f);

    check(std::fabs(std::fabs(positivo) - std::fabs(negativo)) > 0.01f,
          "a mesma amplitude sai com magnitudes diferentes nos dois semiciclos");
}

// A renormalização garante a faixa mesmo com o bias no máximo — o piso fica
// exatamente em -1 e o teto um pouco abaixo de +1 (ver "O BIAS" em
// Preamp.h). Mesmo teste de entradas absurdas do caso simétrico.
void testOutputStaysInRangeWithMaxBias()
{
    std::cout << "saida continua em [-1, +1] com bias no maximo\n";

    std::vector<float> buffer = {50.0f, -50.0f, 1000.0f, -1000.0f};

    Preamp preamp;
    preamp.setOversampling(false);  // curva pura, sem o atraso do filtro
    preamp.setInterstageFilter(false);
    preamp.setDrive(5.0f);
    preamp.setBias(0.3f);
    preamp.process(buffer);

    for (float sample : buffer)
    {
        check(sample <= 1.0f && sample >= -1.0f, "amostra extrema ficou dentro da faixa");
    }
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

    testInterstageFilterTamesHighHarmonics();
    testInterstageCutoffRoundTrip();

    testZeroBiasMatchesTheOldSymmetricCascade();
    testBiasMakesHalfCyclesAsymmetric();
    testOutputStaysInRangeWithMaxBias();

    return reportResults();
}
