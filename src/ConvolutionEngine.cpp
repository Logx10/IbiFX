#include "ConvolutionEngine.h"

#include <algorithm>

void ConvolutionEngine::setImpulseResponse(std::vector<float> impulseResponse)
{
    m_impulseResponse = std::move(impulseResponse);

    // assign redimensiona E zera — igual ao Delay alocando seu buffer
    // circular no prepare(), a alocação acontece aqui, no domínio de
    // controle, nunca dentro de processSample().
    m_history.assign(m_impulseResponse.size(), 0.0f);
    m_writePosition = 0;
}

void ConvolutionEngine::reset()
{
    std::fill(m_history.begin(), m_history.end(), 0.0f);
    m_writePosition = 0;
}

float ConvolutionEngine::processSample(float input)
{
    if (m_history.empty())
    {
        return input;
    }

    m_history[m_writePosition] = input;

    // y[n] = Σ h[k] · x[n-k]. k=0 é a amostra que acabou de entrar
    // (readPosition parte de m_writePosition); k crescente caminha pro
    // passado, andando pra trás no buffer circular.
    float output = 0.0f;
    std::size_t readPosition = m_writePosition;

    for (float tap : m_impulseResponse)
    {
        output += tap * m_history[readPosition];
        readPosition = (readPosition == 0) ? (m_history.size() - 1) : (readPosition - 1);
    }

    ++m_writePosition;

    if (m_writePosition >= m_history.size())
    {
        m_writePosition = 0;
    }

    return output;
}
