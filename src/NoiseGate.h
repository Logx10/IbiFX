#pragma once

#include <vector>

#include "AudioModule.h"

// NoiseGate — silencia o sinal quando ele cai abaixo de um piso.
//
// O PROBLEMA QUE ELE RESOLVE
// Toda captação de guitarra carrega um pouco de ruído de fundo: zumbido de
// captador, chiado da interface, hum de 60Hz da rede elétrica. Parado, esse
// ruído é quase inaudível. Mas a cadeia padrão tem Gain 6.0x e um SoftClipper
// em cima — os dois amplificam TUDO que passa por eles, ruído incluso. O
// resultado é um chiado perceptível entre os acordes: some quando você toca
// (a nota encobre o ruído) e reaparece no silêncio.
//
// COMO RESOLVE
// Mede o nível do sinal e fecha (multiplica por algo perto de 0) quando ele
// cai abaixo de um threshold; abre (multiplica por perto de 1) quando sobe
// de novo. A guitarra soa igual enquanto toca — o gate só age no silêncio
// entre as notas.
//
// POR QUE FICA NO INÍCIO DA CADEIA
// Se viesse depois do Gain/SoftClipper, o ruído já teria sido amplificado e
// distorcido antes de o gate decidir se corta — e amplificado, o nível do
// ruído pode passar do threshold, e o gate nem percebe que é ruído. Fechando
// ANTES do ganho, o ruído nunca chega a ser amplificado.
//
// ATAQUE RÁPIDO, LIBERAÇÃO LENTA
// Abrir precisa ser quase instantâneo, ou o ataque da nota — o transiente
// mais importante pro timbre — sai cortado. Fechar precisa ser gradual, ou a
// cauda de uma nota decaindo é cortada no meio, criando o corte audível
// conhecido como "gate chatter" em vez de um silêncio natural. Por isso
// existem dois coeficientes diferentes: kAttackSeconds, fixo e rápido, e
// release, ajustável e mais lento — ver prepare() e process().
//
// POR QUE m_gain E NÃO UM ON/OFF DIRETO
// Multiplicar a amostra por 0 ou por 1 sem transição criaria a mesma
// descontinuidade que motivou o SmoothedValue nos outros módulos: um degrau
// na forma de onda soa como clique. m_gain caminha entre 0 e 1 amostra a
// amostra, por um filtro de um polo — não um SmoothedValue, porque o alvo
// aqui muda a cada amostra dependendo do NÍVEL DO SINAL, e não uma vez por
// ajuste de controle como um parâmetro comum.
//
// threshold e release NÃO são suavizados como os parâmetros de outros
// módulos: eles não multiplicam a amostra diretamente, só decidem para onde
// m_gain caminha e a que velocidade. Quem multiplica a amostra é m_gain, e
// esse sim caminha suavemente, sempre.
//
// A DECISÃO É TOMADA EM CIMA DE UM ENVELOPE, NÃO DA AMOSTRA CRUA
// Um sinal de guitarra oscila: mesmo numa nota bem acima do threshold, cada
// CICLO da onda passa perto de zero algumas vezes. Comparar a amostra crua
// contra o threshold faz o gate enxergar esses cruzamentos de zero como
// silêncio, abrindo e fechando a cada ciclo em vez de uma vez por nota — o
// resultado é a própria nota sendo picotada no meio dela, não só o silêncio
// entre notas sendo cortado (fica mais audível ainda com o Delay logo
// depois na cadeia, porque a cauda gravada herda os mesmos picotes).
//
// m_envelope segue o NÍVEL do sinal (como m_envelope em Compressor.cpp, que
// já resolvia isso) — só ele é comparado contra o threshold. m_gain
// continua sendo quem de fato multiplica a amostra, suavizado como sempre.
class NoiseGate : public AudioModule
{
public:
    NoiseGate();

    // Nível de amplitude abaixo do qual o gate começa a fechar.
    void setThreshold(float newThreshold);
    float threshold() const;

    // Segundos até fechar completamente depois que o sinal cai abaixo do
    // threshold.
    void setRelease(float seconds);
    float release() const;

    const char* name() const override;

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    void process(std::vector<float>& buffer) override;

private:
    double m_sampleRate = 44100.0;

    // 1.0 = totalmente aberto, 0.0 = totalmente fechado. Nasce fechado: no
    // instante em que o áudio começa a rodar ainda não sabemos se há sinal
    // de verdade, e silêncio é a suposição mais segura.
    float m_gain = 0.0f;

    // Coeficiente do filtro de um polo para ABRIR o gate. Fixo — ver o
    // comentário em Ataque Rápido, Liberação Lenta acima sobre por que não é
    // um parâmetro ainda.
    float m_attackCoeff = 1.0f;

    // Nível de sinal suavizado — é ELE que é comparado contra o threshold,
    // nunca a amostra crua. Ver o comentário "A decisão é tomada em cima de
    // um envelope" acima.
    float m_envelope = 0.0f;

    // Coeficiente do filtro de um polo do envelope. Fixo, assim como
    // m_attackCoeff — ver kEnvelopeSeconds em NoiseGate.cpp.
    float m_envelopeCoeff = 1.0f;
};
