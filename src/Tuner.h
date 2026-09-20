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
// COMO DETECTA A ALTURA: AUTOCORRELAÇÃO
// A ideia: um sinal periódico se parece consigo mesmo quando deslocado
// exatamente um período. Pra cada deslocamento (lag) candidato, soma o
// produto amostra-a-amostra do sinal com sua cópia deslocada — um sinal
// periódico produz uma soma grande exatamente no lag igual ao período (e em
// múltiplos dele); ruído não produz pico nenhum. O lag com a maior soma
// dessa "auto-comparação", dentro da faixa de frequências de um violão/
// guitarra, dá o período — e frequência = sampleRate / período.
//
// Um lag inteiro sozinho é grosseiro demais (dezenas de cents de erro perto
// de 440 Hz), então o lag vencedor é refinado por interpolação parabólica
// entre ele e seus dois vizinhos — ver o comentário em analyzeWindow().
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
