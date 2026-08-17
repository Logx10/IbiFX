#include "HighPassFilter.h"

#include <cmath>

namespace
{
// 20 Hz a 2000 Hz cobre desde "só tirar o ruído de fundo" até "deixar só o
// médio", que é o ajuste de um amplificador bem apertado.
constexpr float kMinFrequency = 20.0f;
constexpr float kMaxFrequency = 2000.0f;
constexpr float kDefaultFrequency = 100.0f;
}

HighPassFilter::HighPassFilter()
{
    m_parameters.emplace_back("frequency", "Frequency",
                              kMinFrequency, kMaxFrequency, kDefaultFrequency);
}

void HighPassFilter::setFrequency(float hz)
{
    m_parameters[0].setValue(hz);
}

float HighPassFilter::frequency() const
{
    return m_parameters[0].value();
}

const char* HighPassFilter::name() const
{
    return "HighPass";
}

void HighPassFilter::prepare(double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
}

void HighPassFilter::reset()
{
    // Zerar a memória evita um degrau na primeira amostra depois de religar.
    m_previousInput = 0.0f;
    m_previousOutput = 0.0f;
}

void HighPassFilter::process(std::vector<float>& buffer)
{
    if (m_sampleRate <= 0.0)
    {
        return;
    }

    const double cutoff = static_cast<double>(m_parameters[0].value());

    // O coeficiente vem da constante de tempo do circuito RC equivalente:
    //
    //     RC    = 1 / (2 * pi * corte)     quanto tempo o filtro "lembra"
    //     dt    = 1 / sampleRate           quanto tempo passa entre amostras
    //     a     = RC / (RC + dt)
    //
    // Perto de 1, o filtro tem memória longa e corta pouco; perto de 0, ele
    // esquece rápido e corta muito. Calculado uma vez por bloco, e não por
    // amostra, porque envolve divisão e a frequência não muda no meio.
    const double rc = 1.0 / (6.283185307179586 * cutoff);
    const double dt = 1.0 / m_sampleRate;
    const float a = static_cast<float>(rc / (rc + dt));

    for (float& sample : buffer)
    {
        const float input = sample;

        // (input - m_previousInput) é a variação do sinal entre duas
        // amostras: pequena no grave, grande no agudo. É daí que vem o corte.
        const float output = a * (m_previousOutput + input - m_previousInput);

        m_previousInput = input;
        m_previousOutput = output;

        sample = output;
    }
}
