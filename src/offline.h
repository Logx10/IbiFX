#pragma once

#include <string>

#include "ModuleChain.h"
#include "WavFile.h"

// offline — processamento de arquivo, fora de tempo real.
//
// É a ponte entre o engine de DSP e algo que se possa ouvir. O §26 do
// AI_GUIDELINES descreve exatamente este fluxo:
//
//     input.wav  ->  DSP  ->  output.wav
//
// A vantagem sobre o tempo real é que aqui não há prazo. Se um bloco demorar,
// ninguém escuta falha — só demora mais. Isso permite validar os algoritmos
// com o ouvido antes de enfrentar callback, latência e real-time safety.
namespace offline
{
// Processa um arquivo inteiro pela cadeia, canal a canal.
//
// Cada canal passa pela MESMA cadeia, um depois do outro, e a cadeia é
// reiniciada entre eles: sem o reset, o eco do canal esquerdo vazaria para o
// direito. Isso também significa que módulos com estado tratam os canais de
// forma independente, que é o comportamento esperado de uma pedaleira mono
// aplicada a cada lado.
//
// blockSize existe para simular a fatia que um dispositivo de áudio real
// entregaria. Processar o arquivo inteiro de uma vez daria outro resultado
// em qualquer módulo com estado, e mascararia bugs de fronteira de bloco.
WavFile processFile(const WavFile& input, ModuleChain& chain, int blockSize = 512);

// Gera um sinal de teste: uma nota que decai, como uma corda tocada.
//
// Serve para experimentar os efeitos sem precisar de um arquivo externo.
// Não é uma guitarra — é uma senoide com harmônicos e envelope — mas tem o
// suficiente para que distorção, eco e ganho fiquem audíveis.
WavFile generateTestSignal(double sampleRate = 44100.0, double seconds = 3.0);
}
