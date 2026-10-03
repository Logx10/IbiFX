// Testes do MasterTransport — o relógio compartilhado da Fase 14.
//
// A infra de verificação vive em test_helpers.h.

#include <iostream>

#include "MasterTransport.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------

// Parado, advance() não move a posição.
void testStoppedTransportDoesNotAdvance()
{
    std::cout << "parado, advance() nao move a posicao\n";

    MasterTransport transport;
    transport.prepare(48000.0);

    transport.advance(128);
    transport.advance(128);

    check(transport.positionSamples() == 0, "posicao continua em 0");
    check(!transport.isPlaying(), "isPlaying() e falso por padrao");
}

// Tocando, advance() soma frameCount a cada chamada.
void testPlayingTransportAdvances()
{
    std::cout << "tocando, advance() soma frameCount\n";

    MasterTransport transport;
    transport.prepare(48000.0);
    transport.play();

    transport.advance(128);
    check(transport.positionSamples() == 128, "primeiro bloco avancou 128");

    transport.advance(128);
    check(transport.positionSamples() == 256, "segundo bloco somou mais 128");

    check(transport.isPlaying(), "isPlaying() e verdadeiro depois de play()");
}

// stop() congela a posição onde estava — não volta para 0.
void testStopFreezesPositionInPlace()
{
    std::cout << "stop() congela a posicao, nao volta pro inicio\n";

    MasterTransport transport;
    transport.prepare(48000.0);
    transport.play();
    transport.advance(500);

    transport.stop();
    check(!transport.isPlaying(), "isPlaying() e falso depois de stop()");

    transport.advance(1000);
    check(transport.positionSamples() == 500, "posicao continua em 500, parada");
}

// seek() move a posição mesmo com o transporte parado, e o próximo
// advance() parte dali.
void testSeekMovesPositionEvenWhileStopped()
{
    std::cout << "seek() move a posicao\n";

    MasterTransport transport;
    transport.prepare(48000.0);

    transport.seek(1000);
    transport.advance(0);   // consome o seek pendente sem tocar
    check(transport.positionSamples() == 1000, "seek aplicado mesmo parado");

    transport.play();
    transport.advance(100);
    check(transport.positionSamples() == 1100, "avanco parte do ponto do seek");
}

// positionSeconds() e positionBeats() convertem corretamente a partir do
// sample rate e do BPM.
void testPositionConversions()
{
    std::cout << "conversao de posicao para segundos e batidas\n";

    MasterTransport transport;
    transport.prepare(100.0);   // 100 Hz: facil de calcular de cabeca
    transport.setBpm(120.0f);   // 2 batidas por segundo
    transport.play();

    transport.advance(250);   // 250 amostras a 100 Hz = 2.5 s

    checkClose(static_cast<float>(transport.positionSeconds()), 2.5f, "2.5 segundos");
    checkClose(static_cast<float>(transport.positionBeats()), 5.0f, "a 120 bpm, 2.5s sao 5 batidas");
}

// O loop volta para o início ao ultrapassar o fim, preservando o excesso
// por módulo — não descartando a fração que passou do fim.
void testLoopWrapsPreservingOvershoot()
{
    std::cout << "loop volta ao inicio preservando o excesso\n";

    MasterTransport transport;
    transport.prepare(48000.0);
    transport.setLoop(100, 200);   // comprimento 100
    transport.setLoopEnabled(true);
    transport.play();
    transport.seek(180);
    transport.advance(0);

    // 180 + 50 = 230, que passa do fim (200) em 30. Volta para 100 + 30 = 130,
    // nao direto para 100 (o que perderia os 30 de excesso).
    transport.advance(50);
    check(transport.positionSamples() == 130, "excesso de 30 preservado apos o wrap");
}

// Um loop desabilitado não interfere, mesmo com start/end configurados.
void testDisabledLoopDoesNotWrap()
{
    std::cout << "loop desabilitado nao interfere\n";

    MasterTransport transport;
    transport.prepare(48000.0);
    transport.setLoop(100, 200);
    transport.setLoopEnabled(false);
    transport.play();
    transport.seek(190);
    transport.advance(0);

    transport.advance(50);
    check(transport.positionSamples() == 240, "passou do 'fim do loop' porque o loop esta desligado");
}

// Um loop invertido ou vazio (end <= start) é tratado como ausente, em vez
// de travar o tempo ou inverter o sentido.
void testInvertedLoopIsIgnored()
{
    std::cout << "loop invertido ou vazio e ignorado\n";

    MasterTransport transport;
    transport.prepare(48000.0);
    transport.setLoop(200, 100);   // invertido de proposito
    transport.setLoopEnabled(true);
    transport.play();

    transport.advance(1000);
    check(transport.positionSamples() == 1000, "avancou normalmente, sem tentar aplicar o loop invertido");
}

// setRecording()/isRecording() são só um sinalizador — não afetam
// isPlaying() nem a posição.
void testRecordingIsIndependentOfPlayback()
{
    std::cout << "gravar e so um sinalizador, independente do play\n";

    MasterTransport transport;
    transport.prepare(48000.0);

    check(!transport.isRecording(), "nao esta gravando por padrao");

    transport.setRecording(true);
    check(transport.isRecording(), "isRecording() reflete o que foi setado");
    check(!transport.isPlaying(), "setRecording() nao liga o play sozinho");

    transport.advance(500);
    check(transport.positionSamples() == 0, "gravar sem tocar nao avanca o relogio");
}

int main()
{
    std::cout << "\n=== testes do MasterTransport ===\n\n";

    testStoppedTransportDoesNotAdvance();
    testPlayingTransportAdvances();
    testStopFreezesPositionInPlace();
    testSeekMovesPositionEvenWhileStopped();
    testPositionConversions();
    testLoopWrapsPreservingOvershoot();
    testDisabledLoopDoesNotWrap();
    testInvertedLoopIsIgnored();
    testRecordingIsIndependentOfPlayback();

    return reportResults();
}
