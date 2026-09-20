// Testes do IRLoader.
//
// Gravam arquivos .wav de verdade num diretório temporário, pelo mesmo
// motivo do test_wav_file.cpp: é a ida ao disco que precisa ser verificada.
//
// A infra de verificação vive em test_helpers.h.

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "IRLoader.h"
#include "WavFile.h"
#include "test_helpers.h"

namespace
{
// PCM de 16 bits tem passo de 1/32768 — mesma tolerância alargada do
// test_wav_file.cpp, pela mesma razão: o erro esperado é a resolução do
// formato, não um bug.
constexpr float kPcm16Tolerance = 1e-4f;

void checkCloseWav(float actual, float expected, const std::string& description)
{
    if (std::fabs(actual - expected) <= kPcm16Tolerance)
    {
        std::cout << "  ok      " << description << "\n";
    }
    else
    {
        std::cout << "  FALHOU  " << description
                  << "  (esperado " << expected << ", obtido " << actual << ")\n";
        ++failures;
    }
}

std::filesystem::path tempPath(const std::string& name)
{
    return std::filesystem::temp_directory_path() / ("ibifx_test_ir_" + name);
}

void removeIfExists(const std::filesystem::path& path)
{
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}
}

// ---------------------------------------------------------------------

// Uma IR mono volta exatamente como foi gravada, dentro da resolução de
// 16 bits.
void testMonoImpulseResponseRoundTrip()
{
    std::cout << "IR mono volta como foi gravada\n";

    const std::filesystem::path path = tempPath("mono.wav");

    WavFile file;
    file.sampleRate = 44100.0;
    file.channels = {{1.0f, 0.5f, -0.5f, 0.25f, 0.0f}};

    wav::write(path.string(), file);

    const std::vector<float> ir = loadImpulseResponse(path.string());

    check(ir.size() == 5, "carregou 5 amostras");
    checkCloseWav(ir[0], 1.0f, "amostra 0");
    checkCloseWav(ir[1], 0.5f, "amostra 1");
    checkCloseWav(ir[2], -0.5f, "amostra 2");
    checkCloseWav(ir[3], 0.25f, "amostra 3");
    checkCloseWav(ir[4], 0.0f, "amostra 4");

    removeIfExists(path);
}

// Uma IR estéreo é reduzida a mono pela MÉDIA dos dois canais.
void testStereoImpulseResponseIsAveragedToMono()
{
    std::cout << "IR estereo vira mono pela media dos canais\n";

    const std::filesystem::path path = tempPath("stereo.wav");

    WavFile file;
    file.sampleRate = 44100.0;
    file.channels = {
        {1.0f, 0.0f, -1.0f},   // esquerdo
        {0.0f, 1.0f, -1.0f},   // direito
    };

    wav::write(path.string(), file);

    const std::vector<float> ir = loadImpulseResponse(path.string());

    check(ir.size() == 3, "carregou 3 amostras");
    checkCloseWav(ir[0], 0.5f, "amostra 0: media de 1.0 e 0.0");
    checkCloseWav(ir[1], 0.5f, "amostra 1: media de 0.0 e 1.0");
    checkCloseWav(ir[2], -1.0f, "amostra 2: media de -1.0 e -1.0");

    removeIfExists(path);
}

// Um arquivo que não existe lança, com mensagem — não devolve uma IR vazia
// silenciosamente.
void testMissingFileThrows()
{
    std::cout << "arquivo inexistente lanca\n";

    bool lancou = false;

    try
    {
        loadImpulseResponse(tempPath("nao_existe.wav").string());
    }
    catch (const std::exception&)
    {
        lancou = true;
    }

    check(lancou, "loadImpulseResponse lancou uma excecao");
}

int main()
{
    std::cout << "\n=== testes do IRLoader ===\n\n";

    testMonoImpulseResponseRoundTrip();
    testStereoImpulseResponseIsAveragedToMono();
    testMissingFileThrows();

    return reportResults();
}
