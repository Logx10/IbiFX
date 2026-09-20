#include "IRLoader.h"

#include <stdexcept>

#include "WavFile.h"

std::vector<float> loadImpulseResponse(const std::string& path)
{
    // wav::read já lança com mensagem clara se o arquivo não existir, não
    // for um .wav válido, ou usar um formato de amostra fora do suportado —
    // não precisa duplicar essa checagem aqui.
    const WavFile file = wav::read(path);

    if (file.frameCount() == 0)
    {
        throw std::runtime_error("IR '" + path + "' esta vazia");
    }

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

    return mono;
}
