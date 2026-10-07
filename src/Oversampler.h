#pragma once

#include <array>
#include <cstddef>

// Oversampler — roda uma curva de distorção a 4× a taxa do motor, para ela
// não gerar aliasing.
//
// O PROBLEMA: DISTORÇÃO CRIA FREQUÊNCIAS QUE NÃO CABEM
// tanh(), um clipper ou qualquer curva não linear acrescenta harmônicos ao
// sinal — é isso que "distorcer" é. Um mi agudo de 1,3 kHz saturado com
// força ganha harmônicos em 3,9 kHz, 6,5 kHz... e bem acima de 24 kHz, o
// limite do que um sinal a 48 kHz consegue representar (Nyquist). Esses
// não somem: voltam ESPELHADOS para dentro da faixa audível, em frequências
// sem relação harmônica nenhuma com a nota. É o "fizz" áspero, abelhudo,
// de simulador digital barato — e com três saturações em série (pedal,
// pré, potência) ele se acumula.
//
// A SOLUÇÃO: DISTORCER ONDE HÁ ESPAÇO
// 1. Sobe a taxa 4× (192 kHz): intercala zeros e filtra — agora há espaço
//    até 96 kHz para os harmônicos novos.
// 2. Aplica a curva em cada uma das 4 sub-amostras.
// 3. Filtra tudo acima de ~20 kHz e devolve uma amostra a cada 4. O que
//    teria virado aliasing é removido ANTES de voltar para 48 kHz.
// A curva continua a mesma; só o lixo some.
//
// O FILTRO
// Um FIR passa-baixa de 128 taps (sinc com janela de Blackman), o mesmo
// nos dois sentidos, com corte em 22 kHz a 192 kHz: passa até ~19 kHz, e
// a partir de ~27 kHz atenua mais de 70 dB — o que sobra e ainda dobra
// para baixo cai acima de 20 kHz ou abaixo do audível. Subindo, só 1 de
// cada 4 amostras não é zero, então cada sub-amostra soma só 32 taps
// (forma "polifásica"); descendo, só a amostra que fica é calculada.
//
// CUSTO
// ~256 multiplicações e 4 avaliações da curva por amostra — pouco perto do
// Cabinet, que faz uma multiplicação por tap da IR. E ~0,66 ms de atraso
// por módulo (os dois filtros são de fase linear, atraso de meio filtro
// cada um): três saturações somam ~2 ms, abaixo do que se sente tocando.
//
// TEMPO REAL
// Tudo de tamanho fixo (std::array); nada aloca. Os coeficientes são
// calculados uma vez só, no primeiro Oversampler construído — no domínio
// de controle, ao montar a cadeia.
class Oversampler
{
public:
    static constexpr int kFactor = 4;

    Oversampler();

    // Zera o histórico dos dois filtros — o equivalente a silêncio desde
    // sempre.
    void reset();

    // Processa UMA amostra na taxa do motor: sobe para 4 sub-amostras,
    // passa cada uma por shape(x) e devolve a amostra filtrada de volta na
    // taxa do motor. shape é qualquer chamável float(float).
    template <typename Shape>
    float process(float input, Shape&& shape);

private:
    static constexpr std::size_t kTaps = 128;
    static constexpr std::size_t kTapsPerPhase = kTaps / kFactor;

    static const std::array<float, kTaps>& coefficients();

    // Buffers circulares; os tamanhos são potências de 2, então "& (N-1)"
    // faz o papel do módulo.
    std::array<float, kTapsPerPhase> m_upHistory{};
    std::size_t m_upPosition = 0;

    std::array<float, kTaps> m_downHistory{};
    std::size_t m_downPosition = 0;
};

template <typename Shape>
float Oversampler::process(float input, Shape&& shape)
{
    const std::array<float, kTaps>& h = coefficients();

    m_upHistory[m_upPosition] = input;

    for (std::size_t phase = 0; phase < static_cast<std::size_t>(kFactor); ++phase)
    {
        // Sub-amostra `phase` do sinal com zeros intercalados, filtrado:
        // só os taps h[phase + 4j] caem sobre amostras que não são zero.
        float upsampled = 0.0f;
        for (std::size_t j = 0; j < kTapsPerPhase; ++j)
            upsampled += h[phase + kFactor * j] * m_upHistory[(m_upPosition - j) & (kTapsPerPhase - 1)];

        // ×4: intercalar zeros dividiu a energia por 4, e o ganho volta aqui.
        const float shaped = shape(upsampled * static_cast<float>(kFactor));

        m_downPosition = (m_downPosition + 1) & (kTaps - 1);
        m_downHistory[m_downPosition] = shaped;
    }

    m_upPosition = (m_upPosition + 1) & (kTapsPerPhase - 1);

    // Descendo: só a amostra que fica é calculada, as outras 3 nem existem.
    float output = 0.0f;
    for (std::size_t k = 0; k < kTaps; ++k)
        output += h[k] * m_downHistory[(m_downPosition - k) & (kTaps - 1)];

    return output;
}
