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

void ModuleChain::process(std::vector<float>& buffer)
{
    for (Slot& slot : m_slots)
    {
        if (!slot.bypassed)
        {
            slot.module->process(buffer);
        }
    }
}
