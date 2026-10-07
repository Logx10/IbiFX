#pragma once

#include <vector>

// resample — converte um sinal inteiro, já em memória, de uma taxa de
// amostragem para outra (44100 Hz -> 48000 Hz, por exemplo).
//
// PRA QUE EXISTE
// Um arquivo gravado a 44100 Hz, tocado por um motor a 48000 Hz amostra a
// amostra, sai ~9% mais rápido e mais agudo — cada amostra dura menos do
// que durava na gravação. Numa impulse response isso não muda a "altura",
// mas desloca todas as ressonâncias do gabinete para cima, e o timbre não
// é mais o do gabinete que foi gravado. Converter a taxa antes resolve.
//
// COMO: INTERPOLAÇÃO POR SINC COM JANELA
// A teoria da amostragem diz que um sinal limitado em banda é reconstruído
// EXATAMENTE somando, em cada amostra, uma função sinc centrada nela. Para
// achar o valor num instante que cai entre duas amostras de entrada, soma-se
// a contribuição das amostras vizinhas pesadas pela sinc naquela distância.
// A sinc verdadeira é infinita; ela é cortada em kZeroCrossings passagens
// por zero de cada lado e suavizada por uma janela de Blackman, que evita o
// "ringing" de um corte seco. É o método de qualidade usado por conversores
// de taxa offline — linear seria mais barato, mas abafa os agudos.
//
// BAIXANDO A TAXA, É PRECISO FILTRAR
// De 96000 para 48000 Hz, tudo entre 24 e 48 kHz não cabe mais na taxa nova
// e voltaria espelhado como aliasing audível. Por isso, quando a taxa de
// saída é menor, a sinc é alargada (corte em toRate/2): ela já é o
// filtro passa-baixa que remove essa faixa antes de reamostrar.
//
// CUSTO
// O(saída × 2·kZeroCrossings) — pesado para um arquivo de minutos, mas
// trivial para uma IR de alguns milhares de amostras. Feito no domínio de
// controle (carregar, preparar), nunca na thread de áudio: aloca.
//
// AMPLITUDE PRESERVADA, NÃO A SOMA
// Um seno de amplitude 0.5 continua com amplitude 0.5 — é a semântica de
// SINAL. Quem converte uma impulse response precisa ainda compensar o
// número de amostras (ver Cabinet::prepare()).
std::vector<float> resample(const std::vector<float>& input, double fromRate, double toRate);
