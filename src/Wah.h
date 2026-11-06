#pragma once

#include <vector>

#include "AudioModule.h"
#include "SmoothedValue.h"

// Wah — um filtro passa-banda ressonante cujo centro anda com o pedal.
//
// O QUE O PEDAL FAZ
// Por dentro, um wah é um filtro que deixa passar uma faixa estreita de
// frequências e realça o meio dela. O pedal (position) move essa faixa:
// calcanhar embaixo = centro grave ("uuu"), ponta embaixo = centro agudo
// ("iii"). Mexer o pé no tempo da música é o "uá" que dá o nome.
//
// A FAIXA E O PICO: OS DO IBANEZ WH-10
// O WH-10 (o wah do John Frusciante) varre de 350 Hz a 2,2 kHz. Diferente
// do Cry Baby, que usa um indutor e tem um pico agudo e estreito, o WH-10 é
// um filtro de múltipla realimentação com amplificador operacional: pico
// mais largo e menos "espetado". Por isso o Q aqui é um parâmetro
// (resonance) — o padrão 3 lembra o WH-10; ~6 já soa como um Cry Baby.
//
// A VARREDURA É EXPONENCIAL
// O ouvido percebe frequência em oitavas, não em Hz. Se o centro andasse
// em linha reta de 350 a 2200 Hz, quase todo o curso do pedal soaria
// "agudo". Com f = 350 · (2200/350)^position, cada pedaço do curso anda o
// mesmo número de oitavas — é como o potenciômetro de um wah real age.
//
// O FILTRO: SVF NA FORMA "TPT" (ZAVALISHIN)
// Um state-variable filter calcula, ao mesmo tempo, passa-baixa, passa-banda
// e passa-altas a partir de dois integradores. A forma TPT (topology-
// preserving transform, de Vadim Zavalishin, "The Art of VA Filter Design")
// discretiza os integradores pela transformada bilinear e continua estável
// mesmo quando a frequência muda a cada amostra — exatamente o que um wah
// faz. Um biquad comum, com os coeficientes trocados no meio do som, pode
// estalar ou disparar.
//
//     g = tan(π·fc/fs)          k = 1/Q
//     v3 = x - ic2
//     v1 = (ic1 + g·v3) / (1 + g·(g + k))      -> passa-banda (pico = Q)
//     v2 = ic2 + g·v1                           -> passa-baixa
//     ic1 = 2·v1 - ic1         ic2 = 2·v2 - ic2 (estado dos integradores)
//
// A SAÍDA
// k·v1 é o passa-banda com pico exatamente em 0 dB. Um wah real realça o
// pico acima do resto, então a saída é o passa-banda mais um pouco do
// passa-baixa (o "corpo" que o pedal real deixa vazar):
//
//     saída = k·v1·kPeakGain + v2·kLowpassLeak
class Wah : public AudioModule
{
public:
    Wah();

    // Posição do pedal: 0 = calcanhar (350 Hz), 1 = ponta (2,2 kHz).
    void setPosition(float position);
    float position() const;

    // Q do filtro: 1,5 (largo) a 10 (estreito e "vocal").
    void setResonance(float q);
    float resonance() const;

    // Frequência central para uma posição do pedal, em Hz.
    static float centerFrequency(float position);

    const char* name() const override;

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    void process(std::vector<float>& buffer) override;

private:
    double m_sampleRate = 44100.0;

    // A posição é suavizada: um pedal de expressão via MIDI chega em
    // degraus de 1/127, e cada degrau seria um estalo.
    SmoothedValue m_smoothedPosition;

    // Estado dos dois integradores do SVF.
    float m_ic1 = 0.0f;
    float m_ic2 = 0.0f;
};
