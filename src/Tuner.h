#pragma once

#include <atomic>
#include <string>
#include <vector>

#include "AudioModule.h"

// Tuner — não processa o som, só ESCUTA e relata a nota mais próxima.
//
// DIFERENTE DE TODO OUTRO MÓDULO DO PROJETO
// Gain, Delay, Reverb etc. transformam o buffer. O Tuner não muda uma
// amostra sequer — process() é passthrough puro. O trabalho dele é analisar
// e guardar um resultado (frequencyHz(), noteName(), centsOff()) pra quem
// estiver do lado de fora ler, como o terminal ou uma UI.
//
// COMO DETECTA A ALTURA: YIN (De Cheveigné & Kawahara, 2002)
//
// A PRIMEIRA VERSÃO USAVA AUTOCORRELAÇÃO SIMPLES, E FALHAVA EM SINAL REAL
// Autocorrelação soma o produto do sinal com uma cópia deslocada de si
// mesmo — um sinal periódico produz uma soma alta no lag igual ao período
// verdadeiro, mas TAMBÉM em lags que correspondem a harmônicos fortes (o
// dobro da frequência, o triplo...). Numa onda senoidal pura de teste isso
// não aparece, porque não há harmônico nenhum. Numa corda de guitarra de
// verdade, com sobretons — às vezes um harmônico mais forte que a própria
// fundamental, dependendo da posição do captador —, a autocorrelação
// simples confundia qual pico era o "de verdade": testado ao vivo, o mi
// grave (~82 Hz) saía detectado ora como outro harmônico dele, pulando de
// nota a cada janela de análise.
//
// A DIFERENÇA DO YIN: EM VEZ DE SEMELHANÇA, MEDE DIFERENÇA
// Onde a autocorrelação pergunta "o quanto o sinal se PARECE com uma cópia
// deslocada?", o YIN pergunta "o quanto o sinal DIFERE de uma cópia
// deslocada?" — soma o quadrado da diferença, amostra a amostra, em vez do
// produto:
//
//     d(lag) = Σ (x[i] - x[i+lag])²
//
// No período verdadeiro, x[i] e x[i+lag] são praticamente a MESMA forma de
// onda inteira — harmônicos incluídos —, então a diferença cai perto de
// zero ali, de um jeito mais nítido do que a autocorrelação simples
// consegue separar fundamental de harmônico.
//
// NORMALIZAÇÃO PELA MÉDIA ACUMULADA
// d(lag) sozinho ainda cresce artificialmente pra lags maiores (menos
// amostras sobrepostas — o mesmo problema que a versão de autocorrelação
// já tinha). O YIN normaliza cada d(lag) pela MÉDIA de todos os d(1..lag)
// vistos até ali:
//
//     d'(lag) = d(lag) / ( (1/lag) · Σ_{j=1}^{lag} d(j) )
//
// O resultado, d'(lag), fica perto de 1 quando não há periodicidade
// nenhuma e cai bem abaixo de 1 exatamente no período verdadeiro — a busca
// então é achar o PRIMEIRO lag, da frequência mais aguda pra mais grave (na
// prática, do lag mais curto pro mais longo), em que d'(lag) cruza um
// limiar baixo (kYinThreshold) e depois desce até um mínimo local.
//
// Um lag inteiro sozinho ainda é grosseiro demais (dezenas de cents de erro
// perto de 440 Hz); o lag escolhido é refinado por interpolação parabólica
// com os dois vizinhos, igual antes — ver o comentário em analyzeWindow().
//
// JANELA DE ANÁLISE, NÃO BLOCO A BLOCO
// Detectar a nota mais grave de uma guitarra (mi grave, ~82 Hz) exige ver
// pelo menos um período inteiro — 1/82 s ≈ 12 ms, ou ~540 amostras a
// 44100 Hz. Um bloco de áudio típico (128 amostras) é curto demais. Por
// isso o Tuner acumula amostras num buffer próprio (2048 amostras, ~46 ms a
// 44100 Hz) e só roda a autocorrelação quando ele enche — não a cada bloco.
//
// CUSTO NA THREAD DE ÁUDIO: UM LIMITE CONHECIDO DESTA PRIMEIRA VERSÃO
// A autocorrelação é O(janela × faixa de lags) — nesta janela, perto de
// 1 milhão de multiplicações, tudo de uma vez, no bloco que completa a
// janela. Não é grátis, e roda dentro do process() — ou seja, na thread de
// áudio. §51 do AI_GUIDELINES manda medir antes de otimizar: use
// engine.overBudgetBlocks() (ver AudioDevice/LiveEngine) rodando o
// --tuner de verdade antes de assumir que isso é ou não é um problema no
// seu hardware.
//
// FAIXA DE FREQUÊNCIA: POR QUE 70 A 1200 Hz
// Cobre da corda mi grave aberta (~82 Hz, com folga pra afinações mais
// baixas) até bem além do mi agudo aberto (~330 Hz) — dá pra afinar notas
// pressionadas em traste médio também. Restringir a faixa não é só
// otimização: também evita que o algoritmo confunda um harmônico agudo
// com a nota fundamental.
class Tuner : public AudioModule
{
public:
    Tuner();

    const char* name() const override;

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    // Acumula amostras na janela de análise e, quando ela enche, atualiza
    // a leitura. Não altera o buffer — o som que entra é o que sai.
    void process(std::vector<float>& buffer) override;

    // Nome da nota mais próxima ("E2", "A4"...), ou string vazia se
    // isValid() for falso. Calculado na hora a partir de frequencyHz() — não
    // é um valor guardado separadamente (ver comentário sobre atômicos
    // abaixo).
    std::string noteName() const;

    // Frequência detectada em Hz, ou 0.0f se isValid() for falso.
    float frequencyHz() const;

    // Desvio em cents da nota mais próxima: negativo = grave, positivo =
    // agudo, perto de 0 = afinado. 100 cents = um semitom.
    float centsOff() const;

    // Verdadeiro quando a última janela analisada tinha um tom claro o
    // bastante pra confiar na leitura (não silêncio, não ruído puro).
    bool isValid() const;

private:
    void analyzeWindow();

    double m_sampleRate = 44100.0;

    // Só a thread de áudio toca nisso, dentro de process()/analyzeWindow() —
    // nunca lido de fora, então não precisa ser atômico.
    std::vector<float> m_window;
    std::size_t m_writePosition = 0;

    // d(lag) e d'(lag) do YIN — pré-alocados no construtor, do tamanho da
    // janela (limite superior seguro pra qualquer lag que a busca use), pra
    // analyzeWindow() nunca alocar dentro da thread de áudio.
    std::vector<float> m_difference;
    std::vector<float> m_cumulativeMeanDifference;

    // ATÔMICOS PELO MESMO MOTIVO DO Parameter
    // process() escreve aqui na thread de áudio; noteName(), frequencyHz(),
    // centsOff() e isValid() são lidos de fora — o terminal do --tuner, por
    // exemplo — na thread de controle. Sem atomicidade seria a mesma corrida
    // de dados que o comentário do Parameter.h explica. noteName() e
    // centsOff() não precisam de atômico próprio: são calculados a partir de
    // m_frequencyHz na hora da leitura, não guardados.
    std::atomic<float> m_frequencyHz{0.0f};
    std::atomic<bool> m_valid{false};
};
