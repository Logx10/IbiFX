#include "Resampler.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace
{
constexpr double kPi = 3.14159265358979323846;

// Passagens por zero da sinc de cada lado — quanto mais, mais íngreme o
// filtro e mais fiel o resultado, ao custo de mais contas. 32 deixa a
// ondulação bem abaixo do que se ouve numa IR.
constexpr int kZeroCrossings = 32;

double sinc(double x)
{
    if (std::fabs(x) < 1e-9)
        return 1.0;
    return std::sin(kPi * x) / (kPi * x);
}

// Blackman, para u em [-1, 1]; zero fora.
double blackman(double u)
{
    if (std::fabs(u) >= 1.0)
        return 0.0;
    return 0.42 + 0.5 * std::cos(kPi * u) + 0.08 * std::cos(2.0 * kPi * u);
}
}

std::vector<float> resample(const std::vector<float>& input, double fromRate, double toRate)
{
    if (input.empty() || fromRate <= 0.0 || toRate <= 0.0 || fromRate == toRate)
        return input;

    const double ratio = toRate / fromRate;
    // O -1e-9 absorve o erro de ponto flutuante: 441 × (48000/44100) sai
    // 480,0000000001, e um ceil() seco daria 481.
    const std::size_t outputLength = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::ceil(static_cast<double>(input.size()) * ratio - 1e-9)));

    // Fração do Nyquist de ENTRADA que sobrevive: 1 subindo a taxa (nada a
    // cortar), toRate/fromRate descendo (ver o comentário do header).
    const double cutoff = std::min(1.0, ratio);

    // Meia largura da sinc, em amostras de entrada. Alargar a sinc pelo
    // corte é o que transforma ela no passa-baixa.
    const double halfWidth = static_cast<double>(kZeroCrossings) / cutoff;

    const auto lastInput = static_cast<std::ptrdiff_t>(input.size()) - 1;
    std::vector<float> output(outputLength, 0.0f);

    for (std::size_t n = 0; n < outputLength; ++n)
    {
        // Onde esta amostra de saída cai, medido em amostras de entrada.
        const double position = static_cast<double>(n) / ratio;

        const auto first = std::max<std::ptrdiff_t>(0, static_cast<std::ptrdiff_t>(std::ceil(position - halfWidth)));
        const auto last = std::min<std::ptrdiff_t>(lastInput, static_cast<std::ptrdiff_t>(std::floor(position + halfWidth)));

        double sum = 0.0;
        for (std::ptrdiff_t k = first; k <= last; ++k)
        {
            const double distance = position - static_cast<double>(k);
            sum += static_cast<double>(input[static_cast<std::size_t>(k)])
                   * cutoff * sinc(cutoff * distance) * blackman(distance / halfWidth);
        }

        output[n] = static_cast<float>(sum);
    }

    return output;
}
