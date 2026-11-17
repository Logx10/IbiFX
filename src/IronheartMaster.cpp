#include "IronheartMaster.h"

#include <cmath>

IronheartMaster::IronheartMaster()
{
    m_parameters.emplace_back("dynamics", "Dynamics", 0.0f, 1.0f, 0.5f);
    m_parameters.emplace_back("tone", "Tone", 0.0f, 1.0f, 0.5f);
    m_parameters.emplace_back("reverbEnabled", "Reverb On", 0.0f, 1.0f, 1.0f);
    m_parameters.emplace_back("reverb", "Reverb", 0.0f, 1.0f, 0.15f);
    m_parameters.emplace_back("watts", "Watts", 0.0f, 1.0f, 0.5f);
    m_parameters.emplace_back("power", "Power", 0.0f, 1.0f, 1.0f);
    m_parameters.emplace_back("play", "Standby / Play", 0.0f, 1.0f, 1.0f);
    m_powerAmp.setDrive(1.2f);
    m_powerAmp.findParameter("coupling")->setValue(5.0f);
    m_reverb.setDecay(0.65f);
    m_reverb.setDamping(0.55f);
}

const char* IronheartMaster::name() const { return "IronheartMaster"; }

void IronheartMaster::prepare(double sampleRate, int blockSize)
{
    m_sampleRate = std::isfinite(sampleRate) && sampleRate > 0.0 ? sampleRate : 44100.0;
    m_reverb.prepare(m_sampleRate, blockSize);
    m_powerAmp.prepare(m_sampleRate, blockSize);
    m_level.prepare(m_sampleRate, kDefaultRampSeconds, 0.0f);
    m_dynamics.prepare(m_sampleRate, kDefaultRampSeconds, m_parameters[Dynamics].value());
    m_tone.prepare(m_sampleRate, kDefaultRampSeconds, m_parameters[Tone].value());
    reset();
}

void IronheartMaster::reset()
{
    m_bass = m_top = 0.0f;
    m_reverb.reset();
    m_powerAmp.reset();
    const float watts = m_parameters[Watts].value();
    const bool playing = m_parameters[Power].value() >= 0.5f && m_parameters[Play].value() >= 0.5f;
    m_level.snapTo(playing ? 0.75f * watts : 0.0f);
    m_dynamics.snapTo(m_parameters[Dynamics].value());
    m_tone.snapTo(m_parameters[Tone].value());
}

void IronheartMaster::process(std::vector<float>& buffer)
{
    const bool playing = m_parameters[Power].value() >= 0.5f && m_parameters[Play].value() >= 0.5f;
    const float watts = m_parameters[Watts].value();
    m_level.setTarget(playing ? 0.75f * watts : 0.0f);
    m_dynamics.setTarget(m_parameters[Dynamics].value());
    m_tone.setTarget(m_parameters[Tone].value());
    m_reverb.setMix(m_parameters[ReverbOn].value() >= 0.5f
        ? 0.35f * m_parameters[ReverbLevel].value() : 0.0f);
    // A cauda continua sendo processada ao desligar o reverb, evitando
    // congelar o histórico e trazer uma nota antiga ao religá-lo.
    m_reverb.process(buffer);
    const float bassCoeff = 1.0f - std::exp(-6.2831853f * 140.0f / static_cast<float>(m_sampleRate));
    for (float& sample : buffer)
    {
        m_bass += bassCoeff * (sample - m_bass);
        sample += (m_dynamics.nextValue() * 1.5f - 0.75f) * m_bass;
    }
    m_powerAmp.process(buffer);
    for (float& sample : buffer)
    {
        const float tone = m_tone.nextValue();
        const float cutoff = 1200.0f * std::pow(10.0f, tone);
        const float coeff = 1.0f - std::exp(-6.2831853f * cutoff / static_cast<float>(m_sampleRate));
        m_top += coeff * (sample - m_top);
        sample = m_top * m_level.nextValue();
    }
}
