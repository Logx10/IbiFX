#include "offline.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace offline
{
WavFile processFile(const WavFile& input, ModuleChain& chain, int blockSize)
{
    if (blockSize <= 0)
    {
        throw std::invalid_argument("blockSize precisa ser positivo");
    }

    WavFile output;
    output.sampleRate = input.sampleRate;
    output.channels.reserve(input.channelCount());

    chain.prepare(input.sampleRate, blockSize);

    const std::size_t block = static_cast<std::size_t>(blockSize);

    // Um buffer reaproveitado por todos os blocos e canais. Alocar aqui, e
    // não dentro do laço, é o mesmo hábito exigido pelo tempo real — mesmo
    // que aqui não fosse obrigatório.
    std::vector<float> scratch;
    scratch.reserve(block);

    for (const std::vector<float>& channel : input.channels)
    {
        // Sem este reset, o eco do canal anterior continuaria dentro do
        // delay e vazaria para o próximo.
        chain.reset();

        std::vector<float> processed;
        processed.reserve(channel.size());

        for (std::size_t start = 0; start < channel.size(); start += block)
        {
            const std::size_t count = std::min(block, channel.size() - start);

            scratch.assign(channel.begin() + static_cast<std::ptrdiff_t>(start),
                           channel.begin() + static_cast<std::ptrdiff_t>(start + count));

            chain.process(scratch);

            processed.insert(processed.end(), scratch.begin(), scratch.end());
        }

        output.channels.push_back(std::move(processed));
    }

    return output;
}

WavFile generateTestSignal(double sampleRate, double seconds)
{
    if (sampleRate <= 0.0 || seconds <= 0.0)
    {
        throw std::invalid_argument("sample rate e duracao precisam ser positivos");
    }

    const std::size_t frameCount = static_cast<std::size_t>(sampleRate * seconds);

    WavFile file;
    file.sampleRate = sampleRate;
    file.channels.assign(1, std::vector<float>(frameCount, 0.0f));

    // Quatro notas, uma por vez, subindo — dá para ouvir se o efeito trata
    // graves e agudos de forma diferente.
    const double frequencies[] = {110.0, 146.83, 196.0, 246.94};
    const std::size_t notes = sizeof(frequencies) / sizeof(frequencies[0]);
    const std::size_t framesPerNote = frameCount / notes;

    const double twoPi = 6.283185307179586;

    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        const std::size_t note = std::min(frame / framesPerNote, notes - 1);
        const std::size_t frameInNote = frame - note * framesPerNote;

        const double t = static_cast<double>(frameInNote) / sampleRate;

        // Envelope de decaimento exponencial: ataque instantâneo e queda
        // gradual, como uma corda pinçada. É o que dá dinâmica ao sinal e
        // permite ouvir a saturação mudar conforme o volume cai.
        const double envelope = std::exp(-3.0 * t);

        const double fundamental = frequencies[note];

        // Fundamental mais dois harmônicos, cada um mais fraco. Uma senoide
        // pura soaria artificial demais e não mostraria bem o que a distorção
        // faz com o conteúdo harmônico.
        double value = std::sin(twoPi * fundamental * t)
                     + 0.5 * std::sin(twoPi * fundamental * 2.0 * t)
                     + 0.25 * std::sin(twoPi * fundamental * 3.0 * t);

        // Normaliza para não chegar perto do limite: o sinal precisa de
        // espaço para ser amplificado pelos efeitos sem estourar sozinho.
        value *= 0.4 * envelope;

        file.channels[0][frame] = static_cast<float>(value);
    }

    return file;
}
}
