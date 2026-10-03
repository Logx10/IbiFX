#include "MidiMapping.h"

void MidiMapping::mapControlChangeToParameter(int ccNumber, std::size_t moduleIndex, std::size_t parameterIndex)
{
    Target target;
    target.kind = TargetKind::Parameter;
    target.moduleIndex = moduleIndex;
    target.parameterIndex = parameterIndex;

    m_controlChangeTargets[ccNumber] = target;
}

void MidiMapping::mapControlChangeToBypass(int ccNumber, std::size_t moduleIndex)
{
    Target target;
    target.kind = TargetKind::Bypass;
    target.moduleIndex = moduleIndex;

    m_controlChangeTargets[ccNumber] = target;
}

void MidiMapping::unmapControlChange(int ccNumber)
{
    m_controlChangeTargets.erase(ccNumber);
}

std::size_t MidiMapping::mappingCount() const
{
    return m_controlChangeTargets.size();
}

std::optional<Command> MidiMapping::translate(const MidiMessage& message) const
{
    if (message.type != MidiMessage::Type::ControlChange)
        return std::nullopt;

    const auto it = m_controlChangeTargets.find(message.data1);

    if (it == m_controlChangeTargets.end())
        return std::nullopt;

    const Target& target = it->second;

    Command command;
    command.moduleIndex = target.moduleIndex;

    if (target.kind == TargetKind::Parameter)
    {
        command.type = Command::Type::SetParameterNormalized;
        command.parameterIndex = target.parameterIndex;
        // CC vem em 0..127; setNormalized() espera 0..1.
        command.value = static_cast<float>(message.data2) / 127.0f;
    }
    else
    {
        command.type = Command::Type::SetBypass;
        // Meio curso pra cima liga o bypass — convenção comum de pedal de
        // expressão/switch MIDI.
        command.value = (message.data2 >= 64) ? 1.0f : 0.0f;
    }

    return command;
}
