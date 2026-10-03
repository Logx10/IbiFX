#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "BackingTrackPlayer.h"
#include "MasterTransport.h"
#include "Metronome.h"
#include "Recorder.h"

// PracticeSession — Fase 19: "backing track + metrônomo + loop + gravação
// integrados".
//
// NENHUM CONCEITO NOVO AQUI
// Esta fase não é DSP — é ORQUESTRAÇÃO do que as Fases 14 a 17 já
// construíram. MasterTransport dá o relógio compartilhado; BackingTrackPlayer
// e Metronome já sabem ler uma posição absoluta dele e somar o que for seu
// ao buffer; Recorder já sabe gravar um sinal num .wav sem travar a thread
// de áudio. PracticeSession só os mantém juntos, na ordem certa, e expõe
// isso como uma coisa só — tocar junto com uma faixa de apoio, com clique,
// em loop, e poder gravar a sessão.
//
// O QUE process() ESPERA RECEBER
// O buffer que chega em process() já deve ser o sinal da GUITARRA
// PROCESSADO pelo pedalboard — a mesma cadeia de sempre roda antes, em
// outro lugar (LiveEngine). Esta classe só SOMA a backing track e o
// clique por cima dele, exatamente como Metronome e BackingTrackPlayer já
// fazem sozinhos.
//
// A GRAVAÇÃO CAPTURA O RESULTADO MISTURADO, CLIQUE INCLUSO
// Por simplicidade: start Recording grava o buffer depois de somar backing
// track e metrônomo, não o sinal seco da guitarra sozinho (isso já existe,
// separadamente, no ReampRecorder da Fase 18, para quem quiser reamp em
// vez de uma gravação de prática). Quem quiser revisar sem o clique
// audível pode desligar o metrônomo antes de gravar.
//
// POR QUE O TRANSPORT É PRIVADO DESTA CLASSE
// Nenhuma outra fase até agora precisa compartilhar o MESMO relógio com o
// modo prática ao mesmo tempo — é uma sessão autocontida. Se isso mudar
// no futuro (por exemplo, um relógio único para o app inteiro), trocar
// este membro por uma referência é a mudança mínima necessária.
class PracticeSession
{
public:
    // A ORDEM DE DECLARAÇÃO IMPORTA: m_transport precisa existir antes de
    // m_metronome, que guarda uma referência a ele — C++ inicializa
    // membros na ordem em que são declarados, não na ordem do construtor.
    PracticeSession();

    void prepare(double sampleRate);

    // --- Backing track ---
    void loadBackingTrack(const std::string& path);
    void setBackingTrackVolume(float volume);
    bool hasBackingTrack() const;

    // --- Transporte (delega ao MasterTransport interno) ---
    void play();
    void stop();
    bool isPlaying() const;
    void seek(std::uint64_t positionSamples);
    void setLoop(std::uint64_t startSamples, std::uint64_t endSamples);
    void setLoopEnabled(bool enabled);
    std::uint64_t positionSamples() const;

    // --- Metrônomo ---
    void setBpm(float bpm);
    float bpm() const;
    void setMetronomeEnabled(bool enabled);
    bool isMetronomeEnabled() const;
    void setMetronomeVolume(float volume);

    // --- Gravação da sessão ---
    void startRecording(const std::string& path);
    void stopRecording();
    bool isRecording() const;

    // Chamada da THREAD DE ÁUDIO, uma vez por bloco processado. `buffer`
    // já contém o sinal da guitarra processado pelo pedalboard — esta
    // chamada soma a backing track e o clique (se habilitado), grava o
    // resultado (se estiver gravando) e avança o relógio interno em
    // buffer.size() amostras.
    void process(std::vector<float>& buffer);

private:
    MasterTransport m_transport;
    BackingTrackPlayer m_backingTrack;
    Metronome m_metronome;
    Recorder m_recorder;

    bool m_metronomeEnabled = true;
};
