#include "ConvolutionEngine.h"

#include <algorithm>

namespace
{
// Ver multiply() em Fft.cpp: o operator* de std::complex é lento sem
// -ffast-math, e o laço de acumulação de Y é o mais quente do engine.
// Aqui já soma no acumulador: acc += a·b.
void multiplyAdd(Fft::Complex& accumulator, Fft::Complex a, Fft::Complex b)
{
    accumulator = {accumulator.real() + a.real() * b.real() - a.imag() * b.imag(),
                   accumulator.imag() + a.real() * b.imag() + a.imag() * b.real()};
}
}

void ConvolutionEngine::setImpulseResponse(std::vector<float> impulseResponse)
{
    // A cabeça: os primeiros kBlockSize taps (ou a IR inteira, se for menor).
    const std::size_t headSize = std::min(impulseResponse.size(), kBlockSize);
    m_head.assign(impulseResponse.begin(), impulseResponse.begin() + static_cast<std::ptrdiff_t>(headSize));

    // assign redimensiona E zera — igual ao Delay alocando seu buffer
    // circular no prepare(), a alocação acontece aqui, no domínio de
    // controle, nunca dentro de processSample().
    m_history.assign(m_head.size(), 0.0f);
    m_writePosition = 0;

    // A cauda: o resto, em partes de kBlockSize. A última é completada com
    // zeros.
    const std::size_t tailSize = impulseResponse.size() - headSize;
    const std::size_t partCount = (tailSize + kBlockSize - 1) / kBlockSize;

    m_partSpectra.clear();
    m_inputSpectra.clear();

    if (partCount > 0)
    {
        m_fft.prepare(kFftSize);
        m_scratch.assign(kFftSize, Complex(0.0f, 0.0f));
        m_accumulator.assign(kFftSize, Complex(0.0f, 0.0f));

        m_partSpectra.assign(partCount, std::vector<Complex>(kSpectrumBins));
        for (std::size_t part = 0; part < partCount; ++part)
        {
            // Os taps da parte na PRIMEIRA metade da janela de 256, zeros na
            // segunda: é o filtro do overlap-save.
            std::fill(m_scratch.begin(), m_scratch.end(), Complex(0.0f, 0.0f));
            const std::size_t first = headSize + part * kBlockSize;
            const std::size_t last = std::min(first + kBlockSize, impulseResponse.size());
            for (std::size_t i = first; i < last; ++i)
                m_scratch[i - first] = Complex(impulseResponse[i], 0.0f);

            m_fft.forward(m_scratch);
            std::copy(m_scratch.begin(), m_scratch.begin() + kSpectrumBins, m_partSpectra[part].begin());
        }

        m_inputSpectra.assign(partCount, std::vector<Complex>(kSpectrumBins, Complex(0.0f, 0.0f)));
    }

    m_newestSpectrum = 0;
    m_inputWindow.assign(kFftSize, 0.0f);
    m_tailOutput.assign(kBlockSize, 0.0f);
    m_blockPosition = 0;
}

void ConvolutionEngine::reset()
{
    std::fill(m_history.begin(), m_history.end(), 0.0f);
    m_writePosition = 0;

    for (std::vector<Complex>& spectrum : m_inputSpectra)
        std::fill(spectrum.begin(), spectrum.end(), Complex(0.0f, 0.0f));
    m_newestSpectrum = 0;

    std::fill(m_inputWindow.begin(), m_inputWindow.end(), 0.0f);
    std::fill(m_tailOutput.begin(), m_tailOutput.end(), 0.0f);
    m_blockPosition = 0;
}

float ConvolutionEngine::processSample(float input)
{
    if (m_head.empty())
    {
        return input;
    }

    // --- cabeça: a conta direta de sempre, só que sobre 128 taps ---
    m_history[m_writePosition] = input;

    // y[n] = Σ h[k] · x[n-k]. k=0 é a amostra que acabou de entrar
    // (readPosition parte de m_writePosition); k crescente caminha pro
    // passado, andando pra trás no buffer circular.
    float output = 0.0f;
    std::size_t readPosition = m_writePosition;

    for (float tap : m_head)
    {
        output += tap * m_history[readPosition];
        readPosition = (readPosition == 0) ? (m_history.size() - 1) : (readPosition - 1);
    }

    ++m_writePosition;

    if (m_writePosition >= m_history.size())
    {
        m_writePosition = 0;
    }

    if (m_partSpectra.empty())
    {
        return output;
    }

    // --- cauda: já calculada quando o bloco anterior fechou ---
    output += m_tailOutput[m_blockPosition];

    // A amostra entra na segunda metade da janela (o bloco atual).
    m_inputWindow[kBlockSize + m_blockPosition] = input;
    ++m_blockPosition;

    if (m_blockPosition == kBlockSize)
    {
        processTailBlock();
        m_blockPosition = 0;
    }

    return output;
}

void ConvolutionEngine::processTailBlock()
{
    const std::size_t partCount = m_partSpectra.size();

    // 1. X_j = FFT de [bloco anterior | bloco atual], guardado como o mais
    // novo da FDL.
    for (std::size_t i = 0; i < kFftSize; ++i)
        m_scratch[i] = Complex(m_inputWindow[i], 0.0f);
    m_fft.forward(m_scratch);

    m_newestSpectrum = (m_newestSpectrum + 1) % partCount;
    std::copy(m_scratch.begin(), m_scratch.begin() + kSpectrumBins, m_inputSpectra[m_newestSpectrum].begin());

    // O bloco atual vira o "anterior" da próxima janela.
    std::copy(m_inputWindow.begin() + kBlockSize, m_inputWindow.end(), m_inputWindow.begin());

    // 2. Y = Σ H_k · X_(j+1-k): a parte 1 com o espectro mais novo, a parte
    // 2 com o anterior, e assim por diante, andando pra trás na FDL.
    std::fill(m_accumulator.begin(), m_accumulator.end(), Complex(0.0f, 0.0f));

    std::size_t spectrum = m_newestSpectrum;
    for (std::size_t part = 0; part < partCount; ++part)
    {
        const std::vector<Complex>& h = m_partSpectra[part];
        const std::vector<Complex>& x = m_inputSpectra[spectrum];

        for (std::size_t bin = 0; bin < kSpectrumBins; ++bin)
            multiplyAdd(m_accumulator[bin], h[bin], x[bin]);

        spectrum = (spectrum == 0) ? (partCount - 1) : (spectrum - 1);
    }

    // Só a metade de baixo foi calculada (ver kSpectrumBins no header): a
    // de cima é o espelho conjugado dela, porque o sinal é real.
    for (std::size_t bin = kSpectrumBins; bin < kFftSize; ++bin)
        m_accumulator[bin] = std::conj(m_accumulator[kFftSize - bin]);

    // 3. FFT inversa; as últimas kBlockSize amostras são as que prestam.
    m_fft.inverse(m_accumulator);
    for (std::size_t i = 0; i < kBlockSize; ++i)
        m_tailOutput[i] = m_accumulator[kBlockSize + i].real();
}
