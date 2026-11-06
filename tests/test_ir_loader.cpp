// Testes do IRLoader.
//
// Gravam arquivos .wav de verdade num diretório temporário, pelo mesmo
// motivo do test_wav_file.cpp: é a ida ao disco que precisa ser verificada.
//
// A infra de verificação vive em test_helpers.h.

#include <algorithm>
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

// Um impulso unitário tem resposta plana em 0 dB: já está na referência, e
// a normalização não muda nada.
void testUnitImpulseIsAlreadyNormalized()
{
    std::cout << "impulso unitario ja esta normalizado\n";

    std::vector<float> ir = {1.0f, 0.0f, 0.0f, 0.0f};
    normalizeImpulseResponseLevel(ir, 48000.0);

    checkClose(ir[0], 1.0f, "tap 0 continua 1.0");
    checkClose(ir[1], 0.0f, "tap 1 continua 0.0");
}

// A mesma IR gravada 20 dB mais alta ou 20 dB mais baixa sai igual — é o
// caso das IRs comerciais, normalizadas pelo pico: o volume do arquivo
// deixa de importar, só a forma da resposta.
void testLevelOfTheFileNoLongerMatters()
{
    std::cout << "o volume do arquivo nao importa mais\n";

    std::vector<float> loud(400);
    for (std::size_t i = 0; i < loud.size(); ++i)
        loud[i] = std::exp(-static_cast<float>(i) / 50.0f) * std::cos(0.05f * static_cast<float>(i));

    std::vector<float> quiet = loud;
    for (float& tap : quiet)
        tap *= 0.01f;

    normalizeImpulseResponseLevel(loud, 48000.0);
    normalizeImpulseResponseLevel(quiet, 48000.0);

    float worst = 0.0f;
    for (std::size_t i = 0; i < loud.size(); ++i)
        worst = std::max(worst, std::fabs(loud[i] - quiet[i]));

    check(worst < 1e-5f, "IR alta e IR 40 dB mais baixa normalizam para os mesmos taps");
}

// Uma IR de silêncio não tem o que medir: fica como está, sem dividir por
// zero.
void testSilentIrStaysSilent()
{
    std::cout << "IR de silencio continua silencio\n";

    std::vector<float> ir(64, 0.0f);
    normalizeImpulseResponseLevel(ir, 48000.0);

    check(std::isfinite(ir[0]), "tap e numero finito, nao NaN");
    checkClose(ir[0], 0.0f, "0.0 continua 0.0");
}

int main()
{
    std::cout << "\n=== testes do IRLoader ===\n\n";

    testMonoImpulseResponseRoundTrip();
    testStereoImpulseResponseIsAveragedToMono();
    testMissingFileThrows();
    testUnitImpulseIsAlreadyNormalized();
    testLevelOfTheFileNoLongerMatters();
    testSilentIrStaysSilent();

    return reportResults();
}
