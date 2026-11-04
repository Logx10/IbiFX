#include "GraphicEQ.h"

#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;

// Uma oitava entre cada banda: cada frequência é o dobro da anterior.
constexpr float kFrequencies[GraphicEQ::kBandCount] = {100.0f, 200.0f, 400.0f, 800.0f,
                                                       1600.0f, 3200.0f, 6400.0f};
constexpr const char* kIds[GraphicEQ::kBandCount] = {"band100", "band200", "band400", "band800",
                                                     "band1600", "band3200", "band6400"};
constexpr const char* kLabels[GraphicEQ::kBandCount] = {"100", "200", "400", "800",
                                                        "1.6k", "3.2k", "6.4k"};

// Largura de uma oitava — ver o header.
constexpr double kQ = 1.4;

constexpr float kMaxGainDb = 15.0f;

// O Level vem depois das sete bandas na lista de parâmetros.
constexpr std::size_t kLevelIndex = GraphicEQ::kBandCount;
}

GraphicEQ::GraphicEQ()
{
    for (std::size_t band = 0; band < kBandCount; ++band)
        m_parameters.emplace_back(kIds[band], kLabels[band], -kMaxGainDb, kMaxGainDb, 0.0f);

    m_parameters.emplace_back("level", "Level", -kMaxGainDb, kMaxGainDb, 0.0f);
}

void GraphicEQ::setBandGain(std::size_t band, float gainDb)
{
    if (band < kBandCount)
        m_parameters[band].setValue(gainDb);
}

float GraphicEQ::bandGain(std::size_t band) const
{
    return band < kBandCount ? m_parameters[band].value() : 0.0f;
}

float GraphicEQ::bandFrequency(std::size_t band)
{
    return band < kBandCount ? kFrequencies[band] : 0.0f;
}

void GraphicEQ::setLevel(float levelDb)
{
    m_parameters[kLevelIndex].setValue(levelDb);
}

float GraphicEQ::level() const
{
    return m_parameters[kLevelIndex].value();
}

const char* GraphicEQ::name() const
{
    return "GraphicEQ";
}

void GraphicEQ::prepare(double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    updateCoefficients();
}

void GraphicEQ::reset()
{
    for (Band& band : m_bands)
        band.x1 = band.x2 = band.y1 = band.y2 = 0.0;
}

// As fórmulas do peaking EQ do "Audio EQ Cookbook" (Bristow-Johnson),
// com os mesmos nomes do texto — ver o header.
void GraphicEQ::updateCoefficients()
{
    for (std::size_t i = 0; i < kBandCount; ++i)
    {
        const double gainDb = static_cast<double>(m_parameters[i].value());

        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * static_cast<double>(kFrequencies[i]) / m_sampleRate;
        const double alpha = std::sin(w0) / (2.0 * kQ);
        const double cosW0 = std::cos(w0);

        const double a0 = 1.0 + alpha / A;

        Band& band = m_bands[i];
        band.b0 = (1.0 + alpha * A) / a0;
        band.b1 = (-2.0 * cosW0) / a0;
        band.b2 = (1.0 - alpha * A) / a0;
        band.a1 = (-2.0 * cosW0) / a0;
        band.a2 = (1.0 - alpha / A) / a0;
    }
}

void GraphicEQ::process(std::vector<float>& buffer)
{
    // Uma vez por bloco, não por amostra — ver a LIMITAÇÃO no header.
    updateCoefficients();

    const double levelGain = std::pow(10.0, static_cast<double>(m_parameters[kLevelIndex].value()) / 20.0);

    for (float& sample : buffer)
    {
        double x0 = static_cast<double>(sample);

        // A saída de cada banda é a entrada da próxima.
        for (Band& band : m_bands)
        {
            const double y0 = band.b0 * x0 + band.b1 * band.x1 + band.b2 * band.x2
                            - band.a1 * band.y1 - band.a2 * band.y2;

            band.x2 = band.x1;
            band.x1 = x0;
            band.y2 = band.y1;
            band.y1 = y0;

            x0 = y0;
        }

        sample = static_cast<float>(x0 * levelGain);
    }
}
