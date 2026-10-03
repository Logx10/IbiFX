// Testes do Recorder — Fase 17.
//
// Grava arquivos de verdade num diretório temporário, como test_wav_file.cpp
// e test_cabinet.cpp. Um dos testes usa uma THREAD DE VERDADE empurrando
// amostras, pelo mesmo motivo de test_command_queue.cpp: testar o buffer
// circular numa thread só não prova que a concorrência de verdade funciona.
//
// A infra de verificação vive em test_helpers.h.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "Recorder.h"
#include "WavFile.h"
#include "test_helpers.h"

namespace
{
// Mesma tolerância de test_wav_file.cpp/test_backing_track_player.cpp: a
// ida e volta por PCM de 16 bits tem passo de ~3e-5, maior que a
// tolerância padrão de checkClose.
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
    return std::filesystem::temp_directory_path() / ("ibifx_test_recorder_" + name);
}

void removeIfExists(const std::filesystem::path& path)
{
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}
}

// ---------------------------------------------------------------------

// isRecording() reflete start()/stop().
void testIsRecordingReflectsState()
{
    std::cout << "isRecording reflete start/stop\n";

    const auto path = tempPath("estado.wav");

    Recorder recorder;
    recorder.prepare(1000.0);

    check(!recorder.isRecording(), "nao esta gravando antes do start");

    recorder.start(path.string());
    check(recorder.isRecording(), "esta gravando depois do start");

    recorder.stop();
    check(!recorder.isRecording(), "nao esta gravando depois do stop");

    removeIfExists(path);
}

// O arquivo gravado contém exatamente as amostras empurradas, na ordem.
void testRecordedFileMatchesPushedSamples()
{
    std::cout << "arquivo gravado bate com as amostras empurradas\n";

    const auto path = tempPath("basico.wav");

    Recorder recorder;
    recorder.prepare(1000.0);
    recorder.start(path.string());

    const std::vector<float> samples = {0.1f, 0.2f, -0.3f, 0.4f, -0.5f};
    recorder.pushSamples(samples.data(), samples.size());

    recorder.stop();

    const WavFile result = wav::read(path.string());
    check(result.frameCount() == samples.size(), "mesma quantidade de amostras");

    for (std::size_t i = 0; i < samples.size(); ++i)
        checkCloseWav(result.channels[0][i], samples[i], "amostra " + std::to_string(i));

    removeIfExists(path);
}

// start() chamado duas vezes sem stop() no meio lança.
void testStartWhileRecordingThrows()
{
    std::cout << "start() duas vezes seguidas lanca\n";

    const auto path = tempPath("duplo.wav");

    Recorder recorder;
    recorder.prepare(1000.0);
    recorder.start(path.string());

    bool lancou = false;
    try
    {
        recorder.start(path.string());
    }
    catch (const std::runtime_error&)
    {
        lancou = true;
    }

    check(lancou, "segundo start() lanca");

    recorder.stop();
    removeIfExists(path);
}

// pushSamples() antes de start() (ou depois de stop()) é um no-op seguro,
// não crasha nem escreve nada de errado.
void testPushBeforeStartIsNoop()
{
    std::cout << "pushSamples antes do start nao faz nada\n";

    Recorder recorder;
    recorder.prepare(1000.0);

    const std::vector<float> samples = {1.0f, 1.0f};
    recorder.pushSamples(samples.data(), samples.size());   // nao deve crashar

    check(recorder.recordedSampleCount() == 0, "nada foi gravado sem um start()");
}

