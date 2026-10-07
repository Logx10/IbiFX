// Testes do Cabinet.
//
// Grava uma IR de verdade num diretório temporário, pelo mesmo motivo do
// test_wav_file.cpp: exercitar o carregamento de arquivo real, não só a
// matemática em memória (essa já está coberta em test_convolution_engine.cpp
// e test_ir_loader.cpp).
//
// A infra de verificação vive em test_helpers.h.

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "Cabinet.h"
#include "WavFile.h"
#include "test_helpers.h"

namespace
{
std::filesystem::path tempPath(const std::string& name)
{
    return std::filesystem::temp_directory_path() / ("ibifx_test_cabinet_" + name);
}

void removeIfExists(const std::filesystem::path& path)
{
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

// Uma IR de dois taps: metade da amostra atual, metade da anterior —
// simples o bastante pra calcular a saída esperada à mão.
std::filesystem::path writeSimpleImpulseResponse()
{
    const std::filesystem::path path = tempPath("ir.wav");

    WavFile file;
    file.sampleRate = 44100.0;
    file.channels = {{0.5f, 0.5f}};

    wav::write(path.string(), file);

    return path;
}
}

// ---------------------------------------------------------------------

// Sem nenhuma IR carregada, o Cabinet é passthrough — não importa o mix,
// porque seco e molhado são a mesma coisa (ConvolutionEngine sem IR devolve
// a entrada intocada).
void testWithoutImpulseResponseIsPassthrough()
{
    std::cout << "sem IR carregada, e passthrough\n";

    std::vector<float> buffer = {0.5f, -0.3f, 0.8f};

    Cabinet cabinet;
    cabinet.process(buffer);

    checkClose(buffer[0], 0.5f, "0.5 continua 0.5");
    checkClose(buffer[1], -0.3f, "-0.3 continua -0.3");
    checkClose(buffer[2], 0.8f, "0.8 continua 0.8");
}

// Com mix 0.0, a saída é sempre o sinal seco, mesmo com uma IR carregada —
// "0% de cabinet" precisa significar 0% mesmo.
void testZeroMixIsFullyDry()
{
    std::cout << "mix 0.0 e completamente seco\n";

    const std::filesystem::path path = writeSimpleImpulseResponse();

    std::vector<float> buffer = {1.0f, 0.5f, -0.5f};
    const std::vector<float> original = buffer;

    Cabinet cabinet;
    cabinet.loadImpulseResponseFile(path.string());
    cabinet.setMix(0.0f);
    cabinet.process(buffer);

    for (std::size_t i = 0; i < buffer.size(); ++i)
    {
        checkClose(buffer[i], original[i], "amostra continua igual a entrada com mix 0.0");
    }

    removeIfExists(path);
}

// Com mix 1.0 (o padrão), a saída é a convolução pura — calculada à mão
// pra uma IR de dois taps {0.5, 0.5} conhecida.
void testFullMixIsPureConvolution()
{
    std::cout << "mix 1.0 e a convolucao pura\n";

    const std::filesystem::path path = writeSimpleImpulseResponse();

    std::vector<float> buffer = {1.0f, 1.0f, 0.0f};

    Cabinet cabinet;
    cabinet.loadImpulseResponseFile(path.string());
    // mix padrao ja e 1.0, nao precisa setar

    cabinet.process(buffer);

    // y[n] = 0.5*x[n] + 0.5*x[n-1]
    checkClose(buffer[0], 0.5f, "y[0] = 0.5*1.0 + 0.5*0 (sem historico ainda)");
    checkClose(buffer[1], 1.0f, "y[1] = 0.5*1.0 + 0.5*1.0");
    checkClose(buffer[2], 0.5f, "y[2] = 0.5*0.0 + 0.5*1.0");

    removeIfExists(path);
}

// Os getters devolvem o que os setters guardaram.
void testParameterRoundTrip()
{
    std::cout << "setter e getter\n";

    Cabinet cabinet;

    checkClose(cabinet.mix(), 1.0f, "mix padrao e 1.0");

    cabinet.setMix(0.4f);
    checkClose(cabinet.mix(), 0.4f, "mix vira 0.4");
}

// Buffer vazio não pode quebrar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    std::vector<float> buffer;

