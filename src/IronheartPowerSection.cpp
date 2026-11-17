#include "IronheartPowerSection.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
float bounded(float x) { return std::isfinite(x) ? std::clamp(x, 0.0f, 1.0f) : 0.0f; }
}

void IronheartPowerSection::Branch::prepare(double rate, double sourceR) noexcept
{
    sourceResistance = sourceR;
    conductance = {2.0 * rate * 100e-9, 2.0 * rate * 10e-9, 2.0 * rate * 100e-9};
    reset();
}

void IronheartPowerSection::Branch::reset() noexcept
{
    history = {}; lastTone = lastWatts = -1.0;
}

std::array<double, 2> IronheartPowerSection::Branch::beginSample(double tone, double watts) noexcept
{
    const double inputY = conductance[0] / (1.0 + conductance[0] * sourceResistance);
    toneResistance = 220e3 * tone;
    const double toneY = conductance[1] / (1.0 + conductance[1] * toneResistance);
    if (tone != lastTone || watts != lastWatts)
    {
        const double upperG = 1.0 / std::max(1.0, 220e3 * (1.0 - watts));
        const double lowerG = 1.0 / (10e3 + 220e3 * watts);
        // R114/R118 terminam em bias desacoplado: terra em AC.
        const double a = inputY + toneY + upperG;
        const double b = upperG + lowerG + conductance[2];
        const double c = conductance[2] + 1.0 / 220e3;
        const double d = -upperG, e = -conductance[2];
        const double determinant = a * (b * c - e * e) - d * d * c;
        inverse = {{{(b*c-e*e)/determinant, -d*c/determinant, d*e/determinant},
                    {-d*c/determinant, a*c/determinant, -a*e/determinant},
                    {d*e/determinant, -a*e/determinant, (a*b-d*d)/determinant}}};
        lastTone = tone; lastWatts = watts;
        for (int n = 0; n < 3; ++n) response[n] = inverse[n][0] * inputY;
    }
    const double inputH = history[0] / (1.0 + conductance[0] * sourceResistance);
    const double toneH = history[1] / (1.0 + conductance[1] * toneResistance);
    const std::array<double, 3> rhs{-inputH + toneH, history[2], -history[2]};
    offset = {};
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col) offset[row] += inverse[row][col] * rhs[col];
    return {response[2], offset[2]};
}

double IronheartPowerSection::Branch::finishSample(double source) noexcept
{
    const double inputY = conductance[0] / (1.0 + conductance[0] * sourceResistance);
    const double toneY = conductance[1] / (1.0 + conductance[1] * toneResistance);
    const double inputH = history[0] / (1.0 + conductance[0] * sourceResistance);
    const double toneH = history[1] / (1.0 + conductance[1] * toneResistance);
    std::array<double, 3> v{};
    for (int n = 0; n < 3; ++n) v[n] = response[n] * source + offset[n];
    const double inputCurrent = inputY * (source - v[0]) - inputH;
    const double toneCurrent = toneY * v[0] - toneH;
    history[0] = 2.0 * conductance[0] * (source - v[0] - sourceResistance * inputCurrent) - history[0];
    history[1] = 2.0 * conductance[1] * (v[0] - toneResistance * toneCurrent) - history[1];
    history[2] = 2.0 * conductance[2] * (v[1] - v[2]) - history[2];
    return v[2];
}

void IronheartPowerSection::prepare(double rate, float tone, float watts, float dynamics)
{
    if (!std::isfinite(rate) || rate <= 0.0)
        throw std::invalid_argument("IronheartPowerSection exige taxa positiva e finita");
    m_rate = rate * Oversampler::kFactor;
    m_tone.prepare(m_rate, kDefaultRampSeconds, bounded(tone));
    m_watts.prepare(m_rate, kDefaultRampSeconds, bounded(watts));
    m_dynamics.prepare(m_rate, kDefaultRampSeconds, bounded(dynamics));
    // Resistências de Thévenin aproximadas das duas placas da inversora.
    // A assimetria de R112=82k/R113=100k permanece na rede reduzida.
    m_positive.prepare(m_rate, 35e3); m_negative.prepare(m_rate, 38e3);
    m_inputG = 1.0 / (2.0 * m_rate * 1e6 * 22e-9); // C65/R120
    m_attack = 1.0 - std::exp(-1.0 / (m_rate * .05));
    m_release = 1.0 - std::exp(-1.0 / (m_rate * .3));
    reset();
}

