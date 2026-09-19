#include "Compressor.h"

#include <algorithm>
#include <cmath>

#include "OnePole.h"

namespace
{
// Piso de amplitude antes de converter pra dB. log10(0) é -infinito, e
// -infinito contamina qualquer conta que o use depois. -120 dB (o piso que
// este valor produz) já está bem abaixo do que qualquer amostra de áudio
// real representa.
constexpr float kMinLinear = 1e-6f;

float linearToDecibels(float linear)
{
    return 20.0f * std::log10(std::max(linear, kMinLinear));
}

float decibelsToLinear(float decibels)
{
    return std::pow(10.0f, decibels / 20.0f);
}
}

Compressor::Compressor()
{
    m_parameters.emplace_back("threshold", "Threshold", -60.0f, 0.0f, -20.0f);
    m_parameters.emplace_back("ratio", "Ratio", 1.0f, 20.0f, 4.0f);
    m_parameters.emplace_back("attack", "Attack", 0.001f, 0.5f, 0.01f);
    m_parameters.emplace_back("release", "Release", 0.01f, 2.0f, 0.15f);
}

void Compressor::setThreshold(float thresholdDb)
{
    m_parameters[0].setValue(thresholdDb);
}

float Compressor::threshold() const
{
    return m_parameters[0].value();
}

void Compressor::setRatio(float newRatio)
{
    m_parameters[1].setValue(newRatio);
}

float Compressor::ratio() const
{
    return m_parameters[1].value();
}

void Compressor::setAttack(float seconds)
{
    m_parameters[2].setValue(seconds);
}

float Compressor::attack() const
{
    return m_parameters[2].value();
}

void Compressor::setRelease(float seconds)
{
    m_parameters[3].setValue(seconds);
}

float Compressor::release() const
{
    return m_parameters[3].value();
}

const char* Compressor::name() const
{
    return "Compressor";
}

void Compressor::prepare(double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

void Compressor::reset()
{
    m_envelope = 0.0f;
}

void Compressor::process(std::vector<float>& buffer)
{
    const float thresholdDb = m_parameters[0].value();
    const float ratioValue = m_parameters[1].value();

    // Recalculados uma vez por bloco: os parâmetros só mudam quando alguém
    // mexe no controle, o que é raro comparado ao número de amostras num
    // bloco.
    const float attackCoeff = onePoleCoefficient(m_parameters[2].value(), m_sampleRate);
    const float releaseCoeff = onePoleCoefficient(m_parameters[3].value(), m_sampleRate);

    for (float& sample : buffer)
    {
        const float inputMagnitude = std::fabs(sample);
        const float coeff = inputMagnitude > m_envelope ? attackCoeff : releaseCoeff;

        m_envelope += coeff * (inputMagnitude - m_envelope);

        const float envelopeDb = linearToDecibels(m_envelope);

        float gainReductionDb = 0.0f;

        if (envelopeDb > thresholdDb)
        {
            const float excessDb = envelopeDb - thresholdDb;

            // ratioValue nunca é <= 0: o parâmetro tem mínimo 1.0.
            gainReductionDb = excessDb - excessDb / ratioValue;
        }

        sample *= decibelsToLinear(-gainReductionDb);
    }
}