    Cabinet cabinet;
    cabinet.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

// Uma IR "de gabinete" sintética a 44100 Hz: um decaimento exponencial
// oscilante, longa o bastante para a conversão ter miolo.
std::filesystem::path writeDecayingImpulseResponse(std::vector<float>& taps)
{
    taps.assign(441, 0.0f);
    for (std::size_t i = 0; i < taps.size(); ++i)
        taps[i] = 0.5f * std::exp(-static_cast<float>(i) / 60.0f) * std::cos(0.15f * static_cast<float>(i));

    const std::filesystem::path path = tempPath("ir_decaimento.wav");
    WavFile file;
    file.sampleRate = 44100.0;
    file.channels = {taps};
    wav::write(path.string(), file);
    return path;
}

// Resposta em magnitude, em dB, de uma IR numa frequência — a transformada
// de Fourier avaliada num ponto só. É o que se OUVE de um cabinet: quanto
// cada frequência sai mais alta ou mais baixa.
double magnitudeDb(const std::vector<float>& ir, double frequency, double sampleRate)
{
    constexpr double kPi = 3.14159265358979323846;
    double real = 0.0;
    double imaginary = 0.0;

    for (std::size_t n = 0; n < ir.size(); ++n)
    {
        const double phase = 2.0 * kPi * frequency * static_cast<double>(n) / sampleRate;
        real += static_cast<double>(ir[n]) * std::cos(phase);
        imaginary -= static_cast<double>(ir[n]) * std::sin(phase);
    }

    return 20.0 * std::log10(std::sqrt(real * real + imaginary * imaginary));
}

// A IR que o cabinet está usando de fato: a saída para um impulso unitário.
std::vector<float> measuredImpulseResponse(Cabinet& cabinet, std::size_t length)
{
    std::vector<float> buffer(length, 0.0f);
    buffer[0] = 1.0f;
    cabinet.reset();
    cabinet.process(buffer);
    return buffer;
}

// Preparado na MESMA taxa da IR, nada muda: a saída para um impulso são os
// taps do arquivo, um a um.
void testSameRateKeepsTheIrUntouched()
{
    std::cout << "IR na mesma taxa do motor nao e convertida\n";

    std::vector<float> taps;
    const std::filesystem::path path = writeDecayingImpulseResponse(taps);

    Cabinet cabinet;
    cabinet.prepare(44100.0, 128);
    cabinet.loadImpulseResponseFile(path.string());

    std::vector<float> buffer(taps.size(), 0.0f);
    buffer[0] = 1.0f;
    cabinet.process(buffer);

    float worst = 0.0f;
    for (std::size_t i = 0; i < taps.size(); ++i)
        worst = std::max(worst, std::fabs(buffer[i] - taps[i]));

    check(worst < 1e-4f, "saida = taps do arquivo (24 bits de precisao)");

    removeIfExists(path);
}

// Uma IR de 44100 Hz num motor a 48000 Hz é convertida: fica ~9% mais
// longa em amostras, e soa IGUAL — a mesma resposta em frequência, volume
// incluído (a compensação de Cabinet::applyImpulseResponse()). Sem a
// conversão, o pico de ressonância em ~1050 Hz iria parar em ~1140 Hz.
//
// Tolerância de 0.2 dB de 200 Hz a 15 kHz — bem abaixo do ~1 dB que o
// ouvido percebe. O erro cresce onde a IR é fraca: 0.11 dB em 200 Hz,
// 24 dB abaixo do pico desta IR, e ~0.3 dB nas pontas (DC e perto de
// 20 kHz): a IR começa de supetão,
// e o pedacinho de pré-eco que a conversão geraria ANTES da amostra 0 é
// cortado para não somar latência. Inaudível, e por isso fora do teste.
void testIrIsConvertedToTheEngineRate()
{
    std::cout << "IR de 44100 Hz e convertida para 48000 Hz soando igual\n";

    std::vector<float> taps;
    const std::filesystem::path path = writeDecayingImpulseResponse(taps);

    // Carregar ANTES e preparar depois — a ordem de preset::apply() seguido
    // de LiveEngine::start().
    Cabinet loadedFirst;
    loadedFirst.loadImpulseResponseFile(path.string());
    loadedFirst.prepare(48000.0, 128);

    // E o contrário — trocar de cabinet com o motor já preparado.
    Cabinet preparedFirst;
    preparedFirst.prepare(48000.0, 128);
    preparedFirst.loadImpulseResponseFile(path.string());

    for (Cabinet* cabinet : {&loadedFirst, &preparedFirst})
    {
        const std::vector<float> converted = measuredImpulseResponse(*cabinet, 1024);

        float worst = 0.0f;
        for (double frequency : {200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 15000.0})
        {
            const double difference =
                magnitudeDb(converted, frequency, 48000.0) - magnitudeDb(taps, frequency, 44100.0);
            worst = std::max(worst, static_cast<float>(std::fabs(difference)));
        }

        check(worst < 0.2f, "mesma resposta em frequencia (200 Hz a 15 kHz, +-0.2 dB)");
    }

    // A IR convertida tem 480 taps: um impulso só gera saída até ali.
    std::vector<float> buffer(600, 0.0f);
    buffer[0] = 1.0f;
    loadedFirst.reset();
    loadedFirst.process(buffer);

    float tail = 0.0f;
    for (std::size_t i = 480; i < buffer.size(); ++i)
        tail = std::max(tail, std::fabs(buffer[i]));

    check(std::fabs(buffer[470]) > 0.0f, "ainda ha IR perto do fim convertido (480)");
    check(tail == 0.0f, "e nada depois dele");

    removeIfExists(path);
}

int main()
{
    std::cout << "\n=== testes do Cabinet ===\n\n";

    testWithoutImpulseResponseIsPassthrough();
    testZeroMixIsFullyDry();
    testFullMixIsPureConvolution();
    testParameterRoundTrip();
    testEmptyBufferDoesNotCrash();
    testSameRateKeepsTheIrUntouched();
    testIrIsConvertedToTheEngineRate();

    return reportResults();
}
