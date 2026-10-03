#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

// MasterTransport — o relógio compartilhado da Fase 14: posição, BPM,
// play/stop/record/loop.
//
// PARA QUE SERVE
// Até aqui, cada módulo tinha seu próprio tempo interno (o buffer circular
// do Delay, o envelope do Compressor). Metrônomo (Fase 15), backing tracks
// (Fase 16) e o gravador (Fase 17) precisam de um relógio ÚNICO e
// compartilhado — senão o metrônomo bate num tempo, a faixa de apoio toca
// em outro, e a gravação não alinha com nenhum dos dois.
//
// O QUE ELE NÃO FAZ
// MasterTransport não processa amostra nenhuma e não é um AudioModule. Ele
// só CONTA — quem lê a contagem decide o que fazer: o metrônomo decide
// quando soar um clique, o player de backing track decide qual frame do
// arquivo tocar. Isso mantém o relógio independente de qualquer um dos
// consumidores, e permite adicionar consumidores novos sem mexer aqui.
//
// A REGRA DAS DUAS THREADS, DE NOVO
// advance() roda na thread de áudio, uma vez por bloco — é ela que faz o
// tempo passar de verdade, em amostras, não em um timer de parede que
// poderia atrasar ou adiantar por causa do sistema operacional.
// play(), stop(), setBpm(), setLoop() e seek() rodam no domínio de
// controle (UI, MIDI Program Change futuro, etc.).
//
// POR QUE seek() NÃO ESCREVE A POSIÇÃO DIRETO
// advance() faz um LER-SOMAR-GRAVAR em m_positionSamples a cada bloco. Se
// seek() gravasse ali diretamente, um seek() chegando entre o LER e o
// GRAVAR de advance() seria apagado sem ninguém perceber — o clique de
// "voltar pro início" simplesmente não aconteceria, e o próximo bloco
// continuaria como se nada tivesse sido pedido.
//
// A correção é a mesma ideia da CommandQueue, só que para um único valor
// em vez de uma fila: seek() deposita um ALVO e levanta uma bandeira;
// advance() confere a bandeira ANTES de somar o bloco, e se houver um
// pedido pendente, parte dali em vez do valor antigo. m_positionSamples
// continua tendo um único escritor de verdade — a própria advance() — o
// que elimina a corrida sem precisar de fila nem de cadeado.
class MasterTransport
{
public:
    MasterTransport() = default;

    // Guarda atômicos que a thread de áudio pode estar lendo ou escrevendo
    // neste instante — copiar ou mover misturaria dois relógios num só.
    MasterTransport(const MasterTransport&) = delete;
    MasterTransport& operator=(const MasterTransport&) = delete;
    MasterTransport(MasterTransport&&) = delete;
    MasterTransport& operator=(MasterTransport&&) = delete;

    // Informa o sample rate, para as conversões de posição em segundos e
    // em batidas. Chamada do domínio de controle, antes do primeiro
    // advance() e de novo sempre que o dispositivo de áudio mudar.
    void prepare(double sampleRate);

    // --- Domínio de controle ---

    void play();
    void stop();
    bool isPlaying() const;

    // Apenas um sinalizador de estado — MasterTransport não grava nada
    // sozinho. É o Recorder (Fase 17) quem lê isRecording() e decide o que
    // fazer com o áudio que passa.
    void setRecording(bool recording);
    bool isRecording() const;

    // Batidas por minuto. Usado por quem converte posição em amostras para
    // posição em batidas (positionBeats()) e pelo metrônomo (Fase 15) para
    // saber de quanto em quanto tempo bater.
    void setBpm(float bpm);
    float bpm() const;

    // Região de loop, em amostras. endSamples deve ser maior que
    // startSamples para o loop ter efeito; um loop "zerado" ou invertido é
    // tratado como ausente por advance(), em vez de travar ou inverter o
    // tempo.
    void setLoop(std::uint64_t startSamples, std::uint64_t endSamples);
    void setLoopEnabled(bool enabled);
    bool isLoopEnabled() const;
    std::uint64_t loopStartSamples() const;
    std::uint64_t loopEndSamples() const;

    // Pede para a posição pular para um ponto específico. Não aplica na
    // hora — ver o comentário da classe sobre por que isso é uma bandeira
    // pendente, consumida pelo próximo advance().
    void seek(std::uint64_t positionSamples);

    // --- Lido por qualquer domínio ---

    std::uint64_t positionSamples() const;
    double positionSeconds() const;
    double positionBeats() const;

    // --- Domínio de áudio ---

    // Avança a posição em frameCount amostras, se estiver tocando (parado,
    // o tempo congela — não existe "tocar no lugar"). Aplica um seek()
    // pendente antes de somar o bloco, e depois disso aplica o loop: se a
    // nova posição ultrapassou o fim do loop, ela volta para o início,
    // preservando o excesso por módulo — não apenas "pulando pro início",
    // o que perderia a fração de bloco que já tinha avançado e faria o
    // loop derivar de tamanho a cada volta.
    //
    // Chamada uma vez por bloco, só pela thread de áudio.
    void advance(std::size_t frameCount);

private:
    std::atomic<double> m_sampleRate{48000.0};
    std::atomic<float> m_bpm{120.0f};

    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_recording{false};

    std::atomic<bool> m_loopEnabled{false};
    std::atomic<std::uint64_t> m_loopStartSamples{0};
    std::atomic<std::uint64_t> m_loopEndSamples{0};

    std::atomic<std::uint64_t> m_positionSamples{0};

    // O pedido de seek pendente — ver o comentário da classe.
    std::atomic<bool> m_seekPending{false};
    std::atomic<std::uint64_t> m_seekTarget{0};
};
