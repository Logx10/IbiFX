#include "Recorder.h"

#include <chrono>
#include <stdexcept>

#include "WavFile.h"

Recorder::Recorder(std::size_t capacitySamples)
    // Uma posição sempre vazia, mesmo truque da CommandQueue, para
    // distinguir "cheio" de "vazio" sem um contador à parte.
    : m_ring(capacitySamples + 1, 0.0f)
{
}

Recorder::~Recorder()
{
    if (m_recording.load(std::memory_order_relaxed))
    {
        stop();
    }
}

void Recorder::prepare(double sampleRate)
{
    m_sampleRate = sampleRate;
}

void Recorder::start(const std::string& path)
{
    if (m_recording.load(std::memory_order_relaxed))
    {
        throw std::runtime_error("Recorder::start chamado enquanto ja estava gravando");
    }

    m_path = path;
    m_recorded.clear();
    m_recordedCount.store(0, std::memory_order_relaxed);
    m_writeIndex.store(0, std::memory_order_relaxed);
    m_readIndex.store(0, std::memory_order_relaxed);
    m_stopRequested.store(false, std::memory_order_relaxed);

    m_recording.store(true, std::memory_order_release);
    m_diskThread = std::thread(&Recorder::diskThreadLoop, this);
}

void Recorder::pushSamples(const float* samples, std::size_t count)
{
    if (!m_recording.load(std::memory_order_relaxed))
    {
        return;
    }

    const std::size_t capacity = m_ring.size();
    std::size_t writeIndex = m_writeIndex.load(std::memory_order_relaxed);
    const std::size_t readIndex = m_readIndex.load(std::memory_order_acquire);

    for (std::size_t i = 0; i < count; ++i)
    {
        const std::size_t nextWrite = (writeIndex + 1) % capacity;

        if (nextWrite == readIndex)
        {
            // Cheio: descarta o resto deste bloco em vez de esperar a
            // thread de disco abrir espaço — ver o comentário da classe.
            break;
        }

        m_ring[writeIndex] = samples[i];
        writeIndex = nextWrite;
    }

    m_writeIndex.store(writeIndex, std::memory_order_release);
}

void Recorder::stop()
{
    if (!m_recording.load(std::memory_order_relaxed))
    {
        return;
    }

    // A ORDEM IMPORTA: desliga a aceitação de amostras novas primeiro, só
    // depois pede pra thread de disco encerrar — assim ela sabe que o que
    // já está no buffer circular neste instante é tudo que vai chegar.
    m_recording.store(false, std::memory_order_relaxed);
    m_stopRequested.store(true, std::memory_order_release);

    if (m_diskThread.joinable())
    {
        m_diskThread.join();
    }
}

bool Recorder::isRecording() const
{
    return m_recording.load(std::memory_order_relaxed);
}

std::size_t Recorder::recordedSampleCount() const
{
    return m_recordedCount.load(std::memory_order_relaxed);
}

void Recorder::diskThreadLoop()
{
    const std::size_t capacity = m_ring.size();

    while (true)
    {
        std::size_t readIndex = m_readIndex.load(std::memory_order_relaxed);
        const std::size_t writeIndex = m_writeIndex.load(std::memory_order_acquire);

        bool drainedAnything = false;

        while (readIndex != writeIndex)
        {
            m_recorded.push_back(m_ring[readIndex]);
            readIndex = (readIndex + 1) % capacity;
            drainedAnything = true;
        }

        if (drainedAnything)
        {
            m_readIndex.store(readIndex, std::memory_order_release);
            m_recordedCount.store(m_recorded.size(), std::memory_order_relaxed);
        }

        // Só sai quando pediram parada E não sobrou nada pendente — uma
        // última leva pode ter chegado depois do stopRequested mas antes
        // deste bloco rodar, e ela não pode ficar pra trás.
        if (m_stopRequested.load(std::memory_order_acquire) &&
            readIndex == m_writeIndex.load(std::memory_order_acquire))
        {
            break;
        }

        if (!drainedAnything)
        {
            // 1ms, não mais — quanto mais tempo dormindo, maior a rajada de
            // amostras que pode se acumular no buffer circular antes desta
            // thread notar e drenar de novo. Continua sendo uma fração
            // mínima de CPU (a thread passa a esmagadora maioria do tempo
            // dormindo, não girando), mas limita o pior caso de latência.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    WavFile file;
    file.sampleRate = m_sampleRate;
    file.channels = {m_recorded};

    wav::write(m_path, file);
}
