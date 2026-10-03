// Testes do Metronome — o clique sample-accurate da Fase 15.
//
// A infra de verificação vive em test_helpers.h.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "MasterTransport.h"
#include "Metronome.h"
#include "test_helpers.h"

namespace
{
// Maior amostra em módulo dentro de um intervalo do buffer.
float peakAbs(const std::vector<float>& buffer, std::size_t from, std::size_t to)
{
    float peak = 0.0f;

    for (std::size_t i = from; i < to && i < buffer.size(); ++i)
        peak = std::max(peak, std::fabs(buffer[i]));

    return peak;
}
}

// ---------------------------------------------------------------------

// Parado, o metrônomo não soa nada, mesmo passando por cima de uma batida.
void testStoppedTransportProducesSilence()
{
    std::cout << "transport parado, metronomo fica em silencio\n";

    MasterTransport transport;
    transport.prepare(48000.0);
    transport.setBpm(120.0f);
    // sem play(): transport.isPlaying() == false

    Metronome metronome(transport);
    metronome.prepare(48000.0);

    std::vector<float> buffer(100, 0.0f);
    metronome.process(buffer, 0);   // posicao 0 e exatamente uma batida

    check(peakAbs(buffer, 0, buffer.size()) == 0.0f, "nenhuma amostra alterada");
}

// O clique começa exatamente na amostra da batida, nem uma antes.
void testClickStartsExactlyOnTheBeat()
{
    std::cout << "o clique comeca exatamente na amostra da batida\n";

    MasterTransport transport;
    transport.prepare(48000.0);
    transport.setBpm(120.0f);   // samplesPerBeat = 48000*60/120 = 24000
    transport.play();

    Metronome metronome(transport);
    metronome.prepare(48000.0);

    // A batida de indice 5 cai exatamente na amostra 120000. Um buffer de
    // 20 amostras comecando em 119995 tem essa batida na posicao 5.
    constexpr std::uint64_t beatSample = 5 * 24000;
    std::vector<float> buffer(20, 0.0f);
    metronome.process(buffer, beatSample - 5);

    check(peakAbs(buffer, 0, 5) == 0.0f, "nada soa antes da batida");
    check(peakAbs(buffer, 6, buffer.size()) > 0.01f, "o clique soa a partir da batida");
}

// Um clique iniciado perto do fim de um bloco continua no bloco seguinte —
// o mesmo espírito da cauda do Delay atravessando blocos.
void testClickContinuesAcrossBlocks()
{
    std::cout << "o clique atravessa a fronteira entre blocos\n";

    MasterTransport transport;
    transport.prepare(48000.0);
    transport.setBpm(120.0f);   // samplesPerBeat = 24000
    transport.play();

    Metronome metronome(transport);
    metronome.prepare(48000.0);

    // Beat 0 cai na amostra 0. Primeiro bloco: amostras 0..9 (o clique, de
    // 30ms a 48kHz, dura 1440 amostras — long e o suficiente pra continuar
    // no proximo bloco, que fica bem antes da proxima batida em 24000).
    std::vector<float> first(10, 0.0f);
    metronome.process(first, 0);
    check(peakAbs(first, 1, first.size()) > 0.0f, "o clique comecou no primeiro bloco");

    std::vector<float> second(10, 0.0f);
    metronome.process(second, 10);   // contiguo ao primeiro bloco

    check(peakAbs(second, 0, second.size()) > 0.0f, "o clique continua no segundo bloco, sem novo trigger");
}

// reset() descarta o clique em andamento.
void testResetDiscardsOngoingClick()
{
    std::cout << "reset() descarta o clique em andamento\n";

    MasterTransport transport;
    transport.prepare(48000.0);
    transport.setBpm(120.0f);
    transport.play();

    Metronome metronome(transport);
    metronome.prepare(48000.0);

    std::vector<float> first(10, 0.0f);
    metronome.process(first, 0);
    check(peakAbs(first, 0, first.size()) > 0.0f, "o clique comecou");

    metronome.reset();

    // Mesma posicao contigua, mas sem a batida de novo (longe da proxima em
    // 24000) — sem o reset, o clique continuaria tocando aqui.
    std::vector<float> second(10, 0.0f);
    metronome.process(second, 10);

    check(peakAbs(second, 0, second.size()) == 0.0f, "nenhum som depois do reset");
}

// O volume escala a amplitude do clique proporcionalmente.
void testVolumeScalesAmplitude()
{
    std::cout << "volume escala a amplitude do clique\n";

    MasterTransport transportCheio;
    transportCheio.prepare(48000.0);
    transportCheio.setBpm(120.0f);
    transportCheio.play();

    Metronome metronomoCheio(transportCheio);
    metronomoCheio.prepare(48000.0);
    metronomoCheio.setVolume(1.0f);

    std::vector<float> bufferCheio(10, 0.0f);
    metronomoCheio.process(bufferCheio, 0);

    MasterTransport transportMeio;
    transportMeio.prepare(48000.0);
    transportMeio.setBpm(120.0f);
    transportMeio.play();

    Metronome metronomoMeio(transportMeio);
    metronomoMeio.prepare(48000.0);
    metronomoMeio.setVolume(0.5f);

    std::vector<float> bufferMeio(10, 0.0f);
    metronomoMeio.process(bufferMeio, 0);

    const float picoCheio = peakAbs(bufferCheio, 0, bufferCheio.size());
    const float picoMeio = peakAbs(bufferMeio, 0, bufferMeio.size());

    check(picoCheio > 0.0f, "volume 1.0 produz som");
    checkClose(picoMeio, picoCheio * 0.5f, "volume 0.5 produz metade da amplitude");
}

// O metrônomo SOMA ao buffer, não substitui o que já estava lá.
void testClickIsAddedNotReplaced()
{
    std::cout << "o clique e somado ao buffer, nao substitui\n";

    MasterTransport transport;
    transport.prepare(48000.0);
    transport.setBpm(120.0f);
    transport.play();

    Metronome metronome(transport);
    metronome.prepare(48000.0);

    // Longe de qualquer batida (proxima em 24000): o sinal original deve
    // sobreviver intocado.
    std::vector<float> buffer(10, 0.5f);
    metronome.process(buffer, 1000);

    for (float sample : buffer)
        checkClose(sample, 0.5f, "amostra longe de uma batida continua 0.5");
}

int main()
{
    std::cout << "\n=== testes do Metronome ===\n\n";

    testStoppedTransportProducesSilence();
    testClickStartsExactlyOnTheBeat();
    testClickContinuesAcrossBlocks();
    testResetDiscardsOngoingClick();
    testVolumeScalesAmplitude();
    testClickIsAddedNotReplaced();

    return reportResults();
}
