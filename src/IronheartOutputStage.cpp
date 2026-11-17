#include "IronheartOutputStage.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
float bounded(float x) { return std::isfinite(x) ? std::clamp(x, 0.0f, 1.0f) : 0.0f; }
struct Volume { double load, transfer; };
Volume volume(float position)
{
    // VR5/VR8 1M log; cursor carrega R52/R84 680k até terra virtual.
    const double ratio = (std::pow(81.0, bounded(position)) - 1.0) / 80.0;
    const double top = 1e6 * (1.0 - ratio), bottom = 1e6 * ratio;
    const double parallel = bottom * 680e3 / (bottom + 680e3);
    const double total = top + parallel;
    return {1.0 / total, parallel / total * 330e3 / 680e3};
}
}

double IronheartOutputStage::LowPass::process(double input) noexcept
{
    const double output = (g * input + history) / (1.0 + g);
    history = 2.0 * output - history;
    return output;
}

void IronheartOutputStage::prepare(double rate, float lead, float rhythm, bool clean)
{
    if (!std::isfinite(rate) || rate <= 0.0)
        throw std::invalid_argument("IronheartOutputStage exige taxa interna positiva e finita");
    m_lead.prepare(rate, kDefaultRampSeconds, bounded(lead));
    m_rhythm.prepare(rate, kDefaultRampSeconds, bounded(rhythm));
    m_clean.prepare(rate, kDefaultRampSeconds, clean ? 1.0f : 0.0f);
    // R50/C17 e R76/C32: 330k || 22p. U4B é inversor unitário;
    // o somador U2B acrescenta R7/C12: 10k || 47p.
    m_leadFeedback.g = m_rhythmFeedback.g = 1.0 / (2.0 * rate * 330e3 * 22e-12);
    m_sumFeedback.g = 1.0 / (2.0 * rate * 10e3 * 47e-12);
    reset();
}

void IronheartOutputStage::setControls(float lead, float rhythm, bool clean) noexcept
{
    m_lead.setTarget(bounded(lead)); m_rhythm.setTarget(bounded(rhythm));
    m_clean.setTarget(clean ? 1.0f : 0.0f);
}

void IronheartOutputStage::reset() noexcept
{
    m_lead.snapTo(m_lead.target()); m_rhythm.snapTo(m_rhythm.target());
    m_clean.snapTo(m_clean.target());
    m_leadFeedback.history = m_rhythmFeedback.history = m_sumFeedback.history = 0.0;
}

IronheartOutputStage::Loads IronheartOutputStage::beginSample() noexcept
{
    const auto lead = volume(m_lead.nextValue()), rhythm = volume(m_rhythm.nextValue());
    const double clean = bounded(m_clean.nextValue());
    m_leadTransfer = lead.transfer;
    // Em Clean, Q3 abre o cursor de VR8 e Q1 fecha o ramo R5+R22.
    // Em Rhythm, Q5 aterra o meio desse ramo: R5 ainda carrega o EQ.
    m_rhythmTransfer = clean * (330e3 / 660e3) + (1.0 - clean) * rhythm.transfer;
    return {lead.load, clean * (1.0 / 1e6 + 1.0 / 660e3)
                      + (1.0 - clean) * (rhythm.load + 1.0 / 330e3)};
}

double IronheartOutputStage::finishSample(double lead, double rhythm, double leadMix) noexcept
{
    // Duas inversões em cada caminho até o somador: polaridade positiva.
    // Saturação dos buffers ±15V aproximada por excursão útil de ±13V;
    // slew rate, recuperação de overload e resistência dos JFETs não modelados.
    const double a = std::clamp(m_leadFeedback.process(lead * m_leadTransfer), -13.0, 13.0);
    const double b = std::clamp(m_rhythmFeedback.process(rhythm * m_rhythmTransfer), -13.0, 13.0);
    return std::clamp(m_sumFeedback.process(leadMix * a + (1.0 - leadMix) * b), -13.0, 13.0);
}
