// Testes do Cabinet.
//
// Grava uma IR de verdade num diretório temporário, pelo mesmo motivo do
// test_wav_file.cpp: exercitar o carregamento de arquivo real, não só a
// matemática em memória (essa já está coberta em test_convolution_engine.cpp
// e test_ir_loader.cpp).
//
// A infra de verificação vive em test_helpers.h.

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

int main()
{
    std::cout << "\n=== testes do Cabinet ===\n\n";

    testWithoutImpulseResponseIsPassthrough();
    testZeroMixIsFullyDry();
    testFullMixIsPureConvolution();
    testParameterRoundTrip();
    testEmptyBufferDoesNotCrash();

    return reportResults();
}
