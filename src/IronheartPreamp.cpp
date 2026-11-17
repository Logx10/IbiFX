#include "IronheartPreamp.h"

#include <cmath>

namespace
{
// Conversão de nível da cadeia em volts para o restante do motor. Não é
// um componente do circuito nem uma normalização dependente dos knobs.
constexpr float kToneOutputCalibration = 6.3f;
float taper(float position) { return position * position; }

// Curva A aproximada, com 10% da resistência a meio curso. O esquema
// identifica VR2 como log, mas não fornece sua curva medida.
float boostResistance(float position)
{
    return 100000.0f * (std::pow(81.0f, position) - 1.0f) / 80.0f;
}
}

IronheartPreamp::IronheartPreamp()
{
    m_parameters.emplace_back("lead", "Channel", 0.0f, 1.0f, 0.0f);
    m_parameters.emplace_back("clean", "Clean / Rhythm", 0.0f, 1.0f, 1.0f);
    m_parameters.emplace_back("boostEnabled", "Boost On", 0.0f, 1.0f, 0.0f);
    m_parameters.emplace_back("boost", "Pre-Boost", 0.0f, 1.0f, 0.3f);
    m_parameters.emplace_back("cleanVolume", "Clean Volume", 0.0f, 1.0f, 0.5f);
    for (const char* prefix : {"rhythm", "lead"})
    {
        const std::string id(prefix);
        const std::string label = id == "lead" ? "Lead " : "Rhythm ";
        m_parameters.emplace_back(id + "Gain", label + "Gain", 0.0f, 1.0f, 0.5f);
        m_parameters.emplace_back(id + "Bass", label + "Bass", 0.0f, 1.0f, 0.5f);
        m_parameters.emplace_back(id + "Mid", label + "Middle", 0.0f, 1.0f, 0.5f);
        m_parameters.emplace_back(id + "Treble", label + "Treble", 0.0f, 1.0f, 0.5f);
        m_parameters.emplace_back(id + "Deep", label + "Deep", 0.0f, 1.0f, 0.0f);
        m_parameters.emplace_back(id + "MidShift", label + "Mid Shift", 0.0f, 1.0f, 0.0f);
        m_parameters.emplace_back(id + "TrebleShift", label + "Treble Shift", 0.0f, 1.0f, 0.0f);
        m_parameters.emplace_back(id + "Volume", label + "Volume", 0.0f, 1.0f, 0.5f);
    }
    configure();
}

float IronheartPreamp::value(Index index) const { return m_parameters[index].value(); }
const char* IronheartPreamp::name() const { return "IronheartPreamp"; }

void IronheartPreamp::configure()
{
    const bool lead = value(Lead) >= 0.5f;
    const bool clean = !lead && value(Clean) >= 0.5f;
    const auto channel = lead ? IronheartChannelNetwork::Channel::Lead
        : clean ? IronheartChannelNetwork::Channel::Clean : IronheartChannelNetwork::Channel::Rhythm;
    m_channels.setControls(channel, value(LeadGain), value(RhythmGain), value(CleanVolume));
    m_inputStage.setResistance(boostResistance(value(Boost)));
    m_inputStage.setEnabled(value(BoostOn) >= 0.5f);
    m_gainStages.setClean(clean);
    m_leadMix.setTarget(lead ? 1.0f : 0.0f);
    // Recuperação de nível maior no modo de menor ganho; não normalizamos
    // a entrada para preservar a resposta à força da palhetada.
    m_volume.setTarget(clean ? (value(CleanVolume) == 0.0f ? 0.0f : 6.0f)
                             : taper(lead ? value(LeadVolume) : value(RhythmVolume)));

    // As duas redes continuam processando: trocar de canal não recupera
    // estados de capacitores congelados. Clean compartilha a seção Rhythm.
    const auto controls = [this](IronheartToneNetwork& tone, Index start) {
        tone.setControls(m_parameters[start].value(), m_parameters[start + 1].value(),
                         m_parameters[start + 2].value(), m_parameters[start + 3].value() >= 0.5f,
                         m_parameters[start + 4].value() >= 0.5f, m_parameters[start + 5].value() >= 0.5f);
    };
    controls(m_leadTone, LeadBass);
    controls(m_rhythmTone, RhythmBass);
}

