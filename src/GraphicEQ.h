#pragma once

#include <array>
#include <vector>

#include "AudioModule.h"

// GraphicEQ — equalizador gráfico de 7 bandas, no espírito dos EQs de pedal
// (100 Hz a 6,4 kHz, uma oitava entre cada banda, ±15 dB cada).
//
// DIFERENTE DO ToneStack, AQUI CADA BANDA É SURDA ÀS OUTRAS
// O ToneStack resolve um circuito passivo em que os três knobs interagem.
// Um EQ gráfico é o oposto de propósito: cada controle mexe numa faixa só,
// e com todos em 0 dB o sinal passa intacto. Por isso ele é montado como
// sete filtros independentes em série — a saída de um é a entrada do
// próximo, e como cada um só altera a sua região, a ordem não importa.
//
// O FILTRO DE CADA BANDA: PEAKING (OU "BELL")
// Um filtro peaking aumenta ou corta uma região em volta de uma frequência
// central e deixa o resto em 0 dB — o desenho de sino que dá o nome. É um
// biquad: filtro de 2ª ordem, 2 amostras de histórico de entrada e 2 de
// saída (o ToneStack é de 3ª, o HighPassFilter de 1ª).
//
//     y[n] = b0 x[n] + b1 x[n-1] + b2 x[n-2] - a1 y[n-1] - a2 y[n-2]
//
// FONTE DOS COEFICIENTES
// Robert Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter
// coefficients" — a referência padrão para esses filtros. Para ganho G em
// dB, frequência central f0 e largura Q:
//
//     A = 10^(G/40)       w0 = 2π f0 / sampleRate     alpha = sin(w0) / (2Q)
//
//     b0 = 1 + alpha*A    b1 = -2 cos(w0)    b2 = 1 - alpha*A
//     a0 = 1 + alpha/A    a1 = -2 cos(w0)    a2 = 1 - alpha/A
//
// (todos divididos por a0 no fim). Com G = 0, A = 1 e numerador fica igual
// ao denominador: o filtro vira identidade, que é o "flat" do pedal.
// O expoente é /40, e não /20 como no resto do projeto, porque o ganho no
// centro sai de A² — metade vem do numerador, metade do denominador.
//
// Q = 1.4: A LARGURA DE UMA OITAVA
// Com as bandas a uma oitava uma da outra, Q ≈ 1,41 faz cada sino ter uma
// oitava de largura, e os vizinhos se encontram no meio do caminho. Mais
// estreito deixaria "buracos" entre as bandas; mais largo, cada knob
// arrastaria muito os vizinhos. Como os sinos se sobrepõem, subir duas
// bandas vizinhas soma um pouco no meio delas — como num EQ gráfico
// analógico, é uma característica, não um erro.
//
// LEVEL
// O oitavo controle, em dB, é o volume de saída: subir várias bandas aumenta
// o volume total, e o Level compensa sem precisar de um GainProcessor depois.
//
// LIMITAÇÃO DESTA PRIMEIRA VERSÃO
// Coeficientes recalculados uma vez por bloco, sem suavização — a mesma
// LIMITAÇÃO documentada no ToneStack, e pelo mesmo motivo.
class GraphicEQ : public AudioModule
{
public:
    static constexpr std::size_t kBandCount = 7;

    GraphicEQ();

    // Ganho de uma banda, em dB (-15 a +15). band vai de 0 (100 Hz) a 6
    // (6,4 kHz).
    void setBandGain(std::size_t band, float gainDb);
    float bandGain(std::size_t band) const;

    // Frequência central de uma banda, em Hz.
    static float bandFrequency(std::size_t band);

    // Volume de saída, em dB (-15 a +15).
    void setLevel(float levelDb);
    float level() const;

    const char* name() const override;

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    void process(std::vector<float>& buffer) override;

private:
    // Um biquad: coeficientes já divididos por a0, mais o histórico.
    struct Band
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;

        double x1 = 0.0, x2 = 0.0;
        double y1 = 0.0, y2 = 0.0;
    };

    // Recalcula os coeficientes das sete bandas a partir dos ganhos atuais.
    void updateCoefficients();

    double m_sampleRate = 44100.0;
    std::array<Band, kBandCount> m_bands;
};
