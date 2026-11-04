#include "ParallelMarker.h"

ParallelMarker::ParallelMarker(Kind kind)
    : m_kind(kind)
{
    if (m_kind != Kind::Merge)
    {
        m_parameters.emplace_back("level", "Level", 0.0f, 2.0f, 1.0f);
        m_parameters.emplace_back("invert", "Phase", 0.0f, 1.0f, 0.0f);
    }

    m_smoothedGain.snapTo(targetGain());
}

ParallelMarker::Kind ParallelMarker::kind() const
{
    return m_kind;
}

void ParallelMarker::setLevel(float level)
{
    if (m_kind != Kind::Merge)
        m_parameters[0].setValue(level);
}

float ParallelMarker::level() const
{
    return m_kind != Kind::Merge ? m_parameters[0].value() : 1.0f;
}

void ParallelMarker::setInvert(bool invert)
{
    if (m_kind != Kind::Merge)
        m_parameters[1].setValue(invert ? 1.0f : 0.0f);
}

bool ParallelMarker::invert() const
{
    return m_kind != Kind::Merge && m_parameters[1].value() >= 0.5f;
}

float ParallelMarker::targetGain() const
{
    return invert() ? -level() : level();
}

void ParallelMarker::addLaneTo(const std::vector<float>& lane, std::vector<float>& sum)
{
    m_smoothedGain.setTarget(targetGain());

    for (std::size_t i = 0; i < lane.size() && i < sum.size(); ++i)
        sum[i] += m_smoothedGain.nextValue() * lane[i];
}

const char* ParallelMarker::name() const
{
    switch (m_kind)
    {
        case Kind::Split: return "Split";
        case Kind::Branch: return "Branch";
        case Kind::Merge: return "Merge";
    }
    return "Split";
}

void ParallelMarker::prepare(double sampleRate, int /*blockSize*/)
{
    m_smoothedGain.prepare(sampleRate, kDefaultRampSeconds, targetGain());
}

void ParallelMarker::reset()
{
    m_smoothedGain.snapTo(targetGain());
}

void ParallelMarker::process(std::vector<float>& /*buffer*/)
{
}
