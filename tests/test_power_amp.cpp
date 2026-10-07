// Testes do PowerAmp.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

#include "PowerAmp.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------

// Silêncio continua silêncio — o envelope de sag parte de 0, e com entrada
// 0 a tanh continua devolvendo 0 não importa o drive efetivo.
void testSilenceStaysSilent()
{
    std::cout << "silencio continua silencio\n";

    std::vector<float> buffer = {0.0f, 0.0f, 0.0f};

    PowerAmp powerAmp;
    powerAmp.setOversampling(false);  // curva pura, sem o atraso do filtro
    powerAmp.setSag(1.0f);
    powerAmp.process(buffer);

    checkClose(buffer[0], 0.0f, "0.0 continua 0.0");
    checkClose(buffer[1], 0.0f, "0.0 continua 0.0 (2a amostra)");
    checkClose(buffer[2], 0.0f, "0.0 continua 0.0 (3a amostra)");
}

// A saída nunca escapa de [-1, +1] — o pior caso do sag é aumentar o drive
// efetivo, nunca diminuí-lo, então a saída final continua sendo uma tanh
// comum, só que mais apertada.
void testOutputNeverLeavesRange()
{
    std::cout << "saida nunca sai de [-1, +1]\n";

    std::vector<float> buffer = {50.0f, -50.0f, 1000.0f, -1000.0f};

    PowerAmp powerAmp;
    powerAmp.setOversampling(false);  // curva pura, sem o atraso do filtro
    powerAmp.setDrive(5.0f);
    powerAmp.setSag(1.0f);
    powerAmp.process(buffer);

    for (float sample : buffer)
    {
        check(sample <= 1.0f && sample >= -1.0f, "amostra extrema ficou dentro da faixa");
    }
}

// Com sag 0.0, o drive efetivo é sempre igual ao drive base — o módulo vira
// um único estágio tanh(drive*x) comum, sem nenhuma influência do envelope.
void testZeroSagIsPlainTanh()
{
    std::cout << "sag 0.0 vira tanh(drive*x) simples\n";

    const float drive = 2.0f;
    std::vector<float> buffer = {0.6f, -0.3f, 0.9f};

    PowerAmp powerAmp;
    powerAmp.setOversampling(false);  // curva pura, sem o atraso do filtro
    powerAmp.setDrive(drive);
    powerAmp.setSag(0.0f);
    powerAmp.process(buffer);

    checkClose(buffer[0], std::tanh(drive * 0.6f), "0.6 bate com tanh(drive*x) puro");
    checkClose(buffer[1], std::tanh(drive * -0.3f), "-0.3 bate com tanh(drive*x) puro");
    checkClose(buffer[2], std::tanh(drive * 0.9f), "0.9 bate com tanh(drive*x) puro");
}

// O CORAÇÃO DO MÓDULO: uma passagem tocada forte deixa a amostra seguinte
// mais comprimida, mesmo que ela mesma seja fraca — a "memória" do sag.
// Testado comparando a MESMA nota fraca depois de um trecho alto vs depois
// de silêncio.
void testLoudPassageLeavesMemoryOnQuietNote()
{
    std::cout << "trecho alto deixa memoria (sag) na nota fraca seguinte\n";

    auto testarDepoisDoHistorico = [](bool historicoAlto) -> float
    {
        PowerAmp powerAmp;
        powerAmp.setOversampling(false);  // curva pura, sem o atraso do filtro
        powerAmp.setDrive(1.0f);
        powerAmp.setSag(1.0f);
        powerAmp.prepare(1000.0, 64);

        // 500 amostras a 1000 Hz = 0.5 s — de sobra para o envelope (ataque
        // de 50 ms) convergir perto do valor sustentado.
        std::vector<float> historico(500, historicoAlto ? 0.9f : 0.0f);
        powerAmp.process(historico);

        std::vector<float> notaFraca = {0.05f};
        powerAmp.process(notaFraca);

        return notaFraca[0];
    };

    const float depoisDeAlto = testarDepoisDoHistorico(true);
    const float depoisDeSilencio = testarDepoisDoHistorico(false);

    check(std::fabs(depoisDeAlto) > std::fabs(depoisDeSilencio),
          "a mesma nota fraca sai mais forte/comprimida depois de um trecho alto");
}

// Sag maior amplifica esse efeito de memória — o parâmetro faz o que
// promete.
void testHigherSagAmplifiesTheMemoryEffect()
{
    std::cout << "sag maior amplifica o efeito de memoria\n";

    auto diferencaComSag = [](float sagValue) -> float
    {
        auto testarDepoisDoHistorico = [sagValue](bool historicoAlto) -> float
        {
            PowerAmp powerAmp;
            powerAmp.setOversampling(false);  // curva pura, sem o atraso do filtro
            powerAmp.setDrive(1.0f);
            powerAmp.setSag(sagValue);
            powerAmp.prepare(1000.0, 64);

            std::vector<float> historico(500, historicoAlto ? 0.9f : 0.0f);
            powerAmp.process(historico);

            std::vector<float> notaFraca = {0.05f};
            powerAmp.process(notaFraca);

            return notaFraca[0];
        };

        return testarDepoisDoHistorico(true) - testarDepoisDoHistorico(false);
    };

    const float diferencaComSagBaixo = diferencaComSag(0.2f);
    const float diferencaComSagAlto = diferencaComSag(1.0f);

    check(diferencaComSagAlto > diferencaComSagBaixo,
          "sag 1.0 produz uma diferenca maior que sag 0.2");
}

