#include "ReampRecorder.h"

void ReampRecorder::prepare(double sampleRate)
{
    m_dryRecorder.prepare(sampleRate);
    m_processedRecorder.prepare(sampleRate);
}

void ReampRecorder::start(const std::string& dryPath, const std::string& processedPath)
{
    // Se qualquer um dos dois já estiver gravando (de um start() anterior
    // sem stop()), Recorder::start() lança — e nenhum dos dois chega a
    // (re)marcar m_active, então o estado desta classe não fica
    // inconsistente com o dos dois Recorder internos.
    m_dryRecorder.start(dryPath);
    m_processedRecorder.start(processedPath);

    m_active.store(true, std::memory_order_release);
}

void ReampRecorder::stop()
{
    // exchange garante que uma segunda chamada (ou uma concorrente) não
    // repita a parada — stop() duas vezes é seguro e idempotente, mesma
    // garantia que Recorder::stop() já dava sozinho.
    if (!m_active.exchange(false, std::memory_order_acq_rel))
    {
        return;
    }

    // As DUAS precisam parar de aceitar amostras novas antes de QUALQUER
    // uma começar a esperar sua thread de disco — ver o comentário da
    // classe sobre o bug que isso corrige.
    m_dryRecorder.requestStop();
    m_processedRecorder.requestStop();

    m_dryRecorder.finishStop();
    m_processedRecorder.finishStop();
}

bool ReampRecorder::isRecording() const
{
    return m_active.load(std::memory_order_acquire);
}

void ReampRecorder::pushBlock(const float* dry, const float* processed, std::size_t frameCount)
{
    if (!m_active.load(std::memory_order_acquire))
    {
        return;
    }

    m_dryRecorder.pushSamples(dry, frameCount);
    m_processedRecorder.pushSamples(processed, frameCount);
}
