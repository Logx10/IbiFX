#include "BackingTrackPlayer.h"

#include <stdexcept>
#include <utility>

#include "Resampler.h"
#include "WavFile.h"

void BackingTrackPlayer::prepare(double sampleRate)
{
    if (sampleRate <= 0.0)
        throw std::invalid_argument("sample rate da backing track precisa ser positivo");

    if (sampleRate == m_sampleRate)
        return;

    if (!m_originalSamples.empty())
        m_samples = resample(m_originalSamples, m_fileSampleRate, sampleRate);
    m_sampleRate = sampleRate;
}

void BackingTrackPlayer::load(const std::string& path)
{
    // wav::read já lança com mensagem clara se o arquivo não existir, não
    // for um .wav válido, ou usar um formato de amostra fora do suportado.
    const WavFile file = wav::read(path);

    // Reduz a mono pela média dos canais — mesma operação de IRLoader.cpp,
    // escrita de novo aqui em vez de reaproveitada: são só duas ocorrências
    // até agora, e a regra do projeto (AI_GUIDELINES §55) é esperar a
    // terceira antes de extrair um nome genérico pras duas.
    std::vector<float> mono(file.frameCount(), 0.0f);

    for (const auto& channel : file.channels)
    {
        for (std::size_t i = 0; i < mono.size(); ++i)
        {
            mono[i] += channel[i];
        }
    }

    const float channelCountInverse = 1.0f / static_cast<float>(file.channelCount());

    for (float& sample : mono)
    {
        sample *= channelCountInverse;
    }

    std::vector<float> converted = resample(mono, file.sampleRate, m_sampleRate);
    m_fileSampleRate = file.sampleRate;
    m_originalSamples = std::move(mono);
    m_samples = std::move(converted);
}

bool BackingTrackPlayer::isLoaded() const
{
    return !m_samples.empty();
}

std::uint64_t BackingTrackPlayer::lengthSamples() const
{
    return m_samples.size();
}

double BackingTrackPlayer::lengthSeconds() const
{
    if (m_sampleRate <= 0.0)
        return 0.0;

    return static_cast<double>(m_samples.size()) / m_sampleRate;
}

void BackingTrackPlayer::setVolume(float volume)
{
    m_volume = volume;
}

float BackingTrackPlayer::volume() const
{
    return m_volume;
}

void BackingTrackPlayer::process(std::vector<float>& buffer, std::uint64_t startPositionSamples)
{
    if (m_samples.empty())
        return;

    for (std::size_t i = 0; i < buffer.size(); ++i)
    {
        const std::uint64_t position = startPositionSamples + i;

        if (position < m_samples.size())
        {
            buffer[i] += m_samples[position] * m_volume;
        }
        // Fora do arquivo: silêncio, a amostra do buffer fica intocada.
    }
}
