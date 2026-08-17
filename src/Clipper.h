#pragma once

#include <vector>

#include "AudioModule.h"
#include "SmoothedValue.h"

// Clipper — hard clipping simétrico.
//
// Impede que amostras ultrapassem um teto, para cima e para baixo. Existe
// porque o GainProcessor empurra o sinal para fora de [-1, +1] de propósito,
// e alguém precisa segurar esse limite de forma explícita.
//
// Cortar os picos muda o formato da onda, e mudar o formato cria frequências
// que não estavam no sinal original — os harmônicos. Por isso o som fica
// "sujo": clipping é distorção.
//
//     antes (senoide)         depois (clipped)
//
//        ___                     _______
//       /   \                   /       \
//      /     \                 |         |
//     ---------------      ---------------------
//            \     /                     |     |
//             \___/                       \_____/
//
// Este é o corte em quina viva — som de fuzz, áspero e agressivo. O
// SoftClipper faz a versão curva do mesmo trabalho.
//
// O corte é simétrico: um único valor controla os dois lados, em +threshold
// e -threshold. A faixa do parâmetro começa em 0 justamente para tornar
// impossível um teto negativo, que inverteria a faixa válida e produziria
// lixo. O limite do parâmetro resolve o problema na origem.
//
// O teto é suavizado: movê-lo de uma vez faria a região cortada saltar, e o
// salto é audível como clique. Sem prepare(), a suavização fica inativa.
class Clipper : public AudioModule
{
public:
    Clipper();

    // Define o teto de destino. A mudança é aplicada gradualmente.
    void setThreshold(float newThreshold);

    // Devolve o teto de destino, não o valor instantâneo da rampa.
    float threshold() const;

    const char* name() const override;

    // Configura a rampa de suavização para este sample rate.
    void prepare(double sampleRate, int blockSize) override;

    // Salta o teto para o valor de destino, sem rampa.
    void reset() override;

    // Corta as amostras que passam de +threshold ou de -threshold.
    void process(std::vector<float>& buffer) override;

private:
    SmoothedValue m_smoothedThreshold;
};
