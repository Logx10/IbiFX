#include "ScreamerTone.h"

namespace
{
// Fig. 15 de Yeh, Abel e Smith, DAFx-07. As duas metades do pot
// ligam o cursor às entradas + e - do op amp, à mesma tensão AC.
constexpr double kInputResistor = 1000.0;
constexpr double kBiasResistor = 10000.0;
constexpr double kInputCapacitor = 220e-9;
constexpr double kTonePot = 20000.0;
constexpr double kWiperResistor = 220.0;
constexpr double kWiperCapacitor = 220e-9;
constexpr double kFeedbackResistor = 1000.0;
}

ScreamerTone::ScreamerTone()
{
    m_parameters.emplace_back("tone", "Tone", 0.0f, 1.0f, 0.5f);
    prepare(48000.0, 0);
}

const char* ScreamerTone::name() const
{
    return "ScreamerTone";
}

void ScreamerTone::prepare(double sampleRate, int /*blockSize*/)
{
    const double rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    m_bilinearScale = 2.0 * rate;
    m_tone.prepare(rate, kDefaultRampSeconds, m_parameters[0].value());
    reset();
}

void ScreamerTone::reset()
{
    m_tone.snapTo(m_parameters[0].value());
    updateCoefficients(m_tone.current());
    m_x1 = m_x2 = m_y1 = m_y2 = 0.0;
}

void ScreamerTone::updateCoefficients(float position)
{
    const double t = position;
    const double parallelPot = kTonePot * t * (1.0 - t);
    const double tauWiper = (kWiperResistor + parallelPot) * kWiperCapacitor;
    const double tauInput = kInputResistor * kInputCapacitor;

    // Com o op amp ideal, a corrente do cursor se divide em 1-t no
    // lado de entrada e t no lado de realimentação. O primeiro carrega
    // o filtro de entrada e o segundo aumenta o ganho do op amp.
    // H(s) = (1 + numerator1*s) / (denominator0 + denominator1*s + denominator2*s²).
    const double numerator1 = tauWiper + kFeedbackResistor * t * kWiperCapacitor;
    const double denominator0 = 1.0 + kInputResistor / kBiasResistor;
    const double denominator1 = tauInput + denominator0 * tauWiper
                              + kInputResistor * (1.0 - t) * kWiperCapacitor;
    const double denominator2 = tauInput * tauWiper;

    // Transformada bilinear s = 2*Fs*(1-z^-1)/(1+z^-1).
    const double linear = denominator1 * m_bilinearScale;
    const double quadratic = denominator2 * m_bilinearScale * m_bilinearScale;
    const double zero = numerator1 * m_bilinearScale;
    const double scale = 1.0 / (denominator0 + linear + quadratic);
    m_b0 = (1.0 + zero) * scale;
    m_b1 = 2.0 * scale;
    m_b2 = (1.0 - zero) * scale;
    m_a1 = 2.0 * (denominator0 - quadratic) * scale;
    m_a2 = (denominator0 - linear + quadratic) * scale;
}

void ScreamerTone::process(std::vector<float>& buffer)
{
    const float initialTone = m_tone.current();
    m_tone.setTarget(m_parameters[0].value());
    if (m_tone.current() != initialTone) updateCoefficients(m_tone.current());
    for (float& sample : buffer)
    {
        if (m_tone.isSmoothing()) updateCoefficients(m_tone.nextValue());

        // Históricos de entrada e saída, preservados ao girar o Tone.
        const double input = sample;
        const double output = m_b0 * input + m_b1 * m_x1 + m_b2 * m_x2
                            - m_a1 * m_y1 - m_a2 * m_y2;
        m_x2 = m_x1;
        m_x1 = input;
        m_y2 = m_y1;
        m_y1 = output;
        sample = static_cast<float>(output);
    }
}
