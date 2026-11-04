#include "Phaser.h"

#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

// Faixa da frequência de corte dos all-pass. O piso vem dos vales medidos
// do Phase 90 (ver o header); o teto é uma estimativa — a ElectroSmash não
// publica o extremo agudo —, escolhida para o segundo vale (2,414·fc)
// chegar perto de 4 kHz, onde a guitarra ainda tem harmônicos audíveis.
constexpr double kLowestHz = 141.0;
constexpr double kHighestHz = 1600.0;

// Coeficiente do all-pass de 1ª ordem pela transformada bilinear — a mesma
// conta do Rotovibe:  y[n] = a·x[n] + x[n-1] - a·y[n-1].
float allPassCoefficient(double frequency, double sampleRate)
{
    const double tangent = std::tan(kPi * frequency / sampleRate);
    return static_cast<float>((tangent - 1.0) / (tangent + 1.0));
}
}

Phaser::Phaser()
{
    m_parameters.emplace_back("rate", "Speed", 0.1f, 8.0f, 0.5f);
    m_parameters.emplace_back("feedback", "Feedback", 0.0f, 0.7f, 0.0f);
}

void Phaser::setRate(float hertz)
{
    m_parameters[0].setValue(hertz);
}

float Phaser::rate() const
{
    return m_parameters[0].value();
}

void Phaser::setFeedback(float amount)
{
    m_parameters[1].setValue(amount);
}

float Phaser::feedback() const
{
    return m_parameters[1].value();
}

const char* Phaser::name() const
{
    return "Phaser";
}

void Phaser::prepare(double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    m_smoothedFeedback.prepare(m_sampleRate, kDefaultRampSeconds, m_parameters[1].value());
    reset();
}

void Phaser::reset()
{
    m_stages = {};
    m_phase = 0.0;
    m_lastWet = 0.0f;
    m_smoothedFeedback.snapTo(m_parameters[1].value());
}

void Phaser::process(std::vector<float>& buffer)
{
    m_smoothedFeedback.setTarget(m_parameters[1].value());

    const double phaseIncrement = static_cast<double>(m_parameters[0].value()) / m_sampleRate;
    const double octaves = std::log2(kHighestHz / kLowestHz);

    for (float& sample : buffer)
    {
        // LFO de 0 a 1, convertido em oitavas acima do piso.
        const double lfo = 0.5 + 0.5 * std::sin(kTwoPi * m_phase);
        const double cutoff = kLowestHz * std::pow(2.0, octaves * lfo);
        const float a = allPassCoefficient(cutoff, m_sampleRate);

        // A realimentação entra com a saída da amostra ANTERIOR: na mesma
        // amostra ela ainda não existe.
        float wet = sample + m_smoothedFeedback.nextValue() * m_lastWet;

        for (Stage& stage : m_stages)
        {
            const float output = a * wet + stage.previousInput - a * stage.previousOutput;
            stage.previousInput = wet;
            stage.previousOutput = output;
            wet = output;
        }

        m_lastWet = wet;

        // Meio a meio, como o mixer de saída do Phase 90: é a soma com o
        // seco que cria os vales.
        sample = 0.5f * (sample + wet);

        m_phase += phaseIncrement;
        if (m_phase >= 1.0)
            m_phase -= 1.0;
    }
}
