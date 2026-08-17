#include "AudioGraph.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

AudioGraph::Node* AudioGraph::find(NodeId id)
{
    for (Node& node : m_nodes)
    {
        if (node.id == id)
        {
            return &node;
        }
    }

    return nullptr;
}

const AudioGraph::Node* AudioGraph::find(NodeId id) const
{
    for (const Node& node : m_nodes)
    {
        if (node.id == id)
        {
            return &node;
        }
    }

    return nullptr;
}

AudioGraph::NodeId AudioGraph::addNode(std::unique_ptr<AudioModule> module)
{
    if (module == nullptr)
    {
        throw std::invalid_argument("AudioGraph::addNode recebeu um modulo nulo");
    }

    Node node;
    node.id = m_nextId++;
    node.module = std::move(module);

    // Um nó recém-criado ainda não tem buffer. Ele nasce no prepare(), e até
    // lá o grafo não pode processar — que é a mesma regra do Delay.
    if (m_blockSize > 0)
    {
        node.buffer.assign(m_blockSize, 0.0f);
    }

    m_nodes.push_back(std::move(node));
    invalidateOrder();

    return m_nodes.back().id;
}

void AudioGraph::removeNode(NodeId id)
{
    const auto position = std::find_if(m_nodes.begin(), m_nodes.end(),
                                       [id](const Node& node) { return node.id == id; });

    if (position == m_nodes.end())
    {
        throw std::out_of_range("AudioGraph::removeNode com id inexistente");
    }

    m_nodes.erase(position);

    // Remover o nó sem limpar as referências a ele deixaria conexões
    // apontando para o vazio, e o process() leria de um nó que não existe.
    for (Node& node : m_nodes)
    {
        node.sources.erase(std::remove(node.sources.begin(), node.sources.end(), id),
                           node.sources.end());
    }

    invalidateOrder();
}

void AudioGraph::connect(NodeId from, NodeId to)
{
    if (find(from) == nullptr || find(to) == nullptr)
    {
        throw std::out_of_range("AudioGraph::connect com id inexistente");
    }

    if (from == to)
    {
        throw std::invalid_argument("AudioGraph::connect: um no nao pode alimentar a si mesmo");
    }

    Node* target = find(to);

    if (std::find(target->sources.begin(), target->sources.end(), from) != target->sources.end())
    {
        throw std::invalid_argument("AudioGraph::connect: a conexao ja existe");
    }

    // Checar ANTES de criar. Deixar o grafo entrar em estado inválido e
    // detectar depois faria o erro aparecer longe da causa.
    if (wouldCreateCycle(from, to))
    {
        throw std::invalid_argument("AudioGraph::connect: a conexao fecharia um ciclo");
    }

    target->sources.push_back(from);
    invalidateOrder();
}

void AudioGraph::disconnect(NodeId from, NodeId to)
{
    Node* target = find(to);

    if (target == nullptr)
    {
        throw std::out_of_range("AudioGraph::disconnect com id inexistente");
    }

    const auto position = std::find(target->sources.begin(), target->sources.end(), from);

    if (position == target->sources.end())
    {
        throw std::invalid_argument("AudioGraph::disconnect: a conexao nao existe");
    }

    target->sources.erase(position);
    invalidateOrder();
}

void AudioGraph::connectFromInput(NodeId to)
{
    Node* target = find(to);

    if (target == nullptr)
    {
        throw std::out_of_range("AudioGraph::connectFromInput com id inexistente");
    }

    target->fedByInput = true;
    invalidateOrder();
}

void AudioGraph::connectToOutput(NodeId from)
{
    Node* source = find(from);

    if (source == nullptr)
    {
        throw std::out_of_range("AudioGraph::connectToOutput com id inexistente");
    }

    source->feedsOutput = true;
}

bool AudioGraph::wouldCreateCycle(NodeId from, NodeId to) const
{
    if (from == to)
    {
        return true;
    }

    // A conexão from -> to fecha um ciclo se `from` já depende de `to`, isto
    // é, se `to` já alcança `from` seguindo as conexões existentes. Basta
    // caminhar para trás a partir de `from` e ver se chegamos em `to`.
    std::vector<NodeId> pending = {from};
    std::vector<NodeId> visited;

    while (!pending.empty())
    {
        const NodeId current = pending.back();
        pending.pop_back();

        if (current == to)
        {
            return true;
        }

        if (std::find(visited.begin(), visited.end(), current) != visited.end())
        {
            continue;
        }

        visited.push_back(current);

        const Node* node = find(current);

        if (node == nullptr)
        {
            continue;
        }

        for (NodeId source : node->sources)
        {
            pending.push_back(source);
        }
    }

    return false;
}

std::size_t AudioGraph::nodeCount() const
{
    return m_nodes.size();
}

bool AudioGraph::empty() const
{
    return m_nodes.empty();
}

bool AudioGraph::hasNode(NodeId id) const
{
    return find(id) != nullptr;
}

AudioModule& AudioGraph::moduleAt(NodeId id)
{
    Node* node = find(id);

    if (node == nullptr)
    {
        throw std::out_of_range("AudioGraph::moduleAt com id inexistente");
    }

    return *node->module;
}

const AudioModule& AudioGraph::moduleAt(NodeId id) const
{
    const Node* node = find(id);

    if (node == nullptr)
    {
        throw std::out_of_range("AudioGraph::moduleAt com id inexistente");
    }

    return *node->module;
}

