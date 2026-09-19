#pragma once

#include <vector>

#include "AudioModule.h"
#include "SmoothedValue.h"

// Preamp — vários estágios de saturação em cascata, não um só.
//
// O PROBLEMA COM "UM SoftClipper SÓ, COM DRIVE BEM ALTO"
// Um único estágio tanh(drive * x), não importa quão alto o drive, é uma
// curva em S só: quanto mais alto o drive, mais ela colapsa pra um degrau
// (isso já é o que test_soft_clipper.cpp prova, em
// testExtremeDriveApproachesHardClipping). Um preamp de válvula de verdade
// não soa assim — ele usa 2 a 5 estágios (triodos) em CASCATA, cada um
// levemente saturado, e é a SOMA de várias curvas levemente não-lineares em
// série — não uma curva única muito não-linear — que produz a densidade
// harmônica e o sustain característicos de um amplificador high-gain.
//
// A ESTRUTURA
//
//     entrada -> [tanh(drive*x) -> ganho de reexpansão] x2 -> tanh(drive*x) -> saída
//                └──────────── estágio 1 e 2 ────────────┘      estágio 3
//
// Cada estágio aplica a MESMA curva do SoftClipper. A diferença é o que
// acontece ENTRE os estágios: o sinal, já comprimido pela tanh anterior,
// é reexpandido por kInterStageBoost antes de entrar no próximo estágio.
// Sem essa reexpansão, o segundo estágio receberia um sinal já perto de
// ±1 — a parte quase reta (achatada) da tanh — e mal faria diferença: a
// cascata colapsaria em "quase a mesma coisa que um estágio só". É a
// reexpansão entre estágios que dá a cada um trabalho de verdade pra fazer,
// e é exatamente o papel que o ganho de cada estágio de válvula cumpre num
// amplificador real.
//
// POR QUE A SAÍDA CONTINUA GARANTIDAMENTE EM [-1, +1]
// Só o ÚLTIMO estágio não tem reexpansão depois dele — a saída final é
// sempre um tanh(...) puro, sem multiplicação depois. Os estágios do meio
// podem ultrapassar ±1 internamente (não tem problema, é sinal
// intermediário, ninguém ouve ele diretamente), mas o que sai do módulo é
// sempre o resultado de uma tanh — nunca escapa da faixa, pela mesma razão
// matemática do SoftClipper.
//
// O QUE ISSO FAZ COM A DINÂMICA DO TOQUE
// Um sinal fraco (tocando leve) ainda ganha volume real através da
// cascata — cada estágio tem ganho, não é neutro — mas fica relativamente
// menos comprimido que um sinal forte, porque entra na parte mais reta da
// primeira tanh. Um sinal forte já satura no primeiro estágio e chega
// espremido nos seguintes. É essa diferença de COMPRESSÃO RELATIVA entre
// tocar fraco e forte — não a ausência de compressão no sinal fraco — que
// caracteriza a resposta ao toque de um amplificador com múltiplos estágios
// de ganho.
//
// TRÊS ESTÁGIOS, FIXO POR ENQUANTO
// Não é um parâmetro ainda — 3 é um número típico de estágios de
// preamplificação num amplificador de guitarra real, e abrir isso como
// parâmetro é fácil de fazer depois, quando houver um motivo concreto pra
// precisar de mais ou menos.
class Preamp : public AudioModule
{
public:
    Preamp();

    // Quanto cada estágio empurra o sinal pra dentro da curva. Aplicado
    // igualmente aos 3 estágios.
    void setDrive(float newDrive);
    float drive() const;

    const char* name() const override;

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    void process(std::vector<float>& buffer) override;

private:
    SmoothedValue m_smoothedDrive;
};
