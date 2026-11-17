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
    m_reverb.setDecay(0.65f);
    m_reverb.setDamping(0.55f);
}

const char* IronheartMaster::name() const { return "IronheartMaster"; }

void IronheartMaster::prepare(double sampleRate, int blockSize)
{
    m_sampleRate = std::isfinite(sampleRate) && sampleRate > 0.0 ? sampleRate : 44100.0;
    m_reverb.prepare(m_sampleRate, blockSize);
    m_powerSection.prepare(m_sampleRate, m_parameters[Tone].value(),
                           m_parameters[Watts].value(), m_parameters[Dynamics].value());
    // Bloqueio de DC equivalente na saída; não modela o transformador.
    m_dcR = static_cast<float>(std::exp(-6.283185307179586 * 5.0 / m_sampleRate));
    m_dcG = .5f * (1.0f + m_dcR);
    m_level.prepare(m_sampleRate, kDefaultRampSeconds, 0.0f);
    m_reverbLevel.prepare(m_sampleRate, kDefaultRampSeconds, 0.0f);
    reset();
}

void IronheartMaster::reset()
{
    m_reverb.reset();
    m_dcX = m_dcY = 0.0f;
    m_powerSection.setControls(m_parameters[Tone].value(), m_parameters[Watts].value(), m_parameters[Dynamics].value());
    m_powerSection.reset();
    const bool playing = m_parameters[Power].value() >= 0.5f && m_parameters[Play].value() >= 0.5f;
    m_level.snapTo(playing ? 0.75f : 0.0f);
    m_reverbLevel.snapTo(m_parameters[ReverbOn].value() >= 0.5f ? 0.35f * m_parameters[ReverbLevel].value() : 0.0f);
}

void IronheartMaster::process(std::vector<float>& buffer)
{
    const bool playing = m_parameters[Power].value() >= 0.5f && m_parameters[Play].value() >= 0.5f;
    m_level.setTarget(playing ? 0.75f : 0.0f);
    m_powerSection.setControls(m_parameters[Tone].value(), m_parameters[Watts].value(), m_parameters[Dynamics].value());
    m_reverbLevel.setTarget(m_parameters[ReverbOn].value() >= 0.5f
        ? 0.35f * m_parameters[ReverbLevel].value() : 0.0f);
    // A cauda continua sendo processada ao desligar o reverb, evitando
    // congelar o histórico e trazer uma nota antiga ao religá-lo.
    for (float& sample : buffer)
    {
        const float wet = m_reverb.processWetSample(sample);
        const float power = m_powerSection.processSample(sample + wet * m_reverbLevel.nextValue());
        const float blocked = m_dcG * (power - m_dcX) + m_dcR * m_dcY;
        m_dcX = power; m_dcY = blocked;
        sample = blocked * m_level.nextValue();
    }
}
