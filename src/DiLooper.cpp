#include "DiLooper.h"

#include <algorithm>
#include <cmath>

namespace
{
// Abaixo disto (~-46 dBFS) é silêncio para o corte automático: acima do
// chiado de uma interface comum, abaixo de uma nota tocada fraco (~-30 dBFS,
// a "palhetada fraca" dos testes do catálogo).
constexpr float kSilenceThreshold = 0.005f;

// Folgas do corte: um pouco antes da primeira nota, para não comer o
// ataque, e um pouco depois da última, para a nota terminar de soar.
constexpr double kPreRollSeconds = 0.02;
constexpr double kPostRollSeconds = 0.2;

constexpr double kFadeSeconds = 0.005;
}

void DiLooper::prepare(double sampleRate)
{
    if (sampleRate <= 0.0)
        sampleRate = 48000.0;

    m_fadeLength = std::max<std::size_t>(1, static_cast<std::size_t>(kFadeSeconds * sampleRate));

    // Mesma taxa: o take continua válido. Taxa diferente: as amostras
    // gravadas tocariam mais rápidas ou mais lentas, então recomeça vazio.
    if (sampleRate == m_sampleRate && !m_buffer.empty())
        return;

    m_sampleRate = sampleRate;
    m_buffer.assign(static_cast<std::size_t>(std::ceil(kMaxSeconds * sampleRate)), 0.0f);

    m_recorded = 0;
    m_takeStart = 0;
    m_takeEnd = 0;
    m_playhead = 0;
    m_heardSomething = false;
    m_stopFadeRemaining = 0;
    m_request.store(Request::None);
    m_state.store(State::Empty);
    m_publishedTakeLength.store(0);
    m_publishedRecorded.store(0);
    m_publishedPlayhead.store(0);
}

void DiLooper::request(Request request)
{
    m_request.store(request, std::memory_order_release);
}

void DiLooper::startRecording()
{
    request(Request::StartRecording);
}

void DiLooper::stopRecording()
{
    request(Request::StopRecording);
}

void DiLooper::play()
{
    request(Request::Play);
}

void DiLooper::stop()
{
    request(Request::Stop);
}

void DiLooper::togglePlay()
{
    request(Request::TogglePlay);
}

void DiLooper::clear()
{
    request(Request::Clear);
}

void DiLooper::setLoop(bool loop)
{
    m_loop.store(loop, std::memory_order_relaxed);
}

bool DiLooper::loop() const
{
    return m_loop.load(std::memory_order_relaxed);
}

DiLooper::State DiLooper::state() const
{
    return m_state.load(std::memory_order_acquire);
}

double DiLooper::takeSeconds() const
{
    return m_sampleRate > 0.0 ? static_cast<double>(m_publishedTakeLength.load()) / m_sampleRate : 0.0;
}

double DiLooper::recordedSeconds() const
{
    return m_sampleRate > 0.0 ? static_cast<double>(m_publishedRecorded.load()) / m_sampleRate : 0.0;
}

double DiLooper::playheadSeconds() const
{
    return m_sampleRate > 0.0 ? static_cast<double>(m_publishedPlayhead.load()) / m_sampleRate : 0.0;
}

void DiLooper::finishRecording()
{
    m_publishedRecorded.store(m_recorded);

    // Nada acima do limiar: não há o que tocar.
    if (!m_heardSomething)
    {
        m_takeStart = m_takeEnd = 0;
        m_publishedTakeLength.store(0);
        m_state.store(State::Empty, std::memory_order_release);
        return;
    }

    const auto preRoll = static_cast<std::size_t>(kPreRollSeconds * m_sampleRate);
    const auto postRoll = static_cast<std::size_t>(kPostRollSeconds * m_sampleRate);

    m_takeStart = m_firstLoud > preRoll ? m_firstLoud - preRoll : 0;
    m_takeEnd = std::min(m_recorded, m_lastLoud + 1 + postRoll);
    m_playhead = m_takeStart;

    m_publishedTakeLength.store(m_takeEnd - m_takeStart);
    m_publishedPlayhead.store(0);
    m_state.store(State::Ready, std::memory_order_release);
}

