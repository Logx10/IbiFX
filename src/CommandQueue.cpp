#include "CommandQueue.h"

#include <stdexcept>

CommandQueue::CommandQueue(std::size_t capacity)
{
    if (capacity == 0)
    {
        throw std::invalid_argument("CommandQueue com capacidade zero");
    }

    // Uma posição a mais que a capacidade útil: é ela que permite distinguir
    // fila cheia de fila vazia sem um contador extra, que precisaria ser
    // atualizado pelas duas threads.
    m_slots.resize(capacity + 1);
}

bool CommandQueue::push(const Command& command)
{
    const std::size_t write = m_writeIndex.load(std::memory_order_relaxed);
    const std::size_t next = (write + 1) % m_slots.size();

    // Se a próxima posição de escrita alcançasse a de leitura, escrever
    // sobrescreveria um comando ainda não consumido.
    if (next == m_readIndex.load(std::memory_order_acquire))
    {
        return false;
    }

    m_slots[write] = command;

    // release: garante que a gravação acima esteja visível ANTES de o
    // consumidor enxergar o novo índice. Invertida a ordem, ele poderia ler
    // uma posição ainda não preenchida.
    m_writeIndex.store(next, std::memory_order_release);

    return true;
}

bool CommandQueue::pop(Command& outCommand)
{
    const std::size_t read = m_readIndex.load(std::memory_order_relaxed);

    // acquire: emparelha com o release do push. Ao ver o índice novo, temos
    // a garantia de que o comando já está inteiro na memória.
    if (read == m_writeIndex.load(std::memory_order_acquire))
    {
        return false;
    }

    outCommand = m_slots[read];

    m_readIndex.store((read + 1) % m_slots.size(), std::memory_order_release);

    return true;
}

bool CommandQueue::empty() const
{
    return m_readIndex.load(std::memory_order_acquire)
        == m_writeIndex.load(std::memory_order_acquire);
}

std::size_t CommandQueue::capacity() const
{
    return m_slots.size() - 1;
}

std::size_t CommandQueue::size() const
{
    const std::size_t write = m_writeIndex.load(std::memory_order_acquire);
    const std::size_t read = m_readIndex.load(std::memory_order_acquire);

    if (write >= read)
    {
        return write - read;
    }

    return m_slots.size() - read + write;
}