// Destruir um Recorder ainda gravando (sem chamar stop() explicitamente)
// ainda assim escreve o arquivo — o destrutor é a rede de segurança.
void testDestructorStopsAndWritesFile()
{
    std::cout << "destrutor para a gravacao e escreve o arquivo\n";

    const auto path = tempPath("destrutor.wav");

    {
        Recorder recorder;
        recorder.prepare(1000.0);
        recorder.start(path.string());

        const std::vector<float> samples = {0.25f, 0.5f};
        recorder.pushSamples(samples.data(), samples.size());

        // Sem chamar stop() — o destrutor precisa fazer isso sozinho.
    }

    check(std::filesystem::exists(path), "arquivo foi escrito mesmo sem stop() explicito");

    const WavFile result = wav::read(path.string());
    check(result.frameCount() == 2, "as 2 amostras empurradas foram gravadas");

    removeIfExists(path);
}

// Uma thread de verdade empurrando muitas amostras pequenas, enquanto a
// thread de disco do próprio Recorder drena em paralelo — prova que o
// buffer circular não perde nem embaralha amostras sob concorrência real,
// não só numa simulação de thread única.
void testConcurrentProducerPreservesOrder()
{
    std::cout << "produtor concorrente preserva a ordem das amostras\n";

    const auto path = tempPath("concorrencia.wav");
    constexpr std::size_t kTotalSamples = 20000;

    // O buffer é maior que o total de amostras de propósito: o objetivo
    // deste teste é provar que o buffer circular não perde nem embaralha
    // amostras sob concorrência real (threads de verdade, escalonadas pelo
    // sistema operacional) — não estressar o comportamento de overflow
    // (que é outro comportamento, já coberto pelo próprio desenho do
    // algoritmo, idêntico ao da CommandQueue). Um buffer pequeno de
    // propósito mediria principalmente a folga de agendamento desta
    // máquina no momento do teste, não a corretude do código.
    Recorder recorder(kTotalSamples * 2);
    recorder.prepare(1000.0);
    recorder.start(path.string());

    // Uma rampa em dente de serra, sempre dentro de [-1, 1) — PCM de 16
    // bits satura fora dessa faixa, então valores grandes (um contador
    // cru 0, 1, 2...) não sobreviveriam à gravação. O período de 1000
    // amostras é curto o bastante para repetir várias vezes dentro dos
    // 20000 totais, então qualquer perda, duplicação ou embaralhamento no
    // meio do fluxo desalinha a fase e é pego pela checagem abaixo.
    const auto expectedValue = [](std::size_t i)
    {
        return (static_cast<float>(i % 1000) - 500.0f) / 500.0f;
    };

    std::thread producer([&recorder, &expectedValue]()
    {
        for (std::size_t i = 0; i < kTotalSamples; ++i)
        {
            const float value = expectedValue(i);
            recorder.pushSamples(&value, 1);

            if (i % 500 == 0)
                std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    producer.join();
    recorder.stop();

    const WavFile result = wav::read(path.string());

    check(result.frameCount() == kTotalSamples, "nenhuma amostra perdida sob concorrencia real");

    // Defensivo de propósito: se o check acima falhar por algum motivo
    // inesperado, continuamos só dentro do que de fato foi gravado, em vez
    // de indexar além do arquivo e mascarar um FALHOU com um crash.
    const std::size_t safeLimit = std::min<std::size_t>(kTotalSamples, result.frameCount());

    bool emOrdem = true;
    for (std::size_t i = 0; i < safeLimit; i += 37)   // amostra o arquivo inteiro, nao so o inicio
    {
        if (std::fabs(result.channels[0][i] - expectedValue(i)) > kPcm16Tolerance)
        {
            emOrdem = false;
            break;
        }
    }
    check(emOrdem, "as amostras chegaram na ordem certa do inicio ao fim, sem desalinhar a fase");

    removeIfExists(path);
}

int main()
{
    std::cout << "\n=== testes do Recorder ===\n\n";

    testIsRecordingReflectsState();
    testRecordedFileMatchesPushedSamples();
    testStartWhileRecordingThrows();
    testPushBeforeStartIsNoop();
    testDestructorStopsAndWritesFile();
    testConcurrentProducerPreservesOrder();

    return reportResults();
}
