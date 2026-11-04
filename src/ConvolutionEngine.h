#pragma once

#include <cstddef>
#include <vector>

#include "Fft.h"

// ConvolutionEngine — aplica uma impulse response a um sinal, amostra a
// amostra, por convolução direta.
//
// A CONTA
// Convolução é uma soma ponderada do PASSADO do sinal, com os pesos vindos
// da impulse response (IR):
//
//     y[n] = Σ_{k=0}^{M-1} h[k] · x[n-k]
//
// h é a IR (M amostras — o "peso" de cada atraso), x é a entrada, y é a
// saída. Cada amostra de saída é uma combinação das M amostras de entrada
// mais recentes, cada uma multiplicada pelo tap correspondente da IR.
//
// POR QUE ISSO REPRODUZ UM GABINETE/MICROFONE
// Um alto-falante dentro de um gabinete, captado por um microfone, é (numa
// boa aproximação) um sistema LINEAR e INVARIANTE NO TEMPO: dobrar o sinal
// de entrada dobra a saída, e o comportamento não muda com o tempo. Todo
// sistema assim é completamente descrito pela forma como reage a um
// impulso — e convolver qualquer sinal com essa resposta reproduz o que
// esse sistema faria com aquele sinal, exatamente, não por semelhança.
//
// O BUFFER CIRCULAR
// Pra calcular y[n] é preciso lembrar as M amostras de entrada mais
// recentes — exatamente como o buffer circular do Delay, só que aqui TODAS
// as M posições são lidas e ponderadas a cada amostra de saída, não uma só.
//
// CUSTO DA CONTA DIRETA: O(M) POR AMOSTRA
// Fazer a soma acima como está escrita multiplica TODOS os M taps a cada
// amostra de saída. Com IRs de poucas centenas de amostras, tudo bem; uma
// IR comercial de 200 ms a 48 kHz tem 9600 taps — 460 milhões de
// multiplicações por segundo, medidos em 33% de um núcleo (Release) só
// para o gabinete, e 81% com a versão de 500 ms. Isso estalava.
//
// A SAÍDA: CONVOLUÇÃO NO DOMÍNIO DA FREQUÊNCIA
// Convolução no tempo é MULTIPLICAÇÃO na frequência: FFT da entrada, vezes
// a FFT da IR, FFT inversa. Com a FFT (Fft.h), isso custa ~N·log2(N) em
// vez de N². O problema é que a FFT trabalha em BLOCOS — esperar um bloco
// inteiro de entrada antes de responder é latência, e num pedal ao vivo
// latência é o que mais se sente.
//
// CABEÇA DIRETA + CAUDA PARTICIONADA: LATÊNCIA ZERO
// A IR é dividida em pedaços de kBlockSize (128) taps:
//
//     h = [ cabeça | parte 1 | parte 2 | ... | parte P ]
//          direta    ------- via FFT, em blocos -------
//
// - A CABEÇA (os primeiros 128 taps) é feita pela conta direta, amostra a
//   amostra, com o buffer circular descrito acima: responde na hora, sem
//   latência. Custa 128 multiplicações por amostra, não M.
// - A parte k começa no tap k·128: ela só enxerga entradas de, no mínimo,
//   k·128 amostras atrás. Então a contribuição de TODA a cauda para o
//   próximo bloco de 128 amostras depende só de blocos de entrada que já
//   terminaram — dá para calculá-la inteira no instante em que um bloco
//   fecha, e ela fica pronta exatamente quando é necessária.
//
// A CONTA DE CADA BLOCO (OVERLAP-SAVE)
// Quando um bloco de entrada x_j fecha:
// 1. X_j = FFT de [bloco anterior | bloco atual] (256 amostras). Guardado
//    na "linha de atraso em frequência" (FDL), que lembra os últimos P
//    espectros de entrada — o buffer circular de novo, só que de espectros.
// 2. Y = Σ_k H_k · X_(j+1-k), para k = 1..P. H_k é a FFT da parte k,
//    calculada uma vez em setImpulseResponse().
// 3. FFT inversa de Y. As últimas 128 amostras são a saída da cauda para o
//    bloco seguinte (as primeiras 128 são o "lixo" da convolução circular
//    — por isso se chama overlap-SAVE: guarda a parte que presta).
//
// O custo cai de M para ~128 + 4·P operações por amostra. Medido em
// Release, a 48 kHz, com o Cabinet inteiro: a Celestion V30 de 500 ms (24
// mil taps, P = 187) foi de 81% para 2,4% de um núcleo; a de 200 ms, de
// 33% para 1,5%.
//
// O TRABALHO NÃO É UNIFORME
// A cauda é calculada inteira numa amostra a cada 128. processSample() é
// chamado num laço sobre o bloco do driver, então o que importa é o custo
// total do bloco — mas com um bloco do driver menor que 128, um callback
// paga a conta e os outros não.
class ConvolutionEngine
{
public:
    // Define a impulse response. ALOCA (históricos, espectros das partes,
    // tabelas da FFT) — nunca chame isto de dentro de
    // process()/processSample(), só do domínio de controle (ao carregar um
    // preset, por exemplo).
    void setImpulseResponse(std::vector<float> impulseResponse);

    // Zera o histórico de entrada (da cabeça e da cauda), sem descartar a
    // impulse response.
    void reset();

    // Convolve uma amostra e devolve o resultado. Sem impulse response
    // carregada, devolve a amostra intocada (passthrough).
    float processSample(float input);

private:
    using Complex = Fft::Complex;

    static constexpr std::size_t kBlockSize = 128;
    static constexpr std::size_t kFftSize = 2 * kBlockSize;

    // Bins guardados de cada espectro: só 0..N/2. A FFT de um sinal REAL é
    // simétrica (X[N-i] = conjugado de X[i]), então a outra metade não traz
    // informação nova — e guardar só esta metade corta pela metade a conta
    // do passo 2.
    static constexpr std::size_t kSpectrumBins = kFftSize / 2 + 1;

    // Fecha um bloco de entrada: passos 1 a 3 do comentário da classe.
    void processTailBlock();

    // --- cabeça: conta direta ---
    std::vector<float> m_head;
    // As últimas m_head.size() amostras de entrada.
    std::vector<float> m_history;
    std::size_t m_writePosition = 0;

    // --- cauda: partes via FFT ---
    Fft m_fft;

    // H_k, um espectro de kSpectrumBins bins por parte.
    std::vector<std::vector<Complex>> m_partSpectra;

    // A FDL: os últimos P espectros de entrada, em buffer circular.
    // m_newestSpectrum aponta para o mais recente.
    std::vector<std::vector<Complex>> m_inputSpectra;
    std::size_t m_newestSpectrum = 0;

    // [bloco anterior | bloco atual] — a janela de 256 amostras do passo 1.
    std::vector<float> m_inputWindow;
    std::size_t m_blockPosition = 0;

    // Saída da cauda para o bloco em andamento.
    std::vector<float> m_tailOutput;

    // Rascunho da FFT e acumulador de Y, alocados uma vez só.
    std::vector<Complex> m_scratch;
    std::vector<Complex> m_accumulator;
};
