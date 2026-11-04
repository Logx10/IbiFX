#include "IRLoader.h"

#include <cmath>
#include <complex>
#include <stdexcept>
#include <utility>

#include "WavFile.h"

ImpulseResponse readImpulseResponse(const std::string& path)
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

    return {std::move(mono), file.sampleRate};
}

std::vector<float> loadImpulseResponse(const std::string& path)
{
    return readImpulseResponse(path).samples;
}

void normalizeImpulseResponseLevel(std::vector<float>& samples, double sampleRate)
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kLowestHz = 80.0;
    constexpr double kHighestHz = 5000.0;
    constexpr double kPointsPerOctave = 12.0;

    double powerSum = 0.0;
    int points = 0;

    for (double frequency = kLowestHz; frequency <= kHighestHz;
         frequency *= std::pow(2.0, 1.0 / kPointsPerOctave))
    {
        // Acima de Nyquist a frequência não existe na IR.
        if (frequency >= 0.5 * sampleRate)
            break;

        // H(f) = Σ h[n] e^(-jωn). Em vez de um cos/sin por tap, o fasor
        // e^(-jωn) é girado de um passo a cada amostra — uma multiplicação
        // complexa, e conta em double para o erro não acumular em IRs
        // longas (uma de 500 ms a 96 kHz tem 48 mil taps).
        const std::complex<double> step = std::polar(1.0, -2.0 * kPi * frequency / sampleRate);
        std::complex<double> phasor = 1.0;
        std::complex<double> response = 0.0;

        for (float tap : samples)
        {
            response += static_cast<double>(tap) * phasor;
            phasor *= step;
        }

        powerSum += std::norm(response);  // |H|²
        ++points;
    }

    if (points == 0 || powerSum <= 0.0)
        return;

    const float gain = static_cast<float>(1.0 / std::sqrt(powerSum / points));
    for (float& tap : samples)
        tap *= gain;
}
