#include "PracticeSession.h"

PracticeSession::PracticeSession()
    : m_metronome(m_transport)
{
}

void PracticeSession::prepare(double sampleRate)
{
    m_transport.prepare(sampleRate);
    m_backingTrack.prepare(sampleRate);
    m_metronome.prepare(sampleRate);
    m_recorder.prepare(sampleRate);
}

void PracticeSession::loadBackingTrack(const std::string& path)
{
    // BackingTrackPlayer::load() já lança se o sample rate do arquivo não
    // bater com o que prepare() informou — nada a duplicar aqui.
    m_backingTrack.load(path);
}

void PracticeSession::setBackingTrackVolume(float volume)
{
    m_backingTrack.setVolume(volume);
}

bool PracticeSession::hasBackingTrack() const
{
    return m_backingTrack.isLoaded();
}

std::uint64_t PracticeSession::backingTrackLengthSamples() const
{
    return m_backingTrack.lengthSamples();
}

void PracticeSession::play()
{
    m_transport.play();
}

void PracticeSession::stop()
{
    m_transport.stop();
}

bool PracticeSession::isPlaying() const
{
    return m_transport.isPlaying();
}

void PracticeSession::seek(std::uint64_t positionSamples)
{
    m_transport.seek(positionSamples);
}

void PracticeSession::setLoop(std::uint64_t startSamples, std::uint64_t endSamples)
{
    m_transport.setLoop(startSamples, endSamples);
}

void PracticeSession::setLoopEnabled(bool enabled)
{
    m_transport.setLoopEnabled(enabled);
}

bool PracticeSession::isLoopEnabled() const
{
    return m_transport.isLoopEnabled();
}

std::uint64_t PracticeSession::loopStartSamples() const
{
    return m_transport.loopStartSamples();
}

std::uint64_t PracticeSession::loopEndSamples() const
{
    return m_transport.loopEndSamples();
}

std::uint64_t PracticeSession::positionSamples() const
{
    return m_transport.positionSamples();
}

void PracticeSession::setBpm(float bpm)
{
    m_transport.setBpm(bpm);
}

float PracticeSession::bpm() const
{
    return m_transport.bpm();
}

void PracticeSession::setMetronomeEnabled(bool enabled)
{
    m_metronomeEnabled.store(enabled, std::memory_order_relaxed);
}

bool PracticeSession::isMetronomeEnabled() const
{
    return m_metronomeEnabled.load(std::memory_order_relaxed);
}

void PracticeSession::setMetronomeVolume(float volume)
{
    m_metronome.setVolume(volume);
}

void PracticeSession::startRecording(const std::string& path)
{
    m_recorder.start(path);
}

void PracticeSession::stopRecording()
{
    m_recorder.stop();
}

bool PracticeSession::isRecording() const
{
    return m_recorder.isRecording();
}

void PracticeSession::process(std::vector<float>& buffer)
{
    // Lida ANTES de advance(): é a posição do PRIMEIRO frame deste bloco,
    // o mesmo contrato que BackingTrackPlayer::process() e
    // Metronome::process() já esperam.
    const std::uint64_t startPosition = m_transport.positionSamples();

    m_backingTrack.process(buffer, startPosition);

    const bool metronomeEnabled = m_metronomeEnabled.load(std::memory_order_relaxed);

    if (metronomeEnabled)
    {
        m_metronome.process(buffer, startPosition);
    }
    else if (m_metronomeWasEnabled)
    {
        // Descarta qualquer clique em andamento — sem isso, desligar o
        // metrônomo no meio de um clique deixaria ele tocando até o fim
        // sozinho, porque o estado de renderização continuaria ativo.
        m_metronome.reset();
    }

    m_metronomeWasEnabled = metronomeEnabled;

    // Grava o resultado já com backing track e clique somados — ver o
    // comentário no header sobre essa escolha.
    m_recorder.pushSamples(buffer.data(), buffer.size());

    m_transport.advance(buffer.size());
}
