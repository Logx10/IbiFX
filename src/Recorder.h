#pragma once

#include <atomic>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

// Recorder — grava um sinal em um arquivo .wav (Fase 17: "gravação de uma
// track, depois sessão" — esta é a parte de uma track só).
//
// POR QUE NÃO ESCREVE NO ARQUIVO DIRETO DA THREAD DE ÁUDIO
// Disco pode bloquear por milissegundos — uma caixa lenta, um antivírus
// examinando o arquivo, o sistema operacional decidindo fazer outra coisa
// primeiro. A thread de áudio não tem esses milissegundos de sobra: ela
// tem o orçamento de um bloco (2,7 ms a 48 kHz/128 amostras) pra devolver
// a vez, e qualquer esforço de I/O ali é o tipo de estalo que o projeto
// vem evitando desde a Fase 7.
//
// Esta divisão já estava prevista no ADR 0001, na tabela de threads:
// "E/S de disco: grava e lê arquivos, conversa por ring buffer com o
// áudio" — é exatamente o desenho aqui.
//
//     thread de áudio → buffer circular → thread de disco → arquivo .wav
//
// O BUFFER CIRCULAR É O MESMO ALGORITMO DA CommandQueue
// Um produtor (a thread de áudio, em pushSamples()), um consumidor (a
// thread de disco própria deste Recorder), dois índices atômicos, uma
// posição sempre vazia pra distinguir cheio de vazio — a mesma ideia de
// CommandQueue.h, só que carregando amostras de áudio em vez de comandos.
// Não foi extraído num tipo genérico compartilhado: é a segunda vez que
// esse algoritmo aparece no projeto, e a regra (AI_GUIDELINES §55) é
// esperar a terceira antes de abstrair.
//
// CHEIO NÃO BLOQUEIA, E TAMBÉM NÃO ENGANA
// Se a thread de disco cair pra trás (algo bem incomum — ela só acumula em
// memória, não escreve em disco até o stop()), pushSamples() descarta as
// amostras que não couberem, em vez de esperar. Perder um trecho de
// gravação é ruim; travar o áudio inteiro por causa disso seria pior.
//
// O ARQUIVO SÓ É ESCRITO NO stop()
// A thread de disco só ACUMULA em memória enquanto grava — wav::write()
// não foi desenhado para escrita incremental (o cabeçalho RIFF precisa do
// tamanho final, conhecido só no fim). Escrever o .wav de verdade acontece
// uma vez, em stop(), que por isso BLOQUEIA até o arquivo estar gravado —
// mas stop() é chamado do domínio de controle, nunca da thread de áudio,
// então esse bloqueio não é um problema aqui.
class Recorder
{
public:
    // capacitySamples é o tamanho do buffer circular — não o limite da
    // gravação, que pode durar o quanto for preciso; é só a folga entre o
    // que a thread de áudio produz e o que a thread de disco consegue
    // acumular. O padrão (acima de 1s a 48 kHz) é generoso o bastante para
    // a thread de disco, que só mexe em memória, nunca ficar pra trás de
    // verdade.
    explicit Recorder(std::size_t capacitySamples = 65536);

    // Para a gravação (se ainda estiver rolando) e junta a thread de disco
    // antes de destruir — nenhuma gravação é perdida por esquecerem de
    // chamar stop().
    ~Recorder();

    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;
    Recorder(Recorder&&) = delete;
    Recorder& operator=(Recorder&&) = delete;

    void prepare(double sampleRate);

    // Começa a gravar: zera o buffer, sobe a thread de disco. Lança se já
    // estiver gravando. Chamada do domínio de controle.
    void start(const std::string& path);

    // Deposita amostras no buffer circular. Chamada da THREAD DE ÁUDIO, a
    // cada bloco processado — não bloqueia, não aloca. Sem efeito se não
    // estiver gravando (chamar sempre é seguro; quem usa não precisa
    // checar isRecording() antes).
    void pushSamples(const float* samples, std::size_t count);

    // Sinaliza a thread de disco para drenar o que resta e escrever o
    // arquivo, e espera (join) ela terminar. BLOQUEIA até o arquivo estar
    // gravado — chamada do domínio de controle, nunca da thread de áudio.
    // Sem efeito se não estiver gravando.
    void stop();

    bool isRecording() const;

    // Quantas amostras já foram de fato acumuladas pela thread de disco —
    // cresce enquanto grava, útil para mostrar progresso numa UI.
    std::size_t recordedSampleCount() const;

private:
    void diskThreadLoop();

    double m_sampleRate = 48000.0;
    std::string m_path;

    std::thread m_diskThread;
    std::atomic<bool> m_recording{false};
    std::atomic<bool> m_stopRequested{false};

    // O buffer circular: produtor é pushSamples() (thread de áudio),
    // consumidor é diskThreadLoop() (a própria thread de disco).
    std::vector<float> m_ring;
    std::atomic<std::size_t> m_writeIndex{0};
    std::atomic<std::size_t> m_readIndex{0};

    // Só a thread de disco mexe nestes dois.
    std::vector<float> m_recorded;
    std::atomic<std::size_t> m_recordedCount{0};
};