// reset() apaga a memória do envelope — depois dele, uma nota fraca não
// deveria carregar nenhum resquício de sinal alto anterior.
//
// NÃO comparamos contra tanh(drive*x) calculado à mão: dentro do laço, o
// envelope é atualizado a partir da AMOSTRA ATUAL antes de ser usado nela
// mesma (mesma ordem do NoiseGate e do Compressor) — então até a primeira
// amostra depois de um reset contribui um pouquinho pra si própria, e o
// resultado não é EXATAMENTE tanh(drive*x) puro. Em vez disso, comparamos
// contra uma instância nova processando a mesma amostra isolada: as duas
// partem de m_envelope = 0 de forma idêntica, então devem bater byte a
// byte — é essa igualdade que prova que reset() realmente zerou tudo.
void testResetClearsTheSagMemory()
{
    std::cout << "reset apaga a memoria do sag\n";

    PowerAmp aquecido;
    aquecido.setOversampling(false);  // curva pura, sem o atraso do filtro
    aquecido.setDrive(1.0f);
    aquecido.setSag(1.0f);
    aquecido.prepare(1000.0, 64);

    std::vector<float> alto(500, 0.9f);
    aquecido.process(alto);

    aquecido.reset();

    std::vector<float> notaFracaDepoisDoReset = {0.05f};
    aquecido.process(notaFracaDepoisDoReset);

    PowerAmp semHistorico;
    semHistorico.setOversampling(false);  // curva pura, sem o atraso do filtro
    semHistorico.setDrive(1.0f);
    semHistorico.setSag(1.0f);
    semHistorico.prepare(1000.0, 64);

    std::vector<float> notaFracaSemHistorico = {0.05f};
    semHistorico.process(notaFracaSemHistorico);

    checkClose(notaFracaDepoisDoReset[0], notaFracaSemHistorico[0],
               "depois do reset, a nota fraca sai identica a uma instancia nova");
}

// Os getters devolvem o que os setters guardaram.
void testParameterRoundTrip()
{
    std::cout << "setters e getters\n";

    PowerAmp powerAmp;
    powerAmp.setOversampling(false);  // curva pura, sem o atraso do filtro

    checkClose(powerAmp.drive(), 1.0f, "drive padrao e 1.0");
    checkClose(powerAmp.sag(), 0.3f, "sag padrao e 0.3");

    powerAmp.setDrive(2.5f);
    powerAmp.setSag(0.8f);

    checkClose(powerAmp.drive(), 2.5f, "drive vira 2.5");
    checkClose(powerAmp.sag(), 0.8f, "sag vira 0.8");
}

// Buffer vazio não pode quebrar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    std::vector<float> buffer;

    PowerAmp powerAmp;
    powerAmp.setOversampling(false);  // curva pura, sem o atraso do filtro
    powerAmp.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

// ---------------------------------------------------------------------
// Presence

namespace
{
// RMS da saída de um seno pequeno (região quase linear da tanh) por um
// PowerAmp com o presence dado. Sem sag, para medir só o filtro.
double outputRms(float presence, double frequency)
{
    std::vector<float> buffer(4800 + 960);
    for (std::size_t n = 0; n < buffer.size(); ++n)
        buffer[n] = 0.01f * static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * frequency * static_cast<double>(n) / 48000.0));

    PowerAmp powerAmp;
    powerAmp.prepare(48000.0, 128);
    powerAmp.setDrive(0.5f);
    powerAmp.setSag(0.0f);
    powerAmp.setPresence(presence);
    powerAmp.reset();
    powerAmp.process(buffer);

    double sum = 0.0;
    for (std::size_t n = 960; n < buffer.size(); ++n)
        sum += static_cast<double>(buffer[n]) * buffer[n];
    return std::sqrt(sum / static_cast<double>(buffer.size() - 960));
}
}

// Presence realça os agudos e deixa o grave em paz — e em 0 não faz nada.
void testPresenceBoostsHighsOnly()
{
    std::cout << "presence realca os agudos sem mexer no grave\n";

    const double highBoostDb = 20.0 * std::log10(outputRms(1.0f, 8000.0) / outputRms(0.0f, 8000.0));
    const double lowChangeDb = 20.0 * std::log10(outputRms(1.0f, 100.0) / outputRms(0.0f, 100.0));

    std::cout << "    presence no maximo: +" << highBoostDb << " dB em 8 kHz, " << lowChangeDb << " dB em 100 Hz\n";

    check(highBoostDb > 6.0, "mais de 6 dB a mais em 8 kHz");
    check(std::fabs(lowChangeDb) < 0.5, "100 Hz praticamente igual (+-0.5 dB)");
}

void testPresenceDefaultsToNeutral()
{
    std::cout << "presence padrao e 0 (neutro)\n";

    PowerAmp powerAmp;
    checkClose(powerAmp.presence(), 0.0f, "padrao 0");
    powerAmp.setPresence(0.7f);
    checkClose(powerAmp.presence(), 0.7f, "vira 0.7");
}

int main()
{
    std::cout << "\n=== testes do PowerAmp ===\n\n";

    testSilenceStaysSilent();
    testOutputNeverLeavesRange();
    testZeroSagIsPlainTanh();
    testLoudPassageLeavesMemoryOnQuietNote();
    testHigherSagAmplifiesTheMemoryEffect();
    testResetClearsTheSagMemory();
    testParameterRoundTrip();
    testEmptyBufferDoesNotCrash();

    testPresenceBoostsHighsOnly();
    testPresenceDefaultsToNeutral();

    return reportResults();
}
