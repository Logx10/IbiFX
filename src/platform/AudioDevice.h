#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

// AudioDevice — a única porta do IbiFX para o hardware de áudio.
//
// A FRONTEIRA
// O princípio 1 do ARCHITECTURE diz que o núcleo não deve conhecer sistema
// operacional nem driver. Este arquivo é onde essa fronteira mora: ele é o
// único lugar do projeto que fala com a biblioteca de plataforma.
//
// Repare que este header NÃO inclui miniaudio.h. Ele guarda um ponteiro para
// uma estrutura declarada só no .cpp — a técnica chamada pimpl. Duas
// consequências práticas: nenhum arquivo do core acaba incluindo 4 MB de
// biblioteca por tabela, e trocar miniaudio por JUCE depois mexe em um .cpp
// só, sem recompilar o resto nem alterar uma linha de DSP.
//
// O CALLBACK RODA NA THREAD DE ÁUDIO
// A função entregue ao start() é chamada pelo driver, numa thread de alta
// prioridade que o sistema operacional cria. Valem ali todas as regras do
// §77 do AI_GUIDELINES: sem alocação, sem disco, sem rede, sem mutex, sem
// logging pesado. Estourar o prazo produz estalo.
//
// É exatamente por isso que a CommandQueue existe: ela é o caminho por onde
// as mudanças de controle chegam a esse callback sem que ninguém espere.
//
// FORMATO DO BUFFER
// O driver entrega as amostras INTERCALADAS (L R L R...), enquanto os módulos
// processam um canal por vez. A conversão entre os dois formatos é
// responsabilidade de quem usa esta classe, e precisa usar buffers alocados
// de antemão.
//
// O BACKEND NULO
// start() aceita um modo sem hardware nenhum, em que o miniaudio gera os
// blocos por conta própria, no ritmo do relógio. Serve para teste automático:
// permite verificar que o dispositivo abre, que o callback é chamado e que o
// DSP roda, tudo sem depender de placa de som, de permissão de microfone ou
// de alguém escutando.
class AudioDevice
{
public:
    // Chamada uma vez por bloco, na thread de áudio.
    //
    //   output      buffer intercalado a ser preenchido
    //   input       buffer intercalado com o que entrou, ou nullptr
    //   frameCount  quantos frames, não amostras
    using ProcessCallback = std::function<void(float* output,
                                               const float* input,
                                               std::size_t frameCount,
                                               std::size_t channelCount)>;

    enum class Mode
    {
        Playback,   // só saída
        Duplex,     // entrada e saída, para tocar guitarra através dos efeitos
        Null        // sem hardware, para teste automático
    };

    AudioDevice();
    ~AudioDevice();

    // O objeto é dono de um dispositivo do sistema e de uma thread ativa.
    // Copiar ou mover isso não faria sentido.
    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;
    AudioDevice(AudioDevice&&) = delete;
    AudioDevice& operator=(AudioDevice&&) = delete;

    // Abre o dispositivo e começa a chamar o callback.
    //
    // sampleRate e blockSize são PEDIDOS, não garantias: o driver pode
    // devolver outros valores, e os métodos abaixo informam o que valeu de
    // fato. Devolve false em caso de falha, com o motivo em lastError().
    bool start(ProcessCallback callback,
               Mode mode = Mode::Duplex,
               double sampleRate = 48000.0,
               int blockSize = 128);

    // Para o dispositivo e libera os recursos. Seguro chamar duas vezes.
    void stop();

    bool isRunning() const;

    // Valores efetivamente negociados com o driver. Zero antes do start().
    double sampleRate() const;
    std::size_t channelCount() const;

    // Nome do dispositivo escolhido pelo sistema.
    std::string deviceName() const;

    // Descrição da última falha, ou string vazia.
    std::string lastError() const;

    // Quantos blocos o callback processou desde o start().
    //
    // Serve para teste e diagnóstico: se o número não cresce, o dispositivo
    // abriu mas não está rodando.
    std::size_t processedBlocks() const;

    // MEDIÇÃO DE CARGA REAL
    //
    // O §51 do AI_GUIDELINES pede para medir, não adivinhar, quando
    // performance importa. Estes três números respondem "o callback está
    // dentro do prazo?" sem exigir estatísticas específicas de driver, que
    // variam entre WASAPI, CoreAudio e ALSA.
    //
    // O ORÇAMENTO de um bloco é o tempo real que ele representa: com
    // blockSize=128 e sampleRate=48000, chegam 128/48000 s ≈ 2,67 ms de
    // áudio por vez, e o callback precisa devolver o bloco processado antes
    // que esse tempo passe. Ultrapassar o orçamento é o que causa o estalo
    // de um underrun — o driver pede o próximo bloco e ele ainda não está
    // pronto.

    // Duração do callback mais recente, em microssegundos.
    std::uint64_t lastCallbackMicros() const;

    // Maior duração observada desde o start(), em microssegundos.
    //
    // O pico importa mais que a média: um callback que estoura o orçamento
    // uma vez a cada mil já produz um estalo audível, mesmo que a média
    // pareça tranquila.
    std::uint64_t maxCallbackMicros() const;

    // Quantos blocos, desde o start(), levaram mais tempo que o orçamento.
    std::size_t overBudgetBlocks() const;

    // Público apenas para que a função de ponte com o miniaudio, no .cpp,
    // possa acessá-la. O tipo é declarado sem definição aqui, então ninguém
    // de fora consegue fazer nada com ele — é opaco na prática.
    struct Impl;

private:
    std::unique_ptr<Impl> m_impl;
};
