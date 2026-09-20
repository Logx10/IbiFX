#pragma once

#include <cstddef>
#include <vector>

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
// CUSTO: O(M) POR AMOSTRA — UM LIMITE CONHECIDO DESTA PRIMEIRA VERSÃO
// Uma IR de gabinete real costuma ter centenas a milhares de amostras. Essa
// implementação multiplica e soma TODAS elas pra cada amostra de saída —
// direto, fácil de entender, mas caro: com M = 2000 e sampleRate = 48000,
// são 96 milhões de multiplicações por segundo, na thread de áudio. O
// próprio AI_GUIDELINES (§34) já antecipa isso: "FFT convolution
// posteriormente" — a técnica que resolve esse custo é assunto próprio
// (convolução via FFT, ou convolução particionada), e só vale a pena depois
// de medir que o custo direto é de fato um problema no seu hardware (§51:
// correto, depois mensurável, depois otimizado).
class ConvolutionEngine
{
public:
    // Define a impulse response. ALOCA o buffer de histórico do tamanho
    // dela — nunca chame isto de dentro de process()/processSample(), só do
    // domínio de controle (ao carregar um preset, por exemplo).
    void setImpulseResponse(std::vector<float> impulseResponse);

    // Zera o histórico de entrada, sem descartar a impulse response.
    void reset();

    // Convolve uma amostra e devolve o resultado. Sem impulse response
    // carregada, devolve a amostra intocada (passthrough).
    float processSample(float input);

private:
    std::vector<float> m_impulseResponse;

    // As últimas m_impulseResponse.size() amostras de entrada.
    std::vector<float> m_history;
    std::size_t m_writePosition = 0;
};
