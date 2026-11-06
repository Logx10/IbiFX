#include "Fft.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace
{
// (a + bj)(c + dj) = (ac - bd) + (ad + bc)j, escrito à mão. O operator* de
// std::complex, sem -ffast-math, chama uma rotina da biblioteca que trata
// NaN e infinito (__mulsc3 no GCC) — várias vezes mais lenta, e este é o
// laço mais quente da FFT.
Fft::Complex multiply(Fft::Complex a, Fft::Complex b)
{
    return {a.real() * b.real() - a.imag() * b.imag(),
            a.real() * b.imag() + a.imag() * b.real()};
}
}

void Fft::prepare(std::size_t size)
{
    if (size == 0 || (size & (size - 1)) != 0)
        throw std::invalid_argument("Fft: tamanho precisa ser potencia de 2");

    m_size = size;

    // Em double e só depois para float: sin/cos em float perderiam
    // precisão justamente nos ângulos pequenos.
    constexpr double kPi = 3.14159265358979323846;
    m_twiddles.resize(size / 2);
    for (std::size_t k = 0; k < size / 2; ++k)
    {
        const double angle = -2.0 * kPi * static_cast<double>(k) / static_cast<double>(size);
        m_twiddles[k] = Complex(static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle)));
    }

    std::size_t bits = 0;
    while ((std::size_t{1} << bits) < size)
        ++bits;

    m_bitReversed.resize(size);
    for (std::size_t i = 0; i < size; ++i)
    {
        std::size_t reversed = 0;
        for (std::size_t b = 0; b < bits; ++b)
        {
            if (i & (std::size_t{1} << b))
                reversed |= std::size_t{1} << (bits - 1 - b);
        }
        m_bitReversed[i] = reversed;
    }
}

std::size_t Fft::size() const
{
    return m_size;
}

void Fft::forward(std::vector<Complex>& data) const
{
    transform(data, false);
}

void Fft::inverse(std::vector<Complex>& data) const
{
    transform(data, true);

    const float scale = 1.0f / static_cast<float>(m_size);
    for (Complex& value : data)
        value *= scale;
}

void Fft::transform(std::vector<Complex>& data, bool inverse) const
{
    // Embaralha para a ordem de bits invertidos. Troca cada par uma vez só
    // (i < j), senão o segundo swap desfaria o primeiro.
    for (std::size_t i = 0; i < m_size; ++i)
    {
        const std::size_t j = m_bitReversed[i];
        if (i < j)
            std::swap(data[i], data[j]);
    }

    // Passadas de borboletas: blocos de 2, 4, 8... até N. Num bloco de
    // tamanho length, o twiddle W^k do tamanho length é W^(k·N/length) da
    // tabela de tamanho N — daí o stride.
    for (std::size_t length = 2; length <= m_size; length *= 2)
    {
        const std::size_t half = length / 2;
        const std::size_t stride = m_size / length;

        for (std::size_t start = 0; start < m_size; start += length)
        {
            for (std::size_t k = 0; k < half; ++k)
            {
                // A inversa usa o conjugado de W: e^(+2πjk/N).
                const Complex twiddle = inverse ? std::conj(m_twiddles[k * stride]) : m_twiddles[k * stride];

                const Complex even = data[start + k];
                const Complex odd = multiply(twiddle, data[start + k + half]);

                data[start + k] = even + odd;
                data[start + k + half] = even - odd;
            }
        }
    }
}
