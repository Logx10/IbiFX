// Testes do ReampRecorder — Fase 18 (DI + sinal processado em tracks
// separadas).
//
// A infra de verificação vive em test_helpers.h.

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ReampRecorder.h"
#include "WavFile.h"
#include "test_helpers.h"

namespace
{
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
    return std::filesystem::temp_directory_path() / ("ibifx_test_reamp_" + name);
}

void removeIfExists(const std::filesystem::path& path)
{
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}
}

// ---------------------------------------------------------------------

// isRecording() reflete start()/stop(), igual ao Recorder de uma track só.
void testIsRecordingReflectsState()
{
    std::cout << "isRecording reflete start/stop\n";

    const auto dryPath = tempPath("estado_seco.wav");
    const auto wetPath = tempPath("estado_processado.wav");

    ReampRecorder recorder;
    recorder.prepare(1000.0);

    check(!recorder.isRecording(), "nao esta gravando antes do start");

    recorder.start(dryPath.string(), wetPath.string());
    check(recorder.isRecording(), "esta gravando depois do start");

    recorder.stop();
    check(!recorder.isRecording(), "nao esta gravando depois do stop");

    removeIfExists(dryPath);
    removeIfExists(wetPath);
}

// As duas tracks gravam sinais DIFERENTES de verdade — a seca e a
// processada não são a mesma coisa duplicada, cada uma recebe o que
// pushBlock() mandou para ela especificamente.
void testTracksCaptureDistinctSignals()
{
    std::cout << "as duas tracks gravam sinais diferentes\n";

    const auto dryPath = tempPath("seco.wav");
    const auto wetPath = tempPath("processado.wav");

    ReampRecorder recorder;
    recorder.prepare(1000.0);
    recorder.start(dryPath.string(), wetPath.string());

    // Simula o que LiveEngine::processBlock() faria: um sinal de entrada
    // (seco) e o MESMO sinal já com ganho 4x aplicado (processado) — como
    // se tivesse passado por um GainProcessor antes de chegar aqui.
    const std::vector<float> dry = {0.1f, 0.1f, 0.1f};
    const std::vector<float> wet = {0.4f, 0.4f, 0.4f};

    recorder.pushBlock(dry.data(), wet.data(), dry.size());
    recorder.stop();

    const WavFile dryFile = wav::read(dryPath.string());
    const WavFile wetFile = wav::read(wetPath.string());

    check(dryFile.frameCount() == 3, "track seca tem as 3 amostras");
    check(wetFile.frameCount() == 3, "track processada tem as 3 amostras");

    checkCloseWav(dryFile.channels[0][0], 0.1f, "seca preserva o sinal original");
    checkCloseWav(wetFile.channels[0][0], 0.4f, "processada preserva o sinal com ganho");

    removeIfExists(dryPath);
    removeIfExists(wetPath);
}

// As duas tracks ficam do MESMO tamanho mesmo em vários blocos — é a
// sincronia que torna possível reamplificar depois amostra a amostra.
void testTracksStaySynchronizedAcrossBlocks()
{
    std::cout << "as duas tracks ficam sincronizadas em varios blocos\n";

    const auto dryPath = tempPath("sync_seco.wav");
    const auto wetPath = tempPath("sync_processado.wav");

    ReampRecorder recorder;
    recorder.prepare(1000.0);
    recorder.start(dryPath.string(), wetPath.string());

    for (int block = 0; block < 10; ++block)
    {
        std::vector<float> dry(16, static_cast<float>(block) * 0.01f);
        std::vector<float> wet(16, static_cast<float>(block) * 0.02f);
        recorder.pushBlock(dry.data(), wet.data(), dry.size());
    }

    recorder.stop();

    const WavFile dryFile = wav::read(dryPath.string());
    const WavFile wetFile = wav::read(wetPath.string());

    check(dryFile.frameCount() == 160, "160 amostras na track seca (10 blocos de 16)");
    check(wetFile.frameCount() == 160, "160 amostras na track processada, igual a seca");

    removeIfExists(dryPath);
    removeIfExists(wetPath);
}

// start() chamado duas vezes seguidas lança, mesma regra do Recorder.
void testStartWhileRecordingThrows()
{
    std::cout << "start() duas vezes seguidas lanca\n";

    const auto dryPath = tempPath("duplo_seco.wav");
    const auto wetPath = tempPath("duplo_processado.wav");

    ReampRecorder recorder;
    recorder.prepare(1000.0);
    recorder.start(dryPath.string(), wetPath.string());

    bool lancou = false;
    try
    {
        recorder.start(dryPath.string(), wetPath.string());
    }
    catch (const std::runtime_error&)
    {
        lancou = true;
    }

    check(lancou, "segundo start() lanca");

    recorder.stop();
    removeIfExists(dryPath);
    removeIfExists(wetPath);
}

// pushBlock() antes de start() não faz nada, não crasha.
void testPushBeforeStartIsNoop()
{
    std::cout << "pushBlock antes do start nao faz nada\n";

    ReampRecorder recorder;
    recorder.prepare(1000.0);

    const std::vector<float> dry = {1.0f};
    const std::vector<float> wet = {1.0f};

    recorder.pushBlock(dry.data(), wet.data(), dry.size());   // nao deve crashar

    check(!recorder.isRecording(), "continua sem gravar");
}

int main()
{
    std::cout << "\n=== testes do ReampRecorder ===\n\n";

    testIsRecordingReflectsState();
    testTracksCaptureDistinctSignals();
    testTracksStaySynchronizedAcrossBlocks();
    testStartWhileRecordingThrows();
    testPushBeforeStartIsNoop();

    return reportResults();
}
