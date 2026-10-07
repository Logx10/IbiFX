#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "MasterTransport.h"

// Metronome — clique sample-accurate, baseado no MasterTransport (Fase 15).
//
// POR QUE NÃO É UM AudioModule
// AudioModule::process() recebe só o buffer — não tem como ele saber ONDE,
// no relógio global, aquele buffer começa. Um metrônomo não pode viver sem
// essa informação: ele não transforma um sinal que chega, ele GERA um
// clique num instante absoluto do tempo da música. Por isso process() aqui
// recebe explicitamente a posição do primeiro frame do bloco.
//
// "SAMPLE-ACCURATE" SIGNIFICA ISTO
// Não bastaria soar o clique "em algum momento perto da batida", dentro da
// margem de um bloco inteiro — a 48 kHz e 128 amostras por bloco, isso é
// até 2,7 ms de incerteza, o bastante para soar fora de tempo num ouvido
// treinado. Em vez disso, cada amostra do bloco é conferida contra a
// posição exata da próxima batida (samplesPerBeat, derivado do BPM do
// transport), e o clique começa na amostra certa, nem uma antes nem depois.
//
// SOMA, NÃO SUBSTITUI
// process() SOMA o clique ao que já está no buffer. Um metrônomo convive
// com o sinal da guitarra (ou da backing track, Fase 16); ele não é um
// efeito que processa esse sinal, é outra fonte de som compartilhando a
// mesma saída.
//
// O CLIQUE ATRAVESSA BLOCOS, COMO O ECO DO Delay
// Se o clique (alguns milissegundos) não cabe inteiro no bloco em que
// começou, o resto continua no próximo process() — por isso o estado do
// clique em andamento (m_clickSampleIndex/m_clickLengthSamples) é membro
// da classe, não uma variável local.
class Metronome
{
public:
    // Guarda uma REFERÊNCIA ao transport, não uma cópia — o metrônomo lê o
    // mesmo relógio que todo o resto do projeto, nunca o seu próprio.
    explicit Metronome(const MasterTransport& transport);

    void prepare(double sampleRate);

    // Descarta o clique em andamento, sem tocar em volume/compasso.
    void reset();

    // Volume do clique, 0 a 1.
    void setVolume(float volume);
    float volume() const;

    // Quantos tempos por compasso — o primeiro tempo de cada compasso soa
    // num tom mais agudo (acentuado). Assume que a posição 0 do transport
    // é o início de um compasso; não lida com anacruse.
    void setBeatsPerBar(int beatsPerBar);
    int beatsPerBar() const;

    // Soma o clique ao buffer. startPositionSamples é a posição, no
    // relógio do MasterTransport, do PRIMEIRO frame deste buffer — quem
    // chama precisa saber essa posição de antemão (lida do transport antes
    // de avançá-lo, nunca depois).
    void process(std::vector<float>& buffer, std::uint64_t startPositionSamples);

private:
    void triggerClick(bool accented);
    float renderClickSample();

    const MasterTransport& m_transport;

    double m_sampleRate = 48000.0;
    // Atômico: a interface muda o volume com o áudio rodando.
    std::atomic<float> m_volume{0.5f};
    int m_beatsPerBar = 4;

    // O clique em andamento. m_clickLengthSamples == 0 significa nenhum
    // clique tocando agora.
    std::size_t m_clickSampleIndex = 0;
    std::size_t m_clickLengthSamples = 0;
    float m_clickFrequency = 1000.0f;

    // Índice do último tempo já disparado, para não redisparar o mesmo
    // tempo duas vezes se process() for chamado de um jeito incomum (ex.:
    // testes que repetem a mesma posição). -1 = nenhum ainda.
    std::int64_t m_lastTriggeredBeat = -1;
};