void DiLooper::applyRequest(Request request)
{
    const State current = m_state.load(std::memory_order_relaxed);

    // Tocar enquanto grava é o fluxo natural — grava o riff, aperta espaço
    // e já ouve: fecha o take primeiro.
    const bool wantsPlay = request == Request::Play
                           || (request == Request::TogglePlay && current != State::Playing);
    if (current == State::Recording
        && (wantsPlay || request == Request::StopRecording || request == Request::Stop))
    {
        finishRecording();
    }

    switch (request)
    {
        case Request::None:
        case Request::StopRecording:
            break;

        case Request::StartRecording:
            if (current != State::Recording)
            {
                m_recorded = 0;
                m_heardSomething = false;
                m_stopFadeRemaining = 0;
                m_publishedRecorded.store(0);
                m_state.store(State::Recording, std::memory_order_release);
            }
            break;

        case Request::Stop:
            if (current == State::Playing)
                m_stopFadeRemaining = m_fadeLength;
            break;

        case Request::Play:
        case Request::TogglePlay:
            if (request == Request::TogglePlay && current == State::Playing)
            {
                if (m_stopFadeRemaining == 0)
                    m_stopFadeRemaining = m_fadeLength;
                break;
            }
            if (m_takeEnd > m_takeStart)
            {
                // Sempre do começo: para comparar dois timbres, o riff tem
                // que começar igual.
                m_playhead = m_takeStart;
                m_stopFadeRemaining = 0;
                m_publishedPlayhead.store(0);
                m_state.store(State::Playing, std::memory_order_release);
            }
            break;

        case Request::Clear:
            m_recorded = 0;
            m_takeStart = m_takeEnd = 0;
            m_playhead = 0;
            m_stopFadeRemaining = 0;
            m_publishedTakeLength.store(0);
            m_publishedRecorded.store(0);
            m_publishedPlayhead.store(0);
            m_state.store(State::Empty, std::memory_order_release);
            break;
    }
}

float DiLooper::edgeGain(std::size_t index) const
{
    const auto fromStart = static_cast<float>(index - m_takeStart + 1);
    const auto toEnd = static_cast<float>(m_takeEnd - index);
    const auto fade = static_cast<float>(m_fadeLength);
    return std::min({1.0f, fromStart / fade, toEnd / fade});
}

void DiLooper::process(float* mono, std::size_t frames)
{
    const Request pending = m_request.exchange(Request::None, std::memory_order_acq_rel);
    if (pending != Request::None)
        applyRequest(pending);

    const State current = m_state.load(std::memory_order_relaxed);

    if (current == State::Recording)
    {
        for (std::size_t i = 0; i < frames; ++i)
        {
            // Buffer cheio: o take fecha sozinho com o que coube.
            if (m_recorded >= m_buffer.size())
            {
                finishRecording();
                break;
            }

            const float sample = mono[i];
            m_buffer[m_recorded] = sample;

            if (std::fabs(sample) > kSilenceThreshold)
            {
                if (!m_heardSomething)
                {
                    m_firstLoud = m_recorded;
                    m_heardSomething = true;
                }
                m_lastLoud = m_recorded;
            }
            ++m_recorded;
        }
        m_publishedRecorded.store(m_recorded);
        return;
    }

    if (current != State::Playing)
        return;

    for (std::size_t i = 0; i < frames; ++i)
    {
        if (m_playhead >= m_takeEnd)
        {
            if (!m_loop.load(std::memory_order_relaxed))
            {
                // Fim sem loop: daqui em diante volta a guitarra ao vivo.
                m_playhead = m_takeStart;
                m_state.store(State::Ready, std::memory_order_release);
                break;
            }
            m_playhead = m_takeStart;
        }

        float gain = edgeGain(m_playhead);

        if (m_stopFadeRemaining > 0)
        {
            gain *= static_cast<float>(m_stopFadeRemaining) / static_cast<float>(m_fadeLength);
            if (--m_stopFadeRemaining == 0)
            {
                mono[i] = 0.0f;
                m_playhead = m_takeStart;
                m_state.store(State::Ready, std::memory_order_release);
                break;
            }
        }

        mono[i] = m_buffer[m_playhead] * gain;
        ++m_playhead;
    }

    m_publishedPlayhead.store(m_playhead - m_takeStart);
}
