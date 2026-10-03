// Testes do BackingTrackPlayer — Fase 16.
//
// Grava arquivos de verdade num diretório temporário, pelo mesmo motivo de
// test_cabinet.cpp e test_wav_file.cpp: exercitar a leitura de arquivo
// real, não só a mistura em memória.
//
// A infra de verificação vive em test_helpers.h.

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "BackingTrackPlayer.h"
#include "WavFile.h"
#include "test_helpers.h"

namespace
{
// PCM de 16 bits tem passo de 1/32768, cerca de 3e-5. A tolerância padrão
// de checkClose (1e-6) é apertada demais para uma ida e volta pelo disco —
// o erro esperado não é bug, é a resolução do formato (mesmo raciocínio de
// test_wav_file.cpp).
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
    return std::filesystem::temp_directory_path() / ("ibifx_test_backing_" + name);
}

void removeIfExists(const std::filesystem::path& path)
{
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

std::filesystem::path writeMonoFile(const std::string& name, double sampleRate,
                                     const std::vector<float>& samples)
{
    const std::filesystem::path path = tempPath(name);

    WavFile file;
    file.sampleRate = sampleRate;
    file.channels = {samples};

    wav::write(path.string(), file);
    return path;
}
}

// ---------------------------------------------------------------------

// Sem nada carregado, process() não altera o buffer.
void testUnloadedPlayerIsSilent()
{
    std::cout << "sem arquivo carregado, fica em silencio\n";

    BackingTrackPlayer player;
    player.prepare(1000.0);

    std::vector<float> buffer = {0.3f, 0.3f, 0.3f};
    player.process(buffer, 0);

    checkClose(buffer[0], 0.3f, "amostra 0 intocada");
    checkClose(buffer[1], 0.3f, "amostra 1 intocada");
    checkClose(buffer[2], 0.3f, "amostra 2 intocada");
    check(!player.isLoaded(), "isLoaded() e falso");
}

// Carregar preenche isLoaded(), lengthSamples() e lengthSeconds()
// corretamente.
void testLoadReportsLength()
{
    std::cout << "carregar relata o tamanho certo\n";

    const auto path = writeMonoFile("length.wav", 1000.0, {1.0f, 2.0f, 3.0f, 4.0f, 5.0f});

    BackingTrackPlayer player;
    player.prepare(1000.0);
    player.load(path.string());

    check(player.isLoaded(), "isLoaded() e verdadeiro");
    check(player.lengthSamples() == 5, "5 amostras");
    checkClose(static_cast<float>(player.lengthSeconds()), 0.005f, "5 amostras a 1000 Hz sao 5 ms");

    removeIfExists(path);
}

// process() soma a amostra certa de cada posição — e SOMA, não substitui.
void testProcessAddsCorrectSamples()
{
    std::cout << "process soma a amostra certa, sem substituir\n";

    const auto path = writeMonoFile("samples.wav", 1000.0, {0.1f, 0.2f, 0.3f, 0.4f});

    BackingTrackPlayer player;
    player.prepare(1000.0);
    player.load(path.string());

    std::vector<float> buffer = {1.0f, 1.0f, 1.0f, 1.0f};
    player.process(buffer, 0);

    checkCloseWav(buffer[0], 1.1f, "1.0 + 0.1");
    checkCloseWav(buffer[1], 1.2f, "1.0 + 0.2");
    checkCloseWav(buffer[2], 1.3f, "1.0 + 0.3");
    checkCloseWav(buffer[3], 1.4f, "1.0 + 0.4");

    removeIfExists(path);
}

// Pedir um trecho que começa no meio do arquivo começa exatamente dali —
// mesma ideia de posição absoluta do Metronome.
void testProcessStartsAtGivenPosition()
{
    std::cout << "process comeca na posicao pedida, nao do zero\n";

    const auto path = writeMonoFile("offset.wav", 1000.0, {0.1f, 0.2f, 0.3f, 0.4f, 0.5f});

    BackingTrackPlayer player;
    player.prepare(1000.0);
    player.load(path.string());

    std::vector<float> buffer = {0.0f, 0.0f};
    player.process(buffer, 3);   // pede as amostras 3 e 4 do arquivo

    checkCloseWav(buffer[0], 0.4f, "amostra 3 do arquivo");
    checkCloseWav(buffer[1], 0.5f, "amostra 4 do arquivo");

    removeIfExists(path);
}

