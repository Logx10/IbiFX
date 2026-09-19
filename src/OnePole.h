#pragma once

#include <cmath>

// onePoleCoefficient — converte um tempo de resposta em segundos no
// coeficiente de um filtro de um polo:
//
//     atual += coeficiente * (alvo - atual)
//
// Quanto maior o coeficiente, mais rápido "atual" se aproxima de "alvo" a
// cada amostra. A fórmula vem de pedir que, depois de `seconds` segundos, a
// distância até o alvo tenha caído para 1/e (~37%) do valor inicial — a
// definição usual de "tempo de resposta" de um filtro exponencial.
//
// TERCEIRA VEZ, ENTÃO AGORA É COMPARTILHADO
// Esta conta apareceu em NoiseGate (ataque/release do gate), Compressor
// (ataque/release do detector de nível) e PowerAmp (ataque/release do
// envelope de sag) — cada um com sua própria cópia da função, comentada com
// a regra do §55 do AI_GUIDELINES: "primeiro problema: resolver; segundo:
// observar; terceiro: considerar abstração". Esta é a extração, no momento
// exato em que a regra manda considerá-la — não antes, quando o formato
// certo da abstração ainda era um palpite.
inline float onePoleCoefficient(float seconds, double sampleRate)
{
    if (seconds <= 0.0f)
    {
        return 1.0f;
    }

    return 1.0f - std::exp(-1.0f / (seconds * static_cast<float>(sampleRate)));
}
