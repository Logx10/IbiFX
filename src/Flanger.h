#pragma once

#include <cstddef>
#include <vector>

#include "AudioModule.h"
#include "SmoothedValue.h"

// Flanger — uma cópia do sinal com atraso CURTO e variável, somada ao
// original, com parte da saída voltando para a entrada.
//
// O FILTRO PENTE
// Somar um sinal com uma cópia atrasada em τ segundos cancela as
// frequências em que o atraso é meio ciclo (f = 1/(2τ), 3/(2τ), ...) e
// reforça as que caem em ciclos inteiros (1/τ, 2/τ, ...). O espectro vira
// um pente de vales igualmente espaçados. Com τ = 1 ms, os vales ficam em
// 500 Hz, 1,5 kHz, 2,5 kHz... Quando o LFO estica e encolhe τ, o pente
// inteiro desliza: é o "avião passando" do flanger.
//
// FLANGER x CHORUS
// É a mesma máquina do Chorus (linha de atraso + LFO + interpolação). A
// diferença é o tamanho do atraso: abaixo de ~5 ms o ouvido não ouve uma
// segunda voz, ouve o pente. E a realimentação: o Chorus não tem.
//
// OS NÚMEROS: OS DO ELECTRIC MISTRESS
// Medições de um Deluxe Electric Mistress (o flanger do John Frusciante)
// dão atraso de ~0,8 a 6 ms com o Range no máximo, e LFO triangular de
// ~0,07 a 10 Hz. Os três knobs do original:
// - Rate: velocidade do LFO.
// - Range: até onde o atraso vai. Em 0 a varredura é estreita, perto de
//   0,8 ms (pente agudo); no máximo vai a 6 ms, levando os vales para o
//   grave.
// - Color: a realimentação. Mais Color = vales mais fundos e picos
//   ressonantes entre eles — o som "metálico" característico.
//
// REALIMENTAÇÃO E ESTABILIDADE
// O que entra na linha é entrada + color · (saída atrasada). Com |color| <
// 1, cada volta pela linha fica menor e a ressonância morre; em 1 ela não
// morreria nunca. O máximo do parâmetro é 0,9.
//
// MISTURA FIXA EM 50%
// Como no original, que não tem knob de mix: é a soma meio a meio que
// produz os vales mais fundos.
class Flanger : public AudioModule
{
public:
    Flanger();

    void setRate(float hertz);
    float rate() const;

    void setRange(float amount);
    float range() const;

    void setColor(float amount);
    float color() const;

    const char* name() const override;

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    void process(std::vector<float>& buffer) override;

private:
    double m_sampleRate = 44100.0;
    double m_phase = 0.0;

    std::vector<float> m_circular;
    std::size_t m_writePosition = 0;

    SmoothedValue m_smoothedRange;
    SmoothedValue m_smoothedColor;
};
