#pragma once

// Interpolação de Hermite de 4 pontos (forma de Catmull-Rom): o valor entre
// y0 e y1, a uma fração t de y0, usando ym1 e y2 para estimar a inclinação
// em cada ponta.
//
// Usada para ler uma linha de atraso numa posição fracionária — Chorus e
// Flanger movem o atraso continuamente, e quase nunca ele cai numa amostra
// inteira. Ler a amostra mais próxima soaria como chiado (o atraso andaria
// em degraus); a reta entre duas amostras (interpolação linear) abafa o
// agudo. Hermite passa pelos pontos com a curva suave das quatro vizinhas.
inline float hermite(float ym1, float y0, float y1, float y2, float t)
{
    const float c0 = y0;
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * t + c2) * t + c1) * t + c0;
}