void IronheartPreamp::prepare(double sampleRate, int blockSize)
{
    m_sampleRate = std::isfinite(sampleRate) && sampleRate > 0.0 ? sampleRate : 44100.0;
    configure();
    m_inputStage.prepare(m_sampleRate * Oversampler::kFactor,
                         boostResistance(value(Boost)), value(BoostOn) >= 0.5f);
    m_v1a.prepare(m_sampleRate * Oversampler::kFactor);
    const auto channel = value(Lead) >= 0.5f ? IronheartChannelNetwork::Channel::Lead
        : value(Clean) >= 0.5f ? IronheartChannelNetwork::Channel::Clean : IronheartChannelNetwork::Channel::Rhythm;
    m_channels.prepare(m_sampleRate * Oversampler::kFactor, channel,
                       value(LeadGain), value(RhythmGain), value(CleanVolume));
    m_gainStages.prepare(m_sampleRate * Oversampler::kFactor,
                         value(Lead) < 0.5f && value(Clean) >= 0.5f);
    const double gridBias = m_gainStages.v2b().biasPoint().plateVolts * 620.0 / 1090.0;
    m_leadFollower.prepare(gridBias);
    m_rhythmFollower.prepare(gridBias);
    const auto prepareTone = [this](IronheartToneNetwork& tone, Index start) {
        tone.prepare(m_sampleRate * Oversampler::kFactor, m_parameters[start].value(),
                     m_parameters[start + 1].value(), m_parameters[start + 2].value(),
                     m_parameters[start + 3].value() >= 0.5f, m_parameters[start + 4].value() >= 0.5f,
                     m_parameters[start + 5].value() >= 0.5f);
    };
    prepareTone(m_leadTone, LeadBass);
    prepareTone(m_rhythmTone, RhythmBass);
    m_volume.prepare(m_sampleRate, kDefaultRampSeconds, m_volume.target());
    m_leadMix.prepare(m_sampleRate, kDefaultRampSeconds, m_leadMix.target());
    (void)blockSize;
    reset();
}

void IronheartPreamp::reset()
{
    configure();
    m_volume.snapTo(m_volume.target());
    m_leadMix.snapTo(m_leadMix.target());
    m_oversampler.reset();
    m_inputStage.reset();
    m_v1a.reset();
    m_channels.reset();
    m_gainStages.reset();
    m_leadFollower.reset(); m_rhythmFollower.reset();
    m_leadTone.reset(); m_rhythmTone.reset();
}

void IronheartPreamp::process(std::vector<float>& buffer)
{
    configure();
    for (float& sample : buffer)
    {
        const float lead = m_leadMix.nextValue();
        sample = m_oversampler.process(sample, [this, lead](float x) {
            // Escala escolhida: 1 unidade digital = 1 V na grade de V1A.
            const auto load = m_channels.beginSample();
            const double plate = m_v1a.processSample(m_inputStage.processSample(x), load);
            const auto outputs = m_channels.finishSample(plate);
            const double grid = m_gainStages.processSample(outputs.grid);
            const auto leadLoad = m_leadTone.beginSample();
            const auto rhythmLoad = m_rhythmTone.beginSample();
            const double leadOut = m_leadTone.finishSample(m_leadFollower.processSample(grid, leadLoad));
            const double rhythmOut = m_rhythmTone.finishSample(m_rhythmFollower.processSample(grid, rhythmLoad));
            return static_cast<float>(0.04 * (lead * leadOut + (1.0f - lead) * rhythmOut));
        });
        sample *= m_volume.nextValue();
    }
    // Adaptação fixa de nível, sem normalização por nota/bloco.
    for (float& sample : buffer) sample *= kToneOutputCalibration;
}
