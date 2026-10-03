#pragma once

#include <atomic>
#include <cstddef>
#include <string>

#include "Recorder.h"

// ReampRecorder — grava DUAS tracks em sincronia: a seca (DI, antes da
// cadeia) e a processada (depois dela). Fase 18: "DI + sinal processado em
// tracks separadas".
//
// PRA QUE SERVE GRAVAR O SINAL SECO
// "Reamp" é reprocessar depois: gravar a guitarra limpa agora, e mais
// tarde tocar essa gravação de volta por uma cadeia DIFERENTE (outro
// ampli, outro drive, outro cabinet) sem precisar tocar a música nem uma
// vez de novo. Isso só é possível se a versão seca foi preservada — o
// sinal processado sozinho já perdeu a informação (não dá para "destilar"
// de volta o que uma distorção já saturou).
//
// POR QUE É SÓ UMA COMPOSIÇÃO DE DOIS Recorder
// As duas tracks não têm NENHUMA necessidade uma da outra além de
// começarem e pararem juntas — cada uma é, por si só, exatamente a
// gravação de uma track da Fase 17. Em vez de duplicar a lógica de buffer
// circular e thread de disco, esta classe só possui dois Recorder e os
// aciona em par.
//
// A SINCRONIA VEM DE pushBlock() RECEBER OS DOIS BUFFERS JUNTOS
// Se o chamador empurrasse o seco e o processado em duas chamadas
// separadas, um bloco poderia ficar pendente no buffer circular de um
// Recorder e já ter sido drenado no outro, por puro acaso de
// escalonamento — as duas tracks sairiam de tamanhos diferentes. Uma
// chamada só, com os dois ponteiros, garante que a amostra N de uma
// corresponde exatamente à amostra N da outra.
//
// POR QUE EXISTE m_active EM VEZ DE PERGUNTAR A CADA Recorder
// Um bug de verdade apareceu aqui, pego pelo teste de concorrência, não
// por inspeção: a primeira versão de stop() chamava
// m_dryRecorder.stop() (que BLOQUEIA num join) e só depois
// m_processedRecorder.stop(). Nesse intervalo — que pode durar o tempo
// inteiro de a thread de disco do primeiro escrever o arquivo —, um
// pushBlock() em andamento na thread de áudio podia ver o primeiro JÁ
// parado e o segundo AINDA gravando, e empurrar só pra um dos dois. As
// duas tracks saíam de tamanhos diferentes, e isso só se manifestava às
// vezes, dependendo de escalonamento — exatamente o tipo de falha
// intermitente que concorrência mal feita produz.
//
// A correção: um único sinalizador (m_active) é a fonte de verdade que
// pushBlock() consulta UMA VEZ; e stop() pede a PARADA (requestStop(),
// que só sinaliza, não bloqueia) nos dois Recorder antes de esperar
// (finishStop()) qualquer um dos dois. Isso encolhe a janela de corrida
// de "o tempo de um join inteiro" para "duas instruções atômicas
// consecutivas" — não zero matematicamente, mas por uma margem de várias
// ordens de grandeza, o mesmo tipo de tolerância que o projeto já aceita
// em Parameter (um valor atômico lido/escrito por threads diferentes,
// onde o pior caso é "um bloco de atraso", não corrupção).
class ReampRecorder
{
public:
    ReampRecorder() = default;

    void prepare(double sampleRate);

    // Começa as duas gravações. Lança se qualquer uma já estiver gravando
    // (mesma regra de Recorder::start()).
    void start(const std::string& dryPath, const std::string& processedPath);

    // Para as duas, cada uma escrevendo seu próprio arquivo.
    void stop();

    bool isRecording() const;

    // Chamada da THREAD DE ÁUDIO, uma vez por bloco processado — mesmo
    // contrato de Recorder::pushSamples() (não bloqueia, não aloca), só
    // que alimentando as duas tracks com o MESMO frameCount. Sem efeito
    // se não estiver gravando.
    void pushBlock(const float* dry, const float* processed, std::size_t frameCount);

private:
    std::atomic<bool> m_active{false};

    Recorder m_dryRecorder;
    Recorder m_processedRecorder;
};