void IronheartPowerSection::setControls(float tone, float watts, float dynamics) noexcept
{
    m_tone.setTarget(bounded(tone)); m_watts.setTarget(bounded(watts));
    m_dynamics.setTarget(bounded(dynamics));
}

void IronheartPowerSection::reset() noexcept
{
    m_tone.snapTo(m_tone.target()); m_watts.snapTo(m_watts.target());
    m_dynamics.snapTo(m_dynamics.target());
    m_positive.reset(); m_negative.reset(); m_oversampler.reset();
    m_inputHistory = m_feedbackHistory = m_envelope = 0.0;
}

double IronheartPowerSection::processInternal(double input) noexcept
{
    const double low = (m_inputG * input + m_inputHistory) / (1.0 + m_inputG);
    m_inputHistory = 2.0 * low - m_inputHistory;
    const double ac = input - low;
    const double tone = bounded(m_tone.nextValue()), watts = bounded(m_watts.nextValue());
    const auto positive = m_positive.beginSample(tone, watts), negative = m_negative.beginSample(tone, watts);
    const double gain = .5 * (positive[0] + negative[0]);
    const double history = .5 * (positive[1] - negative[1]);
    const double ceiling = 1.0 - .075 * std::min(m_envelope, 1.0);

    // Zretorno = 39k + (VR13 || 1/sC81), R129=4k7 em terra.
    // O carregamento da cauda da inversora e o transformador são reduzidos
    // a ganho/impedância constantes. A malha usa a saída da MESMA amostra.
    const double resistance = 220e3 * bounded(m_dynamics.nextValue());
    constexpr double total = 39e3 + 4.7e3, highBeta = 4.7e3 / total;
    const double lowBeta = 4.7e3 / (total + resistance);
    const double tau = resistance * 6.8e-9 * total / (total + resistance);
    const double g = tau > 0.0 ? 1.0 / (2.0 * m_rate * tau) : 0.0;
    const double fraction = g / (1.0 + g);
    const double beta = resistance > 0.0 ? highBeta + (lowBeta - highBeta) * fraction : highBeta;
    const double memory = resistance > 0.0 ? (lowBeta - highBeta) * m_feedbackHistory / (1.0 + g) : 0.0;
    // Ganho aberto/curva normalizados: aproximação comportamental da saída
    // 6L6, não parâmetros extraídos de uma válvula ou transformador medidos.
    constexpr double openGain = 12.0;
    double lower = -ceiling, upper = ceiling, output = 0.0;
    for (int step = 0; step < 16; ++step)
    {
        const double drive = gain * (ac - beta * output - memory) + history;
        const double t = std::tanh(openGain * drive / ceiling);
        const double residual = output - ceiling * t;
        if (std::abs(residual) < 1e-12) break;
        if (residual > 0) upper = output; else lower = output;
        const double next = output - residual / (1.0 + openGain * gain * beta * (1.0 - t * t));
        output = next >= lower && next <= upper ? next : .5 * (lower + upper);
    }
    const double source = ac - beta * output - memory;
    const double drive = .5 * (m_positive.finishSample(source) - m_negative.finishSample(-source));
    const double magnitude = std::abs(drive);
    m_envelope += (magnitude > m_envelope ? m_attack : m_release) * (magnitude - m_envelope);
    if (resistance > 0.0)
    {
        const double filtered = (g * output + m_feedbackHistory) / (1.0 + g);
        m_feedbackHistory = 2.0 * filtered - m_feedbackHistory;
    }
    else m_feedbackHistory = output;
    return output;
}

float IronheartPowerSection::processSample(float input) noexcept
{
    if (!std::isfinite(input)) input = 0.0f;
    input = std::clamp(input, -100.0f, 100.0f); // proteção numérica, fora da faixa musical
    return m_oversampler.process(input, [this](float x) { return static_cast<float>(processInternal(x)); });
}
