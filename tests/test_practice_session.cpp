// Testes do PracticeSession — Fase 19 (backing track + metrônomo + loop +
// gravação integrados).
//
// Esta classe não tem lógica de DSP própria — ela ORQUESTRA MasterTransport,
// BackingTrackPlayer, Metronome e Recorder (Fases 14 a 17), já testados
// isoladamente. Os testes aqui focam na ORQUESTRAÇÃO: a ordem certa, a
// sincronia entre as peças, e os pontos onde a integração poderia quebrar
// mesmo com cada peça correta por si só.
//
// A infra de verificação vive em test_helpers.h.

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "PracticeSession.h"
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
    return std::filesystem::temp_directory_path() / ("ibifx_test_practice_" + name);
}

void removeIfExists(const std::filesystem::path& path)
{
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

float peakAbs(const std::vector<float>& buffer)
{
    float peak = 0.0f;
    for (float sample : buffer)
        peak = std::max(peak, std::fabs(sample));
    return peak;
}
}

// ---------------------------------------------------------------------

// Estado inicial: parado, sem backing track, metrônomo ligado por padrão,
// sem gravar.
void testInitialState()
{
    std::cout << "estado inicial\n";

    PracticeSession session;
    session.prepare(1000.0);

    check(!session.isPlaying(), "nao esta tocando");
    check(!session.hasBackingTrack(), "sem backing track carregada");
    check(session.isMetronomeEnabled(), "metronomo ligado por padrao");
    check(!session.isRecording(), "nao esta gravando");
    check(session.positionSamples() == 0, "posicao comeca em 0");
}

// process() avança o relógio interno em buffer.size() amostras por
// chamada, só enquanto play() estiver ativo.
void testProcessAdvancesPositionWhilePlaying()
{
    std::cout << "process avanca a posicao enquanto toca\n";

    PracticeSession session;
    session.prepare(1000.0);

    std::vector<float> buffer(100, 0.0f);
    session.process(buffer);
    check(session.positionSamples() == 0, "parado, nao avanca");

    session.play();
    session.process(buffer);
    check(session.positionSamples() == 100, "tocando, avancou 100");

    session.process(buffer);
    check(session.positionSamples() == 200, "mais um bloco, mais 100");
}

// A backing track é somada ao sinal da guitarra (que já chega processado
// em buffer), sincronizada à posição do transport interno.
void testBackingTrackIsMixedIn()
{
    std::cout << "backing track e somada ao sinal\n";

    const auto path = tempPath("backing.wav");

    WavFile file;
    file.sampleRate = 1000.0;
    file.channels = {{0.3f, 0.3f, 0.3f, 0.3f}};
    wav::write(path.string(), file);

    PracticeSession session;
    session.prepare(1000.0);
    session.loadBackingTrack(path.string());
    check(session.hasBackingTrack(), "backing track carregada");

    session.setMetronomeEnabled(false);   // isola a backing track pra este teste
    session.play();

    std::vector<float> buffer = {0.1f, 0.1f, 0.1f, 0.1f};   // "guitarra" ja processada
    session.process(buffer);

    checkCloseWav(buffer[0], 0.4f, "guitarra (0.1) + backing track (0.3)");
    checkCloseWav(buffer[1], 0.4f, "idem, segunda amostra");

    removeIfExists(path);
}

// Desligar o metrônomo suprime o clique mesmo numa batida exata; ligado,
// o clique aparece.
void testMetronomeCanBeDisabled()
{
    std::cout << "metronomo desligado nao soa, ligado soa\n";

    PracticeSession session;
    session.prepare(48000.0);
    session.setBpm(120.0f);   // samplesPerBeat = 24000
    session.setMetronomeEnabled(false);
    session.play();

    // Beat 0 cai exatamente na amostra 0.
    std::vector<float> silenciado(50, 0.0f);
    session.process(silenciado);
    check(peakAbs(silenciado) == 0.0f, "desligado: nenhum clique, mesmo numa batida exata");

    PracticeSession outraSessao;
    outraSessao.prepare(48000.0);
    outraSessao.setBpm(120.0f);
    check(outraSessao.isMetronomeEnabled(), "ligado por padrao");
    outraSessao.play();

    std::vector<float> comClique(50, 0.0f);
    outraSessao.process(comClique);
    check(peakAbs(comClique) > 0.0f, "ligado: o clique soa na batida");
}

// O loop do transport funciona através da sessão — positionSamples() volta
// ao início ao ultrapassar o fim do loop.
void testLoopWorksThroughSession()
{
    std::cout << "loop funciona atraves da sessao\n";

    PracticeSession session;
    session.prepare(1000.0);
    session.setLoop(100, 200);
    session.setLoopEnabled(true);
    session.seek(180);
    session.play();

    std::vector<float> buffer(50, 0.0f);   // 180 + 50 = 230, passa do fim (200) em 30
    session.process(buffer);

    check(session.positionSamples() == 130, "voltou para 100 + 30 de excesso, nao direto pro 100");
}

// Gravar a sessão captura o resultado MISTURADO (guitarra + backing
// track), não o sinal seco sozinho.
void testRecordingCapturesTheMixedSignal()
{
    std::cout << "gravar a sessao captura o sinal misturado\n";

    const auto backingPath = tempPath("rec_backing.wav");
    const auto outputPath = tempPath("rec_saida.wav");

    WavFile file;
    file.sampleRate = 1000.0;
    file.channels = {{0.2f, 0.2f}};
    wav::write(backingPath.string(), file);

    PracticeSession session;
    session.prepare(1000.0);
    session.loadBackingTrack(backingPath.string());
    session.setMetronomeEnabled(false);
    session.play();

    session.startRecording(outputPath.string());
    check(session.isRecording(), "comecou a gravar");

    std::vector<float> buffer = {0.1f, 0.1f};
    session.process(buffer);

    session.stopRecording();
    check(!session.isRecording(), "parou de gravar");

    const WavFile recorded = wav::read(outputPath.string());
    check(recorded.frameCount() == 2, "2 amostras gravadas");
    checkCloseWav(recorded.channels[0][0], 0.3f, "gravou o MIX (0.1 guitarra + 0.2 backing), nao so a guitarra");

    removeIfExists(backingPath);
    removeIfExists(outputPath);
}

// setBpm()/bpm() refletem o transport interno.
void testBpmRoundTrips()
{
    std::cout << "setBpm/bpm refletem o transport interno\n";

    PracticeSession session;
    session.prepare(48000.0);
    session.setBpm(140.0f);

    checkClose(session.bpm(), 140.0f, "bpm refletido");
}

int main()
{
    std::cout << "\n=== testes do PracticeSession ===\n\n";

    testInitialState();
    testProcessAdvancesPositionWhilePlaying();
    testBackingTrackIsMixedIn();
    testMetronomeCanBeDisabled();
    testLoopWorksThroughSession();
    testRecordingCapturesTheMixedSignal();
    testBpmRoundTrips();

    return reportResults();
}
