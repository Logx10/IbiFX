#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

#include "Rotovibe.h"
#include "WavFile.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kDuration = 6.0;
constexpr double kTwoPi = 6.283185307179586;
constexpr std::size_t kBlockSize = 128;

WavFile makeSource()
{
    WavFile file;
    file.sampleRate = kSampleRate;
    file.channels.resize(1);
    auto& samples = file.channels[0];
    samples.resize(static_cast<std::size_t>(kSampleRate * kDuration), 0.0f);

    // Dois acordes de quatro cordas, dedilhados com pequenos atrasos.
    // Os harmônicos superiores decaem mais rápido que a fundamental.
    constexpr double chords[2][4] = {{110.0, 164.81, 220.0, 277.18},
                                      {98.0, 146.83, 196.0, 246.94}};
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        const double time = static_cast<double>(i) / kSampleRate;
        double output = 0.0;
        for (int chord = 0; chord < 2; ++chord)
        {
            for (int string = 0; string < 4; ++string)
            {
                const double age = time - (0.12 + 3.0 * chord + 0.018 * string);
                if (age < 0.0)
                    continue;

                const double attack = std::min(age / 0.004, 1.0);
                const double frequency = chords[chord][string];
                for (int harmonic = 1; harmonic <= 7; ++harmonic)
                {
                    const double level = 0.14 / harmonic;
                    const double decay = std::exp(-(0.45 + 0.28 * harmonic) * age);
                    output += level * attack * decay
                              * std::sin(kTwoPi * frequency * harmonic * age);
                }
            }
        }
        // Evita um corte brusco no fim dos arquivos.
        const double fade = std::clamp((kDuration - time) / 0.15, 0.0, 1.0);
        samples[i] = static_cast<float>(output * fade);
    }
    return file;
}

WavFile render(const WavFile& source, const std::string& path,
               float mode, float position, bool moveTreadle)
{
    Rotovibe effect;
    effect.findParameter("mode")->setValue(mode);
    effect.findParameter("position")->setValue(position);
    effect.findParameter("intensity")->setValue(1.0f);
    effect.prepare(kSampleRate, static_cast<int>(kBlockSize));

    WavFile result = source;
    auto& output = result.channels[0];
    for (std::size_t start = 0; start < output.size(); start += kBlockSize)
    {
        const std::size_t count = std::min(kBlockSize, output.size() - start);
        if (moveTreadle)
        {
            const float fraction = static_cast<float>(start) / static_cast<float>(output.size());
            effect.findParameter("position")->setValue(0.05f + 0.95f * fraction);
        }
        std::vector<float> block(output.begin() + static_cast<std::ptrdiff_t>(start),
                                 output.begin() + static_cast<std::ptrdiff_t>(start + count));
        effect.process(block);
        std::copy(block.begin(), block.end(), output.begin() + static_cast<std::ptrdiff_t>(start));
    }
    wav::write(path, result);
    return result;
}
}

int main()
{
    const std::filesystem::path directory = "audio/previews";
    std::filesystem::create_directories(directory);
    const WavFile dry = makeSource();
    wav::write((directory / "rotovibe_01_sem_efeito.wav").string(), dry);
    const WavFile chorus = render(dry, (directory / "rotovibe_02_chorus.wav").string(),
                                  0.0f, 0.35f, false);
    const WavFile vibrato = render(dry, (directory / "rotovibe_03_vibrato.wav").string(),
                                   1.0f, 0.35f, false);
    const WavFile treadle = render(dry, (directory / "rotovibe_04_prancha.wav").string(),
                                   0.0f, 0.05f, true);

    WavFile comparison;
    comparison.sampleRate = kSampleRate;
    comparison.channels.resize(1);
    auto& sequence = comparison.channels[0];
    const std::size_t gap = static_cast<std::size_t>(0.4 * kSampleRate);
    for (const WavFile* segment : {&dry, &chorus, &vibrato, &treadle})
    {
        if (!sequence.empty())
            sequence.insert(sequence.end(), gap, 0.0f);
        sequence.insert(sequence.end(), segment->channels[0].begin(), segment->channels[0].end());
    }
    wav::write((directory / "rotovibe_comparativo.wav").string(), comparison);
    std::cout << "Prévia gerada em " << directory << '\n';
}
