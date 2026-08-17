#pragma once

#include <vector>

#include "AudioModule.h"

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
class Clipper : public AudioModule
{
public:
    Clipper();

    // Define o teto usado nas próximas chamadas de process().
    void setThreshold(float newThreshold);

    // Devolve o teto atual.
    float threshold() const;

    const char* name() const override;

    // Corta as amostras que passam de +threshold ou de -threshold.
    void process(std::vector<float>& buffer) override;
};
