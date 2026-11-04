#pragma once

#include <array>
#include <vector>

#include "AudioModule.h"
#include "SmoothedValue.h"

// Phaser — quatro filtros all-pass em série, varridos por um LFO e
// misturados meio a meio com o sinal seco.
//
// COMO UM FILTRO QUE "DEIXA TUDO PASSAR" CRIA UM EFEITO
// Um all-pass não muda o volume de nenhuma frequência — só o ATRASO de
// fase de cada uma. Sozinho, é inaudível. Somado com o sinal seco, as
// frequências em que o atraso deu exatamente meia volta (180°) chegam
// invertidas e se cancelam: viram vales (notches) no espectro. Quando o
// LFO move os filtros, os vales andam — é o "uóóóm" do phaser.
//
// POR QUE QUATRO ESTÁGIOS = DOIS VALES (MXR PHASE 90)
// Um all-pass de 1ª ordem com frequência de corte fc atrasa a fase em
// φ(f) = 2·atan(f/fc): 90° em fc, chegando a 180° só no infinito. Com
// quatro iguais em série, o total vai de 0 a 720°, passando por 180° e por
// 540° — dois cancelamentos, dois vales:
//
//     180° -> cada estágio 45°  -> f = fc · tan(22,5°) = 0,414 · fc
//     540° -> cada estágio 135° -> f = fc · tan(67,5°) = 2,414 · fc
//
// A análise da ElectroSmash mede os vales do Phase 90 em 58 Hz e 340 Hz no
// extremo grave da varredura: as duas contas acima dão fc ≈ 141 Hz, que é
// exatamente o limite que os resistores de 24k em paralelo com os JFETs
// impõem. É o ponto de partida aqui; o LFO sobe fc até kHighestHz.
//
// REALIMENTAÇÃO
// O Phase 90 "script" (o original) não tem; o "block logo" tem, por um
// resistor que devolve parte da saída dos all-pass para a entrada, e deixa
// os vales mais fundos e um pico ressonante entre eles. feedback = 0 é o
// script, ~0,4 é o block logo.
//
// A VARREDURA É EXPONENCIAL, COMO NO Rotovibe
// fc anda em oitavas (log), para o vale gastar o mesmo tempo em cada
// região do espectro. O LFO é um seno.
class Phaser : public AudioModule
{
public:
    Phaser();

    // Velocidade do LFO, em Hz (o único knob do Phase 90).
    void setRate(float hertz);
    float rate() const;

    // Quanto do sinal dos all-pass volta para a entrada (0 a 0,7).
    void setFeedback(float amount);
    float feedback() const;

    const char* name() const override;

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    void process(std::vector<float>& buffer) override;

private:
    double m_sampleRate = 44100.0;
    double m_phase = 0.0;

    // Último valor da saída dos all-pass, para a realimentação.
    float m_lastWet = 0.0f;

    // Estado de cada all-pass: a entrada e a saída da amostra anterior.
    struct Stage
    {
        float previousInput = 0.0f;
        float previousOutput = 0.0f;
    };
    std::array<Stage, 4> m_stages{};

    SmoothedValue m_smoothedFeedback;
};
