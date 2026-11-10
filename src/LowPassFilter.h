#pragma once

#include <vector>

#include "AudioModule.h"

// LowPassFilter — deixa passar o grave, corta o agudo acima do corte.
//
// O ESPELHO DO HighPassFilter, E POR QUE ELE EXISTE
// Todo pedal de distorção real tem um filtro DEPOIS do clipping: o knob
// "Filter" de um RAT, o "Tone" de um Tube Screamer são passa-baixas na
// saída do estágio que distorce. Sem eles, a clipagem entrega todos os
// harmônicos que gerou — e harmônico de clipagem sobe até onde o ouvido
// alcança, o "fizz" áspero que diferencia um simulador cru de um pedal de
// verdade. O IbiFX tinha o passa-alta de ENTRADA (aperta o grave antes de
// distorcer) mas não o passa-baixa de SAÍDA; os pedais de drive da
// GearLibrary usam este módulo para completar a topologia dos circuitos que
// eles lembram.
//
// Mesma matemática RC de um polo do HighPassFilter, só que medindo o que
// FICA em vez do que VARIA:
//
//     RC = 1 / (2*pi*corte)        a memória do circuito
//     dt = 1 / sampleRate          o tempo entre amostras
//     a  = dt / (RC + dt)          quanto da amostra nova entra na média
//
//     y += a * (x - y)
//
// É uma média que persegue a entrada: o grave (variação lenta) é seguido de
// perto e passa inteiro; o agudo (variação rápida) é alisado para fora. Em
// DC o ganho é exatamente 1 — um passa-baixa não mexe no nível, só no
// brilho. 6 dB/oitava, como o filtro passivo de um pedal real.
class LowPassFilter : public AudioModule
{
public:
    LowPassFilter();

    // Frequência de corte em Hz — acima dela o sinal cai 6 dB/oitava.
    void setFrequency(float hz);
    float frequency() const;

    const char* name() const override;

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    void process(std::vector<float>& buffer) override;

private:
    double m_sampleRate = 44100.0;

    // A "média que persegue" — o estado do filtro. Só a thread de áudio
    // mexe, dentro do process().
    float m_state = 0.0f;
};