// Depois do fim do arquivo, sem loop nenhum, fica em silêncio — não
// trava, não repete a última amostra.
void testProcessPastEndIsSilent()
{
    std::cout << "depois do fim do arquivo, silencio\n";

    const auto path = writeMonoFile("short.wav", 1000.0, {0.5f, 0.5f});

    BackingTrackPlayer player;
    player.prepare(1000.0);
    player.load(path.string());

    std::vector<float> buffer = {0.2f, 0.2f, 0.2f, 0.2f};
    player.process(buffer, 0);   // arquivo tem só 2 amostras, buffer pede 4

    checkCloseWav(buffer[0], 0.7f, "amostra 0 do arquivo somada");
    checkCloseWav(buffer[1], 0.7f, "amostra 1 do arquivo somada");
    checkClose(buffer[2], 0.2f, "depois do fim: intocado (silencio)");
    checkClose(buffer[3], 0.2f, "depois do fim: intocado (silencio)");

    removeIfExists(path);
}

// Estéreo é reduzido à média dos canais, mesma convenção de IRLoader.
void testStereoFileIsAveragedToMono()
{
    std::cout << "arquivo estereo vira mono pela media\n";

    const std::filesystem::path path = tempPath("stereo.wav");

    WavFile file;
    file.sampleRate = 1000.0;
    file.channels = {{1.0f, 0.0f}, {0.0f, 1.0f}};
    wav::write(path.string(), file);

    BackingTrackPlayer player;
    player.prepare(1000.0);
    player.load(path.string());

    std::vector<float> buffer = {0.0f, 0.0f};
    player.process(buffer, 0);

    checkCloseWav(buffer[0], 0.5f, "(1.0 + 0.0) / 2");
    checkCloseWav(buffer[1], 0.5f, "(0.0 + 1.0) / 2");

    removeIfExists(path);
}

// Volume escala a amplitude.
void testVolumeScalesAmplitude()
{
    std::cout << "volume escala a amplitude\n";

    const auto path = writeMonoFile("volume.wav", 1000.0, {1.0f, 1.0f});

    BackingTrackPlayer player;
    player.prepare(1000.0);
    player.load(path.string());
    player.setVolume(0.25f);

    std::vector<float> buffer = {0.0f, 0.0f};
    player.process(buffer, 0);

    checkCloseWav(buffer[0], 0.25f, "volume 0.25 aplicado");
    checkClose(player.volume(), 0.25f, "getter reflete o setter");

    removeIfExists(path);
}

// Sample rate do arquivo diferente do motor lança, em vez de tocar
// silenciosamente em pitch errado.
void testMismatchedSampleRateThrows()
{
    std::cout << "sample rate incompativel lanca\n";

    const auto path = writeMonoFile("outrarate.wav", 44100.0, {0.1f, 0.2f});

    BackingTrackPlayer player;
    player.prepare(48000.0);

    bool lancou = false;
    try
    {
        player.load(path.string());
    }
    catch (const std::runtime_error&)
    {
        lancou = true;
    }

    check(lancou, "sample rate incompativel lanca runtime_error");
    check(!player.isLoaded(), "nada foi carregado");

    removeIfExists(path);
}

// Arquivo inexistente lança com a mensagem de wav::read, sem precisar de
// tratamento próprio.
void testMissingFileThrows()
{
    std::cout << "arquivo inexistente lanca\n";

    BackingTrackPlayer player;
    player.prepare(48000.0);

    bool lancou = false;
    try
    {
        player.load("/definitivamente/nao/existe/backing.wav");
    }
    catch (const std::runtime_error&)
    {
        lancou = true;
    }

    check(lancou, "arquivo inexistente lanca");
}

int main()
{
    std::cout << "\n=== testes do BackingTrackPlayer ===\n\n";

    testUnloadedPlayerIsSilent();
    testLoadReportsLength();
    testProcessAddsCorrectSamples();
    testProcessStartsAtGivenPosition();
    testProcessPastEndIsSilent();
    testStereoFileIsAveragedToMono();
    testVolumeScalesAmplitude();
    testMismatchedSampleRateThrows();
    testMissingFileThrows();

    return reportResults();
}
