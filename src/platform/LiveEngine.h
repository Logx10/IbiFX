#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "AudioDevice.h"
#include "ModuleChain.h"
#include "PracticeSession.h"
#include "ReampRecorder.h"
#include "Tuner.h"

// LiveEngine — liga o dispositivo de áudio à cadeia de módulos.
//
// O QUE ELE RESOLVE
// O driver entrega as amostras INTERCALADAS (L R L R L R...), enquanto os
// módulos processam um canal por vez, num std::vector<float>. Alguém precisa
// converter entre os dois formatos — e não pode alocar para isso, porque a
// conversão acontece dentro do callback.
//
// Este é esse alguém. Os buffers de trabalho nascem no start(), no domínio de
// controle, e o callback apenas os reutiliza.
//
// MONO POR DENTRO
// A cadeia é mono, como uma pedaleira de guitarra. O sinal usado é o do
// primeiro canal de entrada, e o resultado é copiado para todos os canais de
// saída. Não é uma limitação a corrigir depois — é o formato certo para o
// instrumento: um cabo de guitarra carrega um sinal só.
//
// Efeitos genuinamente estéreo, como um ping-pong delay, precisariam de outro
// desenho, e esse desenho depende de decidir antes o que "estéreo" significa
// para cada módulo.
//
// SEM ENTRADA, SILÊNCIO
// No modo de só reprodução não há sinal de entrada, e o engine alimenta a
// cadeia com zeros. Isso não é inútil: um delay com eco pendente continua
// devolvendo som, e é assim que dá para ouvir a cauda de um efeito.
//
// COMO MUDAR ALGO ENQUANTO TOCA
// Não chame add(), remove() nem move() com o dispositivo rodando: elas
// realocam o vetor de módulos e a thread de áudio estaria percorrendo ele.
// Para mudanças durante o som, use os métodos abaixo, que passam pela fila de
// comandos e nunca bloqueiam.
class LiveEngine
{
public:
    // O engine é dono de um dispositivo com thread ativa e de uma cadeia com
    // índices atômicos. Nada disso deve ser copiado ou movido.
    LiveEngine() = default;
    LiveEngine(const LiveEngine&) = delete;
    LiveEngine& operator=(const LiveEngine&) = delete;
    LiveEngine(LiveEngine&&) = delete;
    LiveEngine& operator=(LiveEngine&&) = delete;

    // A cadeia processada. Monte-a ANTES de chamar start().
    ModuleChain& chain();
    const ModuleChain& chain() const;

    // Abre o dispositivo, prepara os módulos com o sample rate negociado e
    // começa a processar. Devolve false em caso de falha.
    bool start(AudioDevice::Mode mode = AudioDevice::Mode::Duplex,
               double sampleRate = 48000.0,
               int blockSize = 128);

    void stop();

    bool isRunning() const;

    // Ajusta um parâmetro com o áudio rodando, pela fila de comandos.
    // Devolve false se a fila estiver cheia.
    bool setParameter(std::size_t moduleIndex, std::size_t parameterIndex, float value);

    // Liga ou desliga o bypass de um módulo com o áudio rodando.
    bool setBypassed(std::size_t moduleIndex, bool bypassed);

    // Descarta o estado acumulado de um módulo com o áudio rodando.
    bool resetModule(std::size_t moduleIndex);

    // Pico do sinal antes e depois da cadeia, para medidores de nível.
    //
    // Escritos pela thread de áudio, lidos pela interface — atômicos pelo
    // mesmo motivo do Parameter. O valor decai sozinho a cada bloco: um
    // medidor que só sobe ficaria travado no maior pico de sempre.
    float inputPeak() const;
    float outputPeak() const;

    // Informações do dispositivo negociado.
    double sampleRate() const;
    std::size_t channelCount() const;
    std::string deviceName() const;
    std::string captureDeviceName() const;
    std::string lastError() const;
    std::size_t processedBlocks() const;

    // Repassados do AudioDevice — ver o comentário lá sobre o que significam
    // e por que orçamento de bloco é o jeito certo de olhar para latência.
    std::uint64_t lastCallbackMicros() const;
    std::uint64_t maxCallbackMicros() const;
    std::size_t overBudgetBlocks() const;
    std::size_t rerouteCount() const;
    std::size_t interruptionCount() const;

    // Acesso ao gravador de reamp (Fase 18) — start()/stop() são chamados
    // do domínio de controle. Sempre existe, mesmo sem gravar nada: é por
    // isso que não há um setReampRecorder() trocando um ponteiro enquanto
    // o áudio roda — isso teria a mesma corrida de trocar a cadeia (ver
    // ModuleChain.h). pushBlock() já não faz nada quando não está
    // gravando, então tê-lo sempre presente custa só uma checagem atômica
    // por bloco.
    ReampRecorder& reampRecorder();

    // Acesso à sessão de prática (Fase 19) — play()/loadBackingTrack()/
    // etc são chamados do domínio de controle. Mesma política do
    // reampRecorder(): sempre existe, e processar sem nada carregado nem
    // nada gravando custa só algumas checagens baratas por bloco.
    //
    // A ORDEM EM processBlock() IMPORTA: a sessão de prática mistura
    // backing track e clique DEPOIS do ReampRecorder já ter capturado o
    // sinal processado — senão a track "processada" do reamp sairia
    // contaminada com a faixa de apoio e o clique, que não fazem parte do
    // que se quer reamplificar depois.
    PracticeSession& practiceSession();

    // Afinador sempre à mão, sem precisar entrar na cadeia. Ele escuta o
    // sinal SECO (antes dos pedais), como um afinador de verdade no começo
    // do pedalboard — uma distorção na frente bagunçaria a leitura.
    //
    // Desligado por padrão: a análise do YIN custa CPU na thread de áudio
    // (ver Tuner.h), então só roda enquanto alguém está olhando para ele.
    // Com mute ligado, a saída fica em silêncio enquanto o afinador está
    // ativo — afinar sem o som passar pelo resto da cadeia.
    void setTunerEnabled(bool enabled);
    bool isTunerEnabled() const;
    void setTunerMuted(bool muted);
    bool isTunerMuted() const;
    const Tuner& tuner() const;

private:
    // Prepara cadeia, gravador, prática e afinador numa taxa. Só com o
    // dispositivo parado — ver start().
    void prepareAll(double sampleRate, int blockSize);

    // Chamado na thread de áudio.
    void processBlock(float* output, const float* input, std::size_t frameCount, std::size_t channelCount);

    AudioDevice m_device;
    ModuleChain m_chain;
    ReampRecorder m_reampRecorder;
    PracticeSession m_practiceSession;
    Tuner m_tuner;

    std::atomic<bool> m_tunerEnabled{false};
    std::atomic<bool> m_tunerMuted{false};

    // Só a thread de áudio lê e escreve: detecta a borda de "acabou de
    // ligar" para zerar a janela do afinador ali mesmo, sem que a interface
    // precise chamar reset() de outra thread.
    bool m_tunerWasEnabled = false;

    // Buffer mono de trabalho, alocado no start(). O callback só muda o
    // tamanho lógico dele, nunca a capacidade — então não aloca.
    std::vector<float> m_monoBuffer;

    // Cópia do sinal ANTES da cadeia, para o ReampRecorder e o afinador —
    // só preenchida quando um dos dois precisa dela (ver processBlock()). Mesma política de
    // alocação do m_monoBuffer: reservado no start(), nunca redimensionado
    // além da capacidade dentro do callback.
    std::vector<float> m_dryBuffer;

    std::atomic<float> m_inputPeak{0.0f};
    std::atomic<float> m_outputPeak{0.0f};
};
