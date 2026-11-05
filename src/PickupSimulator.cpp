#include "PickupSimulator.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace
{
constexpr double kPi = 3.14159265358979323846;
// Medianas dos ajustes das curvas -LD.csv do pickupAnalyseR-data (200 kOhm,
// 470 pF). Reproduziveis com tools/fit_pickup_profiles.py. Nao incluem a
// captacao espacial da corda nem a carga exata da guitarra do usuario.
constexpr double kSingleHz = 2850.0;
constexpr double kSingleQ = 1.14;
constexpr double kHumbuckerHz = 1900.0;
constexpr double kHumbuckerQ = 1.11;
// Diferenca de nivel aproximada: as curvas de resposta eletrica medidas nao
// calibram a sensibilidade magnetica absoluta entre captadores distintos.
constexpr double kLevelDifferenceDb = 5.0;
}

float PickupSimulator::Biquad::process(float sample)
{
    const double output = b0 * sample + z1;
    z1 = b1 * sample - a1 * output + z2;
    z2 = b2 * sample - a2 * output;
    return static_cast<float>(output);
}

void PickupSimulator::Biquad::reset()
{
    z1 = z2 = 0.0;
}

PickupSimulator::Biquad PickupSimulator::makeConversion(double sampleRate,
                                                        double sourceHz, double sourceQ,
                                                        double targetHz, double targetQ,
                                                        double levelDb)
{
    // A resposta eletrica de cada captador e aproximada por um passa-baixas
    // ressonante de segunda ordem. A conversao aplica H_alvo / H_origem:
    // zeros retiram a ressonancia da origem; polos inserem a do destino.
    // Prewarp preserva a frequencia das duas ressonancias na transformacao
    // bilinear para qualquer taxa de amostragem suportada.
    const double source = 2.0 * sampleRate * std::tan(kPi * sourceHz / sampleRate);
    const double target = 2.0 * sampleRate * std::tan(kPi * targetHz / sampleRate);
    const double k = 2.0 * sampleRate;
    const double gain = std::pow(10.0, levelDb / 20.0);
    const double ratio = gain * target * target / (source * source);

    const double a0 = k * k + target / targetQ * k + target * target;
    Biquad filter;
    filter.b0 = ratio * (k * k + source / sourceQ * k + source * source) / a0;
    filter.b1 = ratio * (-2.0 * k * k + 2.0 * source * source) / a0;
    filter.b2 = ratio * (k * k - source / sourceQ * k + source * source) / a0;
    filter.a1 = (-2.0 * k * k + 2.0 * target * target) / a0;
    filter.a2 = (k * k - target / targetQ * k + target * target) / a0;
    return filter;
}

void PickupSimulator::prepare(double sampleRate)
{
    const double rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    m_singleToHumbucker = makeConversion(rate, kSingleHz, kSingleQ,
                                         kHumbuckerHz, kHumbuckerQ, kLevelDifferenceDb);
    m_humbuckerToSingle = makeConversion(rate, kHumbuckerHz, kHumbuckerQ,
                                         kSingleHz, kSingleQ, -kLevelDifferenceDb);
    m_activeMode = m_mode.load(std::memory_order_relaxed);
    std::fill(std::begin(m_weights), std::end(m_weights), 0.0);
    m_weights[static_cast<int>(m_activeMode)] = 1.0;
    m_transition = 1.0 / std::max(1.0, rate * 0.01);
}

void PickupSimulator::setMode(Mode mode)
{
    m_mode.store(mode, std::memory_order_relaxed);
}

PickupSimulator::Mode PickupSimulator::mode() const
{
    return m_mode.load(std::memory_order_relaxed);
}

void PickupSimulator::process(std::vector<float>& buffer)
{
    const Mode target = m_mode.load(std::memory_order_relaxed);
    const int selected = static_cast<int>(target);

    // Os dois filtros acompanham continuamente a entrada, mesmo em Original.
    // Ao mudar de modo, o estado ja corresponde ao sinal recente. O peso
    // muda em 10 ms sem alocacao nem operacao na thread de controle.
    for (float& sample : buffer)
    {
        const float original = sample;
        const float humbucker = m_singleToHumbucker.process(original);
        const float single = m_humbuckerToSingle.process(original);

        if (target != m_activeMode || m_weights[selected] < 1.0)
        {
            for (int i = 0; i < 3; ++i)
            {
                const double destination = i == selected ? 1.0 : 0.0;
                const double remaining = destination - m_weights[i];
                m_weights[i] += std::clamp(remaining, -m_transition, m_transition);
            }
        }

        sample = static_cast<float>(m_weights[0] * original
                                  + m_weights[1] * humbucker
                                  + m_weights[2] * single);
    }

    m_activeMode = target;
}