void AudioGraph::invalidateOrder()
{
    m_orderValid = false;
}

void AudioGraph::rebuildOrder()
{
    // ORDENAÇÃO TOPOLÓGICA — algoritmo de Kahn.
    //
    // Para cada nó contamos de quantos outros ele depende. Os que dependem de
    // zero podem ser processados de imediato; ao processar um nó, os que
    // esperavam por ele passam a depender de um a menos, e entram na fila
    // quando chegam a zero.
    //
    // Se ao fim sobrar algum nó com dependência pendente, é porque existe um
    // ciclo. Aqui isso não deveria acontecer — connect() rejeita ciclos —,
    // mas o algoritmo naturalmente revela o caso.
    m_order.clear();
    m_order.reserve(m_nodes.size());

    std::vector<std::size_t> remaining(m_nodes.size(), 0);

    for (std::size_t i = 0; i < m_nodes.size(); ++i)
    {
        remaining[i] = m_nodes[i].sources.size();
    }

    std::vector<bool> emitted(m_nodes.size(), false);
    bool progress = true;

    while (progress)
    {
        progress = false;

        for (std::size_t i = 0; i < m_nodes.size(); ++i)
        {
            if (emitted[i] || remaining[i] != 0)
            {
                continue;
            }

            m_order.push_back(m_nodes[i].id);
            emitted[i] = true;
            progress = true;

            // Libera quem esperava por este nó.
            for (std::size_t j = 0; j < m_nodes.size(); ++j)
            {
                if (emitted[j])
                {
                    continue;
                }

                const std::size_t count = static_cast<std::size_t>(
                    std::count(m_nodes[j].sources.begin(), m_nodes[j].sources.end(), m_nodes[i].id));

                remaining[j] -= std::min(remaining[j], count);
            }
        }
    }

    m_orderValid = true;
}

std::vector<AudioGraph::NodeId> AudioGraph::processingOrder() const
{
    if (!m_orderValid)
    {
        const_cast<AudioGraph*>(this)->rebuildOrder();
    }

    return m_order;
}

void AudioGraph::prepare(double sampleRate, int blockSize)
{
    if (blockSize <= 0)
    {
        throw std::invalid_argument("AudioGraph::prepare com blockSize nao positivo");
    }

    m_blockSize = static_cast<std::size_t>(blockSize);

    m_scratch.reserve(m_blockSize);
    m_scratch.assign(m_blockSize, 0.0f);

    for (Node& node : m_nodes)
    {
        // Cada nó precisa do próprio buffer: um nó que alimenta dois caminhos
        // precisa que sua saída sobreviva intacta até o segundo consumidor.
        node.buffer.assign(m_blockSize, 0.0f);
        node.module->prepare(sampleRate, blockSize);
    }

    rebuildOrder();
}

void AudioGraph::reset()
{
    for (Node& node : m_nodes)
    {
        node.module->reset();
        std::fill(node.buffer.begin(), node.buffer.end(), 0.0f);
    }
}

void AudioGraph::processSlice(float* data, std::size_t count)
{
    if (!m_orderValid)
    {
        rebuildOrder();
    }

    for (NodeId id : m_order)
    {
        Node* node = find(id);

        if (node == nullptr)
        {
            continue;
        }

        // Monta a entrada do nó somando o que chega até ele. Somar é o que
        // "misturar sinais" significa em áudio.
        for (std::size_t i = 0; i < count; ++i)
        {
            node->buffer[i] = 0.0f;
        }

        if (node->fedByInput)
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                node->buffer[i] += data[i];
            }
        }

        for (NodeId sourceId : node->sources)
        {
            const Node* source = find(sourceId);

            if (source == nullptr)
            {
                continue;
            }

            for (std::size_t i = 0; i < count; ++i)
            {
                node->buffer[i] += source->buffer[i];
            }
        }

        // O módulo processa no lugar, sobre o próprio buffer do nó — que a
        // partir daqui passa a ser a SAÍDA dele.
        //
        // O buffer do nó tem o tamanho do bloco preparado; a fatia pode ser
        // menor no último pedaço. Redimensionar alocaria, então trabalhamos
        // numa view do tamanho certo.
        m_scratch.assign(node->buffer.begin(),
                         node->buffer.begin() + static_cast<std::ptrdiff_t>(count));

        node->module->process(m_scratch);

        for (std::size_t i = 0; i < count; ++i)
        {
            node->buffer[i] = m_scratch[i];
        }
    }

    // A saída do grafo é a soma dos nós marcados como terminais.
    for (std::size_t i = 0; i < count; ++i)
    {
        data[i] = 0.0f;
    }

    for (const Node& node : m_nodes)
    {
        if (!node.feedsOutput)
        {
            continue;
        }

        for (std::size_t i = 0; i < count; ++i)
        {
            data[i] += node.buffer[i];
        }
    }
}

void AudioGraph::process(std::vector<float>& buffer)
{
    if (m_nodes.empty() || m_blockSize == 0 || buffer.empty())
    {
        return;
    }

    // Fatia o buffer em pedaços de no máximo o bloco preparado. É mais barato
    // que realocar, e mantém a promessa de não alocar na thread de áudio.
    for (std::size_t start = 0; start < buffer.size(); start += m_blockSize)
    {
        const std::size_t count = std::min(m_blockSize, buffer.size() - start);
        processSlice(buffer.data() + start, count);
    }
}
