#include "MasterTransport.h"

void MasterTransport::prepare(double sampleRate)
{
    m_sampleRate.store(sampleRate, std::memory_order_relaxed);
}

void MasterTransport::play()
{
    m_playing.store(true, std::memory_order_relaxed);
}

void MasterTransport::stop()
{
    // Só para o relógio. A posição fica onde estava — tocar de novo
    // continua dali, não do início, como qualquer transporte de DAW.
    m_playing.store(false, std::memory_order_relaxed);
}

bool MasterTransport::isPlaying() const
{
    return m_playing.load(std::memory_order_relaxed);
}

void MasterTransport::setRecording(bool recording)
{
    m_recording.store(recording, std::memory_order_relaxed);
}

bool MasterTransport::isRecording() const
{
    return m_recording.load(std::memory_order_relaxed);
}

void MasterTransport::setBpm(float bpm)
{
    m_bpm.store(bpm, std::memory_order_relaxed);
}

float MasterTransport::bpm() const
{
    return m_bpm.load(std::memory_order_relaxed);
}

void MasterTransport::setLoop(std::uint64_t startSamples, std::uint64_t endSamples)
{
    m_loopStartSamples.store(startSamples, std::memory_order_relaxed);
    m_loopEndSamples.store(endSamples, std::memory_order_relaxed);
}

void MasterTransport::setLoopEnabled(bool enabled)
{
    m_loopEnabled.store(enabled, std::memory_order_relaxed);
}

bool MasterTransport::isLoopEnabled() const
{
    return m_loopEnabled.load(std::memory_order_relaxed);
}

std::uint64_t MasterTransport::loopStartSamples() const
{
    return m_loopStartSamples.load(std::memory_order_relaxed);
}

std::uint64_t MasterTransport::loopEndSamples() const
{
    return m_loopEndSamples.load(std::memory_order_relaxed);
}

void MasterTransport::seek(std::uint64_t positionSamples)
{
    m_seekTarget.store(positionSamples, std::memory_order_relaxed);
    m_seekPending.store(true, std::memory_order_release);
}

std::uint64_t MasterTransport::positionSamples() const
{
    return m_positionSamples.load(std::memory_order_relaxed);
}

double MasterTransport::positionSeconds() const
{
    const double sampleRate = m_sampleRate.load(std::memory_order_relaxed);

    if (sampleRate <= 0.0)
        return 0.0;

    return static_cast<double>(positionSamples()) / sampleRate;
}

double MasterTransport::positionBeats() const
{
    const float bpmValue = bpm();

    if (bpmValue <= 0.0f)
        return 0.0;

    // segundos por batida = 60 / bpm; batidas = segundos / segundos-por-batida.
    return positionSeconds() * (static_cast<double>(bpmValue) / 60.0);
}

void MasterTransport::advance(std::size_t frameCount)
{
    std::uint64_t position;

    // Um seek() pendente substitui a posição ANTES de somar o bloco — ver
    // o comentário da classe sobre por que isso evita perder o pedido.
    // exchange(false) consome a bandeira: só a próxima advance() depois de
    // um seek() novo volta a vê-la erguida.
    if (m_seekPending.exchange(false, std::memory_order_acq_rel))
    {
        position = m_seekTarget.load(std::memory_order_relaxed);
    }
    else
    {
        position = m_positionSamples.load(std::memory_order_relaxed);
    }

    if (m_playing.load(std::memory_order_relaxed))
    {
        position += frameCount;
    }

    if (m_loopEnabled.load(std::memory_order_relaxed))
    {
        const std::uint64_t loopStart = m_loopStartSamples.load(std::memory_order_relaxed);
        const std::uint64_t loopEnd = m_loopEndSamples.load(std::memory_order_relaxed);

        if (loopEnd > loopStart && position >= loopEnd)
        {
            // Volta para o início preservando o EXCESSO por módulo, não só
            // "pulando pro início": um bloco quase sempre atravessa a borda
            // do loop no meio, não exatamente em cima dela, e descartar essa
            // fração faria o loop encolher um pouco a cada volta.
            const std::uint64_t loopLength = loopEnd - loopStart;
            const std::uint64_t overshoot = (position - loopStart) % loopLength;
            position = loopStart + overshoot;
        }
    }

    m_positionSamples.store(position, std::memory_order_relaxed);
}
