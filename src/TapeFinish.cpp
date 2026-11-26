#include "TapeFinish.h"

#include <cmath>
#include <numbers>

#include "OnePole.h"

namespace
{
constexpr float kMaxDrive = 4.0f;
constexpr float kTrebleLoss = 0.72f;
constexpr float kBassBump = 0.36f;

float coefficient(float hz, double rate)
{
    return onePoleCoefficient(1.0f / (2.0f * std::numbers::pi_v<float> * hz), rate);
}
}

TapeFinish::TapeFinish()
{
    m_parameters.emplace_back("drive", "Drive", 0.0f, 1.0f, 0.35f);
    m_parameters.emplace_back("color", "Color", 0.0f, 1.0f, 0.30f);
    m_parameters.emplace_back("level", "Output", 0.0f, 2.0f, 1.0f);
    prepare(48000.0, 0);
}

const char* TapeFinish::name() const
{
    return "TapeFinish";
}

void TapeFinish::prepare(double sampleRate, int /*blockSize*/)
{
    const double rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    m_drive.prepare(rate, kDefaultRampSeconds, m_parameters[0].value());
    m_color.prepare(rate, kDefaultRampSeconds, m_parameters[1].value());
    m_output.prepare(rate, kDefaultRampSeconds, m_parameters[2].value());
    m_trebleCoefficient = coefficient(4800.0f, rate);
    m_bassUpperCoefficient = coefficient(150.0f, rate);
    m_bassLowerCoefficient = coefficient(40.0f, rate);
    reset();
}

void TapeFinish::reset()
{
    m_drive.snapTo(m_parameters[0].value());
    m_color.snapTo(m_parameters[1].value());
    m_output.snapTo(m_parameters[2].value());
    m_oversampler.reset();
    m_trebleLow = m_bassUpper = m_bassLower = 0.0f;
}

void TapeFinish::process(std::vector<float>& buffer)
{
    m_drive.setTarget(m_parameters[0].value());
    m_color.setTarget(m_parameters[1].value());
    m_output.setTarget(m_parameters[2].value());

    for (float& sample : buffer)
    {
        const float drive = kMaxDrive * m_drive.nextValue();
        const float color = m_color.nextValue();
        const float level = m_output.nextValue();
        // tanh(d*x)/d tem inclinação unitária perto de zero. Um Drive
        // maior amacia os picos sem subir junto o nível das notas fracas.
        const float tape = m_oversampler.process(sample, [drive](float input) {
            return drive > 0.0f ? std::tanh(input * drive) / drive : input;
        });

        // Perda suave de agudos e realce largo de graves, ambos variando
        // com Color. A diferença dos dois passa-baixas forma o realce.
        m_trebleLow += m_trebleCoefficient * (tape - m_trebleLow);
        m_bassUpper += m_bassUpperCoefficient * (tape - m_bassUpper);
        m_bassLower += m_bassLowerCoefficient * (tape - m_bassLower);
        const float treble = tape - m_trebleLow;
        const float bass = m_bassUpper - m_bassLower;
        sample = level * (tape + color * (kBassBump * bass - kTrebleLoss * treble));
    }
}
