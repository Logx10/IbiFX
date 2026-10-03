#include "ModuleChain.h"

#include <stdexcept>
#include <utility>

void ModuleChain::add(std::unique_ptr<AudioModule> module)
{
    if (module == nullptr)
    {
        throw std::invalid_argument("ModuleChain::add recebeu um modulo nulo");
    }

    m_slots.push_back(Slot{std::move(module), false});
}

void ModuleChain::remove(std::size_t index)
{
    if (index >= m_slots.size())
    {
        throw std::out_of_range("ModuleChain::remove com indice fora da cadeia");
    }

    m_slots.erase(m_slots.begin() + static_cast<std::ptrdiff_t>(index));
}

void ModuleChain::move(std::size_t from, std::size_t to)
{
    if (from >= m_slots.size() || to >= m_slots.size())
    {
        throw std::out_of_range("ModuleChain::move com indice fora da cadeia");
    }

    if (from == to)
    {
        return;
    }

    // Tira o slot da posição antiga e o reinsere na nova. O conteúdo é
    // movido, nunca copiado — um unique_ptr não permitiria a cópia.
    Slot moved = std::move(m_slots[from]);
    m_slots.erase(m_slots.begin() + static_cast<std::ptrdiff_t>(from));
    m_slots.insert(m_slots.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
}

void ModuleChain::clear()
{
    m_slots.clear();
}

std::size_t ModuleChain::size() const
{
    return m_slots.size();
}

bool ModuleChain::empty() const
{
    return m_slots.empty();
}

AudioModule& ModuleChain::moduleAt(std::size_t index)
{
    if (index >= m_slots.size())
    {
        throw std::out_of_range("ModuleChain::moduleAt com indice fora da cadeia");
    }

    return *m_slots[index].module;
}

const AudioModule& ModuleChain::moduleAt(std::size_t index) const
{
    if (index >= m_slots.size())
    {
        throw std::out_of_range("ModuleChain::moduleAt com indice fora da cadeia");
    }

    return *m_slots[index].module;
}

void ModuleChain::setBypassed(std::size_t index, bool bypassed)
{
    if (index >= m_slots.size())
    {
        throw std::out_of_range("ModuleChain::setBypassed com indice fora da cadeia");
    }

    m_slots[index].bypassed = bypassed;
}

bool ModuleChain::isBypassed(std::size_t index) const
{
    if (index >= m_slots.size())
    {
        throw std::out_of_range("ModuleChain::isBypassed com indice fora da cadeia");
    }

    return m_slots[index].bypassed;
}

void ModuleChain::prepare(double sampleRate, int blockSize)
{
    for (Slot& slot : m_slots)
    {
        slot.module->prepare(sampleRate, blockSize);
    }
}

void ModuleChain::reset()
{
    // Sem checar bypass: um delay desligado no meio de um preset não pode
    // guardar o eco antigo para soltá-lo quando for religado.
    for (Slot& slot : m_slots)
    {
        slot.module->reset();
    }
}

bool ModuleChain::pushCommand(const Command& command)
{
    return m_commands.push(command);
}

std::size_t ModuleChain::pendingCommandCount() const
{
    return m_commands.size();
}

bool ModuleChain::pushMidiCommand(const Command& command)
{
    return m_midiCommands.push(command);
}

std::size_t ModuleChain::pendingMidiCommandCount() const
{
    return m_midiCommands.size();
}

void ModuleChain::applyCommand(const Command& command)
{
    // Índice inválido é descartado em silêncio, e aqui isso é intencional:
    // estamos na thread de áudio, onde lançar não é opção, e o comando pode
    // ter sido criado antes de um módulo ser removido. Ignorar é a resposta
    // menos danosa.
    if (command.moduleIndex >= m_slots.size())
    {
        return;
    }

    Slot& slot = m_slots[command.moduleIndex];

    switch (command.type)
    {
        case Command::Type::SetParameter:
            if (command.parameterIndex < slot.module->parameterCount())
            {
                slot.module->parameterAt(command.parameterIndex).setValue(command.value);
            }
            break;

        case Command::Type::SetParameterNormalized:
            if (command.parameterIndex < slot.module->parameterCount())
            {
                slot.module->parameterAt(command.parameterIndex).setNormalized(command.value);
            }
            break;

        case Command::Type::SetBypass:
            slot.bypassed = command.value != 0.0f;
            break;

        case Command::Type::Reset:
            slot.module->reset();
            break;
    }
}

void ModuleChain::process(std::vector<float>& buffer)
{
    // Os comandos são aplicados ANTES de qualquer amostra ser tocada, para
    // que a estrutura não mude no meio do bloco. Retirar da fila é apenas
    // leitura de índice atômico: não bloqueia e não aloca.
    //
    // Duas filas, uma por produtor (UI e MIDI) — ver o comentário de
    // pushMidiCommand() no header para o motivo de não ser uma só.
    Command command;

    while (m_commands.pop(command))
    {
        applyCommand(command);
    }

    while (m_midiCommands.pop(command))
    {
        applyCommand(command);
    }

    for (Slot& slot : m_slots)
    {
        if (!slot.bypassed)
        {
            slot.module->process(buffer);
        }
    }
}
