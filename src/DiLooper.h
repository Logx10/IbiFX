#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

// DiLooper — grava um trecho da guitarra SECA na memória e o toca de volta
// no lugar da guitarra, para timbrar sem precisar ficar tocando.
//
// O PROBLEMA QUE ELE RESOLVE
// Timbrar é girar um knob e ouvir o resultado. Com a guitarra no colo, cada
// ajuste vira "solta o knob, pega a palheta, toca o riff, volta pro knob" —
// e o riff nunca sai igual duas vezes, então fica difícil saber se o som
// mudou por causa do knob ou da mão. Os plugins de amp caros resolvem isso
// gravando a DI uma vez e tocando-a em loop pela cadeia: a mão fica livre
// para os knobs, e o riff é sempre idêntico.
//
// POR QUE A GRAVAÇÃO É SECA, E ENTRA ANTES DA CADEIA
// Gravar o som já processado não serviria: o que se quer é ouvir o MESMO
// riff passando por pedais e amps DIFERENTES. Por isso o take é a guitarra
// crua, e na reprodução ele SUBSTITUI a entrada — vai para o lugar onde a
// guitarra entraria, antes de qualquer módulo. É o mesmo princípio do
// reamp (ver ReampRecorder.h), só que em loop e sem passar pelo disco.
//
// NA MEMÓRIA, NÃO NO DISCO
// O ReampRecorder escreve arquivos numa thread própria, porque gravações
// longas não cabem na memória. Aqui o take é curto (um riff, até
// kMaxSeconds) e precisa ser relido do começo a cada volta do loop, então
// mora num vetor alocado no prepare(). A thread de áudio só escreve e lê
// posições dele — não aloca, não bloqueia.
//
// QUEM MUDA O ESTADO: SÓ A THREAD DE ÁUDIO
// A interface não mexe no buffer nem no estado diretamente: ela deposita um
// PEDIDO (gravar, parar, tocar...) num atômico, e process() o aplica no
// começo do próximo bloco. Assim existe um único escritor do estado e do
// buffer, e nunca há duas threads mexendo no take ao mesmo tempo — a mesma
// ideia da fila de comandos do ModuleChain, reduzida a um pedido por vez
// (a interface só tem um botão apertado de cada vez).
//
// CORTE AUTOMÁTICO DO SILÊNCIO
// Entre apertar "gravar" e tocar a primeira nota sempre há silêncio, e
// outro depois da última. Tocar isso em loop deixaria um buraco a cada
// volta. Durante a gravação, process() anota a primeira e a última amostra
// acima de kSilenceThreshold (uma comparação por amostra); ao parar, o take
// vira só esse trecho, mais uma folga (kPreRoll antes, kPostRoll depois,
// para não cortar o ataque nem o fim da nota). Anotar enquanto grava evita
// varrer o take inteiro dentro do callback na hora de parar.
//
// FADES NAS EMENDAS
// Um take que termina com o sinal em +0,3 e recomeça em -0,2 dá um degrau
// de uma amostra para a outra — um estalo, a cada volta. Por isso o take é
// tocado com um fade de kFadeSeconds no começo e no fim, e parar no meio
// também desce em fade em vez de cortar seco.
class DiLooper
{
public:
    enum class State
    {
        Empty,      // nada gravado
        Recording,  // gravando a entrada
        Ready,      // take pronto, parado
        Playing     // tocando o take no lugar da guitarra
    };

    // Duração máxima de um take. Um riff para timbrar cabe com folga, e o
    // buffer (alocado inteiro no prepare) fica em ~11 MB a 48 kHz.
    static constexpr double kMaxSeconds = 60.0;

    DiLooper() = default;
    DiLooper(const DiLooper&) = delete;
    DiLooper& operator=(const DiLooper&) = delete;

    // Aloca o buffer para sampleRate. Só com o áudio parado. Se a taxa não
    // mudou, o take atual é preservado — reabrir o dispositivo não deveria
    // apagar o riff gravado.
    void prepare(double sampleRate);

    // --- domínio de controle (interface): só depositam o pedido ----------
    void startRecording();
    void stopRecording();
    void play();
    void stop();
    // Espaço: toca se estiver parado, para se estiver tocando.
    void togglePlay();
    void clear();

    // Recomeçar do início ao chegar no fim (padrão) ou parar.
    void setLoop(bool loop);
    bool loop() const;

    // Leituras para a interface (atômicas, podem atrasar um bloco).
    State state() const;
    double takeSeconds() const;
    double recordedSeconds() const;
    double playheadSeconds() const;

    // --- thread de áudio -------------------------------------------------
    // Aplica o pedido pendente e então: gravando, copia o bloco para o take
    // (sem alterá-lo — você continua ouvindo a guitarra); tocando,
    // SUBSTITUI o bloco pelo take. Nos outros estados, não toca no bloco.
    void process(float* mono, std::size_t frames);

private:
    enum class Request
    {
        None,
        StartRecording,
        StopRecording,
        Play,
        Stop,
        TogglePlay,
        Clear
    };

    void request(Request request);
    void applyRequest(Request request);
    void finishRecording();

    // Ganho do fade na posição index do take: sobe no começo, desce no fim.
    float edgeGain(std::size_t index) const;

    double m_sampleRate = 0.0;
    std::size_t m_fadeLength = 1;

    // O take inteiro mora aqui; [m_takeStart, m_takeEnd) é o trecho tocado.
    std::vector<float> m_buffer;
    std::size_t m_recorded = 0;
    std::size_t m_takeStart = 0;
    std::size_t m_takeEnd = 0;
    std::size_t m_playhead = 0;

    // Primeira e última amostra acima do limiar, anotadas enquanto grava.
    std::size_t m_firstLoud = 0;
    std::size_t m_lastLoud = 0;
    bool m_heardSomething = false;

    // Parada em fade: quantas amostras faltam até o silêncio (0 = não está
    // parando).
    std::size_t m_stopFadeRemaining = 0;

    std::atomic<Request> m_request{Request::None};
    std::atomic<State> m_state{State::Empty};
    std::atomic<bool> m_loop{true};

    // Cópias para a interface ler sem tocar nos campos da thread de áudio.
    std::atomic<std::size_t> m_publishedTakeLength{0};
    std::atomic<std::size_t> m_publishedRecorded{0};
    std::atomic<std::size_t> m_publishedPlayhead{0};
};
