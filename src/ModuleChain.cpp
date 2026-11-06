#include "ModuleChain.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "ParallelMarker.h"

namespace
{
// Folga mínima dos buffers dos caminhos paralelos. O dispositivo de áudio
// costuma entregar blocos do tamanho pedido no prepare(), mas alguns
// drivers mandam blocos maiores de vez em quando; reservar pelo menos isto
// evita que esse caso aloque na thread de áudio.
constexpr std::size_t kMinLaneCapacity = 8192;
}

void ModuleChain::add(std::unique_ptr<AudioModule> module)
{
    if (module == nullptr)
    {
        throw std::invalid_argument("ModuleChain::add recebeu um modulo nulo");
    }

    ParallelMarker* marker = dynamic_cast<ParallelMarker*>(module.get());
    m_slots.push_back(Slot{std::move(module), false, marker});
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
    const std::size_t capacity = std::max(kMinLaneCapacity, static_cast<std::size_t>(std::max(blockSize, 0)));
    m_laneInput.reserve(capacity);
    m_laneSum.reserve(capacity);

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

    m_openLane = nullptr;

    for (Slot& slot : m_slots)
    {
        if (slot.marker != nullptr)
        {
            switch (slot.marker->kind())
            {
                case ParallelMarker::Kind::Split:
                case ParallelMarker::Kind::Branch:
                    if (m_openLane == nullptr)
                    {
                        // Um Branch sem Split aberto não abre nada.
                        if (slot.marker->kind() == ParallelMarker::Kind::Branch)
                            break;

                        // assign dentro da capacidade reservada: copia, não
                        // aloca.
                        m_laneInput.assign(buffer.begin(), buffer.end());
                        m_laneSum.assign(buffer.size(), 0.0f);
                    }
                    else
                    {
                        // Fim do caminho anterior: soma, e o próximo
                        // começa da cópia do sinal de entrada.
                        finishLane(buffer);
                        std::copy(m_laneInput.begin(), m_laneInput.end(), buffer.begin());
                    }
                    m_openLane = slot.marker;
                    m_openLaneMuted = slot.bypassed;
                    break;

                case ParallelMarker::Kind::Merge:
                    if (m_openLane != nullptr)
                    {
                        finishLane(buffer);
                        std::copy(m_laneSum.begin(), m_laneSum.end(), buffer.begin());
                        m_openLane = nullptr;
                    }
                    break;
            }
            continue;
        }

        // Um caminho mudo nem processa: economiza a CPU de um amp desligado.
        if (!slot.bypassed && !(m_openLane != nullptr && m_openLaneMuted))
        {
            slot.module->process(buffer);
        }
    }

    // Trecho sem Merge: fecha no fim da fila.
    if (m_openLane != nullptr)
    {
        finishLane(buffer);
        std::copy(m_laneSum.begin(), m_laneSum.end(), buffer.begin());
        m_openLane = nullptr;
    }
}

void ModuleChain::finishLane(const std::vector<float>& buffer)
{
    if (!m_openLaneMuted)
        m_openLane->addLaneTo(buffer, m_laneSum);
}
