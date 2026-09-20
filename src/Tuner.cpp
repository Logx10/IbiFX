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

// Abaixo disso, a janela é silêncio/ruído de fundo — nem vale rodar a
// autocorrelação.
constexpr float kMinEnergy = 0.0005f;

// Correlação normalizada mínima pra aceitar uma leitura como confiável.
// Um tom limpo e sustentado costuma passar de 0.6-0.9; ruído fica bem
// abaixo disso.
constexpr float kMinConfidence = 0.35f;

constexpr float kA4Frequency = 440.0f;
constexpr int kA4MidiNumber = 69;

const char* const kNoteNames[12] =
    {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
}

Tuner::Tuner()
{
    // Alocado uma vez, no construtor — não depende do sample rate (é um
    // número fixo de amostras), então não precisa esperar o prepare().
    m_window.assign(kWindowSize, 0.0f);
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
// Média (não soma) do produto amostra-a-amostra do sinal com uma cópia
// deslocada de si mesmo por `lag` amostras.
//
// POR QUE MÉDIA, E NÃO SOMA BRUTA
// Um lag maior sobrepõe MENOS pares de amostras (window.size() - lag). Uma
// soma bruta cresce com a quantidade de termos somados, então lags curtos
// ganham uma vantagem artificial só por somarem mais parcelas — mesmo que
// cada parcela, em média, esteja menos correlacionada. Isso favorece
// exatamente o lag errado: sem dividir pela quantidade de termos, a busca
// abaixo encontrava sempre o menor lag da faixa (a frequência mais aguda
// permitida), não o período de verdade do sinal. Dividir pela quantidade de
// termos sobrepostos remove esse viés e deixa lags diferentes comparáveis
// entre si.
float correlationAtLag(const std::vector<float>& window, int lag)
{
    const std::size_t count = window.size() - static_cast<std::size_t>(lag);

    float sum = 0.0f;

    for (std::size_t i = 0; i < count; ++i)
    {
        sum += window[i] * window[i + static_cast<std::size_t>(lag)];
    }

    return sum / static_cast<float>(count);
}
}

// Autocorrelação: compara o sinal com uma cópia deslocada de si mesmo, pra
// cada deslocamento (lag) candidato dentro da faixa de frequência de
// interesse. Um sinal periódico produz uma correlação alta no lag igual ao
// período — e TAMBÉM em qualquer múltiplo inteiro dele (2×, 3×...), porque
// duas cópias deslocadas por dois períodos inteiros ficam tão alinhadas
// quanto por um período só. Uma onda de 440 Hz "parece" tanto com ela mesma
// deslocada por 1 período quanto por 5.
//
// POR QUE PARAR NO PRIMEIRO PICO, NÃO NO MAIOR
// Se a busca pegasse o lag de MAIOR correlação em toda a faixa, um múltiplo
// do período verdadeiro poderia vencer por uma margem numérica mínima —
// aconteceu na prática: a primeira versão deste código detectava 440 Hz
// como 88 Hz (lag 5× maior) só porque a correlação nesse lag saiu
// marginalmente mais alta. É o "erro de oitava" clássico de detecção de
// altura por autocorrelação. A correção: caminhar dos lags mais curtos para
// os mais longos e parar no PRIMEIRO máximo local que passe do limiar de
// confiança — o período fundamental sempre aparece antes de qualquer
// múltiplo dele, porque é o de menor lag.
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

    // energy é a SOMA bruta (usada só no piso de silêncio acima); a busca
    // abaixo compara contra a MÉDIA, para ficar na mesma grandeza da
    // correlação, que também é uma média (ver correlationAtLag).
    const float meanEnergy = energy / static_cast<float>(m_window.size());

    const int minLag = static_cast<int>(m_sampleRate / kMaxFrequencyHz);
    const int maxLag = std::min(
        static_cast<int>(m_sampleRate / kMinFrequencyHz),
        static_cast<int>(m_window.size()) - 1);

    float bestCorrelation = 0.0f;
    int bestLag = -1;

    float previousCorrelation = correlationAtLag(m_window, minLag);
    bool wasRising = false;

    for (int lag = minLag + 1; lag <= maxLag; ++lag)
    {
        const float currentCorrelation = correlationAtLag(m_window, lag);
        const bool isRising = currentCorrelation > previousCorrelation;

        // Acabou de passar de subindo para descendo: lag-1 foi um máximo
        // local. Só aceita como candidato se for confiável o bastante pra
        // não ser apenas ruído balançando.
        if (wasRising && !isRising && previousCorrelation / meanEnergy > kMinConfidence)
        {
            bestLag = lag - 1;
            bestCorrelation = previousCorrelation;
            break;
        }

        wasRising = isRising;
        previousCorrelation = currentCorrelation;
    }

    if (bestLag <= 0)
    {
        m_frequencyHz.store(0.0f, std::memory_order_relaxed);
        m_valid.store(false, std::memory_order_relaxed);
        return;
    }

    // REFINAMENTO: INTERPOLAÇÃO PARABÓLICA
    //
    // Um lag inteiro sozinho tem resolução ruim: perto de 440 Hz, o passo
    // entre um lag e o vizinho já corresponde a dezenas de cents — grosseiro
    // demais pra um afinador (que precisa distinguir uns poucos cents).
    // A correlação em volta do pico verdadeiro tem formato aproximadamente
    // parabólico; ajustando uma parábola pelos três pontos (bestLag-1,
    // bestLag, bestLag+1) e achando o vértice dela, a estimativa do lag
    // "verdadeiro" fica entre amostras inteiras, não presa a uma delas.
    float refinedLag = static_cast<float>(bestLag);

    if (bestLag > minLag && bestLag < maxLag)
    {
        const float corrPrev = correlationAtLag(m_window, bestLag - 1);
        const float corrNext = correlationAtLag(m_window, bestLag + 1);
        const float denominator = corrPrev - 2.0f * bestCorrelation + corrNext;

        if (denominator != 0.0f)
        {
            refinedLag += 0.5f * (corrPrev - corrNext) / denominator;
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
