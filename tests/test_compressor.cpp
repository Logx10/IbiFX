// Testes do Compressor.
//
// Sample rate de 1000 Hz, pelo mesmo motivo do test_noise_gate.cpp: 1
// amostra = 1 ms, o que torna previsível quantas amostras o envelope leva
// pra convergir, e dá pra calcular o valor esperado à mão.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <cmath>
#include <iostream>
#include <vector>

#include "Compressor.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------

// Sinal constante e bem abaixo do threshold nunca é comprimido, nem depois
// do envelope convergir — o compressor não deveria colorir o que já está
// dentro da faixa normal.
void testSignalBelowThresholdIsNotCompressed()
{
    std::cout << "sinal abaixo do threshold nao e comprimido\n";

    // -20 dBFS (padrão) ~= amplitude 0.1. 0.05 fica bem abaixo disso.
    std::vector<float> buffer(500, 0.05f);

    Compressor compressor;
    compressor.prepare(1000.0, 64);
    compressor.process(buffer);

    checkClose(buffer.back(), 0.05f, "depois de convergir, amostra continua 0.05");
}

// Ratio 1.0 é a definição de "sem compressão": para cada dB que passa do
// threshold, a saída sobe o mesmo 1 dB — ou seja, nada muda.
void testRatioOneMeansNoCompression()
{
    std::cout << "ratio 1.0 nao comprime\n";

    std::vector<float> buffer(500, 0.8f);  // bem acima do threshold padrao

    Compressor compressor;
    compressor.setRatio(1.0f);
    compressor.prepare(1000.0, 64);
    compressor.process(buffer);

    checkClose(buffer.back(), 0.8f, "com ratio 1.0, amostra continua 0.8 mesmo acima do threshold");
}

// O valor exato da redução de ganho, calculado à mão com a mesma fórmula do
// código: threshold -20 dB, ratio 4:1, entrada sustentada em 0.5 (-6.02 dB).
void testKnownCompressionValue()
{
    std::cout << "valor exato de compressao (threshold -20dB, ratio 4:1, entrada 0.5)\n";

    std::vector<float> buffer(500, 0.5f);

    Compressor compressor;
    compressor.prepare(1000.0, 64);  // threshold -20, ratio 4.0 sao o padrao
    compressor.process(buffer);

    const float envelopeDb = 20.0f * std::log10(0.5f);
    const float excessDb = envelopeDb - (-20.0f);
    const float reductionDb = excessDb - excessDb / 4.0f;
    const float esperado = 0.5f * std::pow(10.0f, -reductionDb / 20.0f);

    check(std::fabs(buffer.back() - esperado) < 1e-4f, "amostra final bate com a formula calculada a mao");
}

// Ratio maior comprime mais, para o mesmo sinal — prova de que o parâmetro
// realmente afeta o resultado, e na direção certa.
void testHigherRatioCompressesMore()
{
    std::cout << "ratio maior comprime mais\n";

    std::vector<float> suave(500, 0.8f);
    std::vector<float> forte(500, 0.8f);

    Compressor compressorSuave;
    compressorSuave.setRatio(2.0f);
    compressorSuave.prepare(1000.0, 64);
    compressorSuave.process(suave);

    Compressor compressorForte;
    compressorForte.setRatio(10.0f);
    compressorForte.prepare(1000.0, 64);
    compressorForte.process(forte);

    check(forte.back() < suave.back(), "ratio 10:1 reduz mais que ratio 2:1, para o mesmo sinal");
}

// Silêncio continua silêncio — sem threshold negativo estranho fazendo
// log(0) explodir em NaN ou infinito.
void testSilenceStaysSilent()
{
    std::cout << "silencio continua silencio, sem NaN\n";

    std::vector<float> buffer(10, 0.0f);

    Compressor compressor;
    compressor.prepare(1000.0, 64);
    compressor.process(buffer);

    for (float sample : buffer)
    {
        check(std::isfinite(sample), "amostra e um numero finito, nao NaN/infinito");
        checkClose(sample, 0.0f, "0.0 continua 0.0");
    }
}

// Os getters devolvem o que os setters guardaram.
void testParameterRoundTrip()
{
    std::cout << "setters e getters\n";

    Compressor compressor;

    checkClose(compressor.threshold(), -20.0f, "threshold padrao e -20 dB");
    checkClose(compressor.ratio(), 4.0f, "ratio padrao e 4.0");
    checkClose(compressor.attack(), 0.01f, "attack padrao e 0.01s");
    checkClose(compressor.release(), 0.15f, "release padrao e 0.15s");

    compressor.setThreshold(-30.0f);
    compressor.setRatio(8.0f);
    compressor.setAttack(0.005f);
    compressor.setRelease(0.3f);

    checkClose(compressor.threshold(), -30.0f, "threshold vira -30");
    checkClose(compressor.ratio(), 8.0f, "ratio vira 8.0");
    checkClose(compressor.attack(), 0.005f, "attack vira 0.005");
    checkClose(compressor.release(), 0.3f, "release vira 0.3");
}

// Buffer vazio não pode quebrar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    std::vector<float> buffer;

    Compressor compressor;
    compressor.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

int main()
{
    std::cout << "\n=== testes do Compressor ===\n\n";

    testSignalBelowThresholdIsNotCompressed();
    testRatioOneMeansNoCompression();
    testKnownCompressionValue();
    testHigherRatioCompressesMore();
    testSilenceStaysSilent();
    testParameterRoundTrip();
    testEmptyBufferDoesNotCrash();

    return reportResults();
}
