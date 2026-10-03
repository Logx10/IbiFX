#include "Metronome.h"

#include <cmath>

namespace
{
constexpr double kTwoPi = 6.283185307179586;

// Duração de cada clique. Curto o bastante para nunca se confundir com a
// próxima batida em nenhum andamento musicalmente razoável (a 300 bpm, o
// tempo mais rápido de qualquer repertório comum, uma batida já dura
// 200 ms — bem mais que os 30 ms do clique).
constexpr double kClickDurationSeconds = 0.03;

constexpr float kNormalClickFrequency = 1000.0f;
constexpr float kAccentedClickFrequency = 1500.0f;
}

Metronome::Metronome(const MasterTransport& transport)
    : m_transport(transport)
{
}

void Metronome::prepare(double sampleRate)
{
    m_sampleRate = sampleRate;
    reset();
}

void Metronome::reset()
{
    m_clickSampleIndex = 0;
    m_clickLengthSamples = 0;
    m_lastTriggeredBeat = -1;
}

void Metronome::setVolume(float volume)
{
    m_volume = volume;
}

float Metronome::volume() const
{
    return m_volume;
}

void Metronome::setBeatsPerBar(int beatsPerBar)
{
    m_beatsPerBar = beatsPerBar;
}

int Metronome::beatsPerBar() const
{
    return m_beatsPerBar;
}

void Metronome::triggerClick(bool accented)
{
    m_clickFrequency = accented ? kAccentedClickFrequency : kNormalClickFrequency;
    m_clickSampleIndex = 0;
    m_clickLengthSamples = static_cast<std::size_t>(m_sampleRate * kClickDurationSeconds);
}

float Metronome::renderClickSample()
{
    const double phase = kTwoPi * static_cast<double>(m_clickFrequency)
                        * static_cast<double>(m_clickSampleIndex) / m_sampleRate;

    // Envelope linear decrescente: o clique nasce no pico e se apaga até
    // zero ao fim da duração — sem isso, o corte abrupto no fim soaria
    // como um estalo próprio, por cima do clique.
    const float envelope = 1.0f - static_cast<float>(m_clickSampleIndex)
                                 / static_cast<float>(m_clickLengthSamples);

    ++m_clickSampleIndex;

    return static_cast<float>(std::sin(phase)) * envelope;
}

void Metronome::process(std::vector<float>& buffer, std::uint64_t startPositionSamples)
{
    const float bpm = m_transport.bpm();
    const bool playing = m_transport.isPlaying();
    const double samplesPerBeat = (bpm > 0.0f) ? (m_sampleRate * 60.0 / static_cast<double>(bpm)) : 0.0;

    for (std::size_t i = 0; i < buffer.size(); ++i)
    {
        if (playing && samplesPerBeat > 0.0)
        {
            const std::uint64_t position = startPositionSamples + i;

            // A batida mais próxima (pra trás) desta amostra. Derivado
            // sempre da posição absoluta, nunca acumulado bloco a bloco —
            // isso é o que evita o relógio do metrônomo derivar quando
            // samplesPerBeat não é um número inteiro de amostras.
            const std::int64_t beatIndex =
                static_cast<std::int64_t>(std::floor(static_cast<double>(position) / samplesPerBeat));
            const std::uint64_t beatSample =
                static_cast<std::uint64_t>(std::llround(static_cast<double>(beatIndex) * samplesPerBeat));

            if (beatSample == position && beatIndex != m_lastTriggeredBeat)
            {
                triggerClick(m_beatsPerBar > 0 && (beatIndex % m_beatsPerBar) == 0);
                m_lastTriggeredBeat = beatIndex;
            }
        }

        if (m_clickSampleIndex < m_clickLengthSamples)
        {
            buffer[i] += renderClickSample() * m_volume;
        }
    }
}
