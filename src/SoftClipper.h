#pragma once

#include <vector>

#include "AudioModule.h"
#include "SmoothedValue.h"

// SoftClipper — saturação suave via tanh.
//
// Comprime o sinal gradualmente à medida que ele se aproxima do limite, em
// vez de decepá-lo na quina como o Clipper.
//
//     hard clipping (Clipper)      soft clipping (este módulo)
//
//        _______                        ______
//       /       \                     /        \
//      |         |                   |          |
//     -+---------+-               ---+----------+---
//      |         |                   |          |
//       \_______/                     \________/
//
// A quina viva gera harmônicos agudos que se estendem muito acima da
// fundamental: é o som de fuzz. A curva suave também distorce, mas seus
// harmônicos decaem rápido — é o som de overdrive valvulado.
//
// TODO DISTORCEDOR É UMA FUNÇÃO DE TRANSFERÊNCIA, aplicada amostra a amostra:
//
//     GainProcessor    f(x) = gain * x            reta
//     Clipper          f(x) = corta em ±teto      reta com dois platôs
//     SoftClipper      f(x) = tanh(drive * x)     curva em S
//
// tanh serve porque perto de zero é quase uma reta (sinal fraco passa
// intacto), longe de zero achata (sinal forte comprime) e nunca ultrapassa
// ±1 por definição matemática — não existe estouro possível, sem nenhum if.
//
// Da primeira propriedade nasce a dinâmica de toque: tocando leve o sinal
// fica na parte reta e sai limpo; atacando forte ele sobe para a curva e
// satura. É o que se chama de "responder ao toque" num amplificador.
//
// Com drive muito alto a curva colapsa num degrau e vira hard clipping. Os
// dois módulos são pontos da mesma linha, não efeitos distintos.
//
// O drive é suavizado: girá-lo de uma vez mudaria o formato da curva entre
// duas amostras vizinhas, o que soa como clique. Sem prepare(), a suavização
// fica inativa.
class SoftClipper : public AudioModule
{
public:
    SoftClipper();

    // Define o quanto o sinal é empurrado para dentro da curva. Drive maior
    // distorce mais. Atenção: 1.0 NÃO é neutro — a curva sempre colore, pois
    // este módulo é um distorcedor e não um limitador.
    void setDrive(float newDrive);

    // Devolve o drive atual.
    float drive() const;

    const char* name() const override;

    // Configura a rampa de suavização para este sample rate.
    void prepare(double sampleRate, int blockSize) override;

    // Salta o drive para o valor de destino, sem rampa.
    void reset() override;

    // Aplica tanh(drive * amostra) a todas as amostras do buffer.
    void process(std::vector<float>& buffer) override;

private:
    SmoothedValue m_smoothedDrive;
};
