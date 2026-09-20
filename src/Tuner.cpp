#include "Tuner.h"

#include <algorithm>
#include <cmath>

namespace
{
// 2048 amostras (~46 ms a 44100 Hz) — o suficiente pra ver vários períodos
// mesmo da corda mais grave. Fixo em amostras, não em tempo: o algoritmo
// olha pra formato de onda, não pro relógio, então não precisa reescalar
// pelo sample rate.
constexpr std::size_t kWindowSize = 2048;

// Faixa de frequência de guitarra/violão — ver o comentário no .h.
constexpr float kMinFrequencyHz = 70.0f;
constexpr float kMaxFrequencyHz = 1200.0f;

// Abaixo disso, a janela é silêncio/ruído de fundo — nem vale rodar o YIN.
constexpr float kMinEnergy = 0.0005f;

// Limiar do YIN: d'(lag) precisa cair abaixo disso pra virar candidato.
// 0.15 é o valor de referência do artigo original para sinal com algum
// ruído — 0.1 seria mais rígido (mais preciso em sinal limpo, mais
// propenso a não achar nada em sinal ruidoso).
constexpr float kYinThreshold = 0.15f;

constexpr float kA4Frequency = 440.0f;
constexpr int kA4MidiNumber = 69;

const char* const kNoteNames[12] =
    {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
}

Tuner::Tuner()
{
    // Alocados uma vez, no construtor — nenhum depende do sample rate (são
    // números fixos de amostras), então não precisam esperar o prepare(), e
    // analyzeWindow() nunca aloca dentro da thread de áudio.
    m_window.assign(kWindowSize, 0.0f);
    m_difference.assign(kWindowSize, 0.0f);
    m_cumulativeMeanDifference.assign(kWindowSize, 1.0f);
}

const char* Tuner::name() const
{
    return "Tuner";
}

void Tuner::prepare(double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

void Tuner::reset()
{
    std::fill(m_window.begin(), m_window.end(), 0.0f);
    m_writePosition = 0;
    m_frequencyHz.store(0.0f, std::memory_order_relaxed);
    m_valid.store(false, std::memory_order_relaxed);
}

void Tuner::process(std::vector<float>& buffer)
{
    for (float sample : buffer)
    {
        m_window[m_writePosition] = sample;
        ++m_writePosition;

        if (m_writePosition >= m_window.size())
        {
            analyzeWindow();
            m_writePosition = 0;
        }
    }

    // Passthrough: o Tuner só escuta, nunca altera o que passa por ele.
}

namespace
{
// Soma do quadrado da diferença amostra a amostra entre o sinal e uma cópia
// deslocada de si mesmo por `lag` amostras — d(lag) do YIN. Ver o
// comentário na classe (Tuner.h) sobre por que diferença, e não semelhança
// como a autocorrelação usava antes.
float differenceAtLag(const std::vector<float>& window, int lag)
{
    const std::size_t count = window.size() - static_cast<std::size_t>(lag);

    float sum = 0.0f;

    for (std::size_t i = 0; i < count; ++i)
    {
        const float diff = window[i] - window[i + static_cast<std::size_t>(lag)];
        sum += diff * diff;
    }

    return sum;
}
}

// YIN: ver o comentário completo em Tuner.h. Resumo dos passos:
//
//   1. d(lag) para cada lag: soma do quadrado da diferença amostra a
//      amostra entre o sinal e sua cópia deslocada.
//   2. d'(lag): d(lag) normalizado pela média acumulada de d(1..lag) — cai
//      perto de 0 exatamente no período verdadeiro, mesmo com harmônicos
//      fortes atrapalhando.
//   3. Primeiro lag, dentro da faixa de frequência de interesse, em que
//      d'(lag) cruza um limiar baixo — e a partir daí, desce até o mínimo
//      local (o fundo do vale), não para assim que cruza a linha.
void Tuner::analyzeWindow()
{
    float energy = 0.0f;

    for (float sample : m_window)
    {
        energy += sample * sample;
    }

    if (energy < kMinEnergy)
    {
        m_frequencyHz.store(0.0f, std::memory_order_relaxed);
        m_valid.store(false, std::memory_order_relaxed);
        return;
    }

    const int minLag = static_cast<int>(m_sampleRate / kMaxFrequencyHz);
    const int maxLag = std::min(
        static_cast<int>(m_sampleRate / kMinFrequencyHz),
        static_cast<int>(m_window.size()) - 1);

    // Passos 1 e 2. Calculados para TODO lag de 1 até maxLag, não só dentro
    // de [minLag, maxLag]: a média acumulada em qualquer lag depende de
    // todos os d(j) anteriores a ele, incluindo os de frequência mais aguda
    // que kMaxFrequencyHz.
    m_difference[0] = 0.0f;
    m_cumulativeMeanDifference[0] = 1.0f;

    float runningSum = 0.0f;

    for (int lag = 1; lag <= maxLag; ++lag)
    {
        m_difference[lag] = differenceAtLag(m_window, lag);
        runningSum += m_difference[lag];

        m_cumulativeMeanDifference[lag] =
            (runningSum > 0.0f) ? m_difference[lag] * static_cast<float>(lag) / runningSum : 1.0f;
    }

    // Passo 3.
    int candidateLag = -1;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        if (m_cumulativeMeanDifference[lag] < kYinThreshold)
        {
            while (lag + 1 <= maxLag
                && m_cumulativeMeanDifference[lag + 1] < m_cumulativeMeanDifference[lag])
            {
                ++lag;
            }

            candidateLag = lag;
            break;
        }
    }

    if (candidateLag <= 0)
    {
        m_frequencyHz.store(0.0f, std::memory_order_relaxed);
        m_valid.store(false, std::memory_order_relaxed);
        return;
    }

    // REFINAMENTO: INTERPOLAÇÃO PARABÓLICA
    //
    // Um lag inteiro sozinho tem resolução ruim: perto de 440 Hz, o passo
    // entre um lag e o vizinho já corresponde a dezenas de cents — grosseiro
    // demais pra um afinador (que precisa distinguir uns poucos cents). O
    // vale de d'(lag) em volta do mínimo verdadeiro tem formato
    // aproximadamente parabólico; ajustando uma parábola pelos três pontos
    // (candidateLag-1, candidateLag, candidateLag+1) e achando o vértice
    // dela, a estimativa do lag "verdadeiro" fica entre amostras inteiras,
    // não presa a uma delas.
    float refinedLag = static_cast<float>(candidateLag);

    if (candidateLag > 1 && candidateLag < maxLag)
    {
        const float dPrev = m_cumulativeMeanDifference[candidateLag - 1];
        const float dCurr = m_cumulativeMeanDifference[candidateLag];
        const float dNext = m_cumulativeMeanDifference[candidateLag + 1];
        const float denominator = dPrev - 2.0f * dCurr + dNext;

        if (denominator != 0.0f)
        {
            refinedLag += 0.5f * (dPrev - dNext) / denominator;
        }
    }

    m_frequencyHz.store(static_cast<float>(m_sampleRate) / refinedLag, std::memory_order_relaxed);
    m_valid.store(true, std::memory_order_relaxed);
}

namespace
{
// Número MIDI contínuo (não arredondado) correspondente a uma frequência:
// 69 (A4/440Hz) mais quantos semitons ela está acima ou abaixo disso.
// noteName() e centsOff() partem os dois daqui — arredondar dá a nota mais
// próxima, a parte fracionária (em semitons × 100) dá os cents de desvio.
float midiNumberFromFrequency(float frequencyHz)
{
    return static_cast<float>(kA4MidiNumber) + 12.0f * std::log2(frequencyHz / kA4Frequency);
}
}

std::string Tuner::noteName() const
{
    if (!isValid())
    {
        return "";
    }

    const int roundedMidi = static_cast<int>(std::lround(midiNumberFromFrequency(frequencyHz())));
    const int noteIndex = ((roundedMidi % 12) + 12) % 12;
    const int octave = roundedMidi / 12 - 1;

    return std::string(kNoteNames[noteIndex]) + std::to_string(octave);
}

float Tuner::frequencyHz() const
{
    return m_frequencyHz.load(std::memory_order_relaxed);
}

float Tuner::centsOff() const
{
    if (!isValid())
    {
        return 0.0f;
    }

    const float midiNumber = midiNumberFromFrequency(frequencyHz());
    return 100.0f * (midiNumber - std::round(midiNumber));
}

bool Tuner::isValid() const
{
    return m_valid.load(std::memory_order_relaxed);
}
