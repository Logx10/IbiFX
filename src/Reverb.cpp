#include "Reverb.h"

#include <algorithm>
#include <cmath>

Reverb::Reverb()
{
    // 0.5 dá uma cauda perceptível sem dominar o sinal seco — um ponto de
    // partida razoável, não "a sala certa". Não existe padrão universal
    // aqui, cada sala real soa diferente.
    m_parameters.emplace_back("decay", "Decay", 0.0f, 0.98f, 0.5f);
    m_parameters.emplace_back("damping", "Damping", 0.0f, 1.0f, 0.5f);
    m_parameters.emplace_back("mix", "Mix", 0.0f, 1.0f, 0.3f);
}

void Reverb::setDecay(float amount)
{
    m_parameters[0].setValue(amount);
}

float Reverb::decay() const
{
    return m_parameters[0].value();
}

void Reverb::setDamping(float amount)
{
    m_parameters[1].setValue(amount);
}

float Reverb::damping() const
{
    return m_parameters[1].value();
}

void Reverb::setMix(float amount)
{
    m_parameters[2].setValue(amount);
}

float Reverb::mix() const
{
    return m_parameters[2].value();
}

const char* Reverb::name() const
{
    return "Reverb";
}

void Reverb::prepare(double sampleRate, int /*blockSize*/)
{
    const double rate = sampleRate > 0.0 ? sampleRate : 44100.0;

    // Os comprimentos de referência são para 44100 Hz. Rodando a 48000 Hz,
    // por exemplo, cada buffer precisa ser proporcionalmente maior, ou os
    // combs soariam mais agudos que o desenhado — o mesmo raciocínio do
    // Delay convertendo segundos em amostras a partir do sample rate real.
    const double ratio = rate / 44100.0;

    for (int i = 0; i < kCombCount; ++i)
    {
        const auto length = static_cast<std::size_t>(
            std::max(1, static_cast<int>(std::lround(kCombLengthsAt44100[i] * ratio))));

        m_combs[i].buffer.assign(length, 0.0f);
        m_combs[i].writePosition = 0;
        m_combs[i].filterStore = 0.0f;
    }

    for (int i = 0; i < kAllpassCount; ++i)
    {
        const auto length = static_cast<std::size_t>(
            std::max(1, static_cast<int>(std::lround(kAllpassLengthsAt44100[i] * ratio))));

        m_allpasses[i].buffer.assign(length, 0.0f);
        m_allpasses[i].writePosition = 0;
    }
}

void Reverb::reset()
{
    for (Comb& comb : m_combs)
    {
        std::fill(comb.buffer.begin(), comb.buffer.end(), 0.0f);
        comb.writePosition = 0;
        comb.filterStore = 0.0f;
    }

    for (Allpass& allpass : m_allpasses)
    {
        std::fill(allpass.buffer.begin(), allpass.buffer.end(), 0.0f);
        allpass.writePosition = 0;
    }
}

float Reverb::processComb(Comb& comb, float input, float feedback, float damping)
{
    if (comb.buffer.empty())
    {
        return input;
    }

    const float output = comb.buffer[comb.writePosition];

    // O passa-baixas mora no loop de feedback, não na saída: é o que volta
    // pro buffer que precisa escurecer a cada volta, não a amostra que sai
    // agora. Se filtrássemos a saída em vez disso, o timbre do primeiro eco
    // já sairia errado.
    comb.filterStore = output * (1.0f - damping) + comb.filterStore * damping;

    comb.buffer[comb.writePosition] = input + comb.filterStore * feedback;

    ++comb.writePosition;

    if (comb.writePosition >= comb.buffer.size())
    {
        comb.writePosition = 0;
    }

    return output;
}

float Reverb::processAllpass(Allpass& allpass, float input, float feedback)
{
    if (allpass.buffer.empty())
    {
        return input;
    }

    const float bufferedOutput = allpass.buffer[allpass.writePosition];
    const float output = bufferedOutput - input * feedback;

    allpass.buffer[allpass.writePosition] = input + bufferedOutput * feedback;

    ++allpass.writePosition;

    if (allpass.writePosition >= allpass.buffer.size())
    {
        allpass.writePosition = 0;
    }

    return output;
}

void Reverb::process(std::vector<float>& buffer)
{
    const float feedback = m_parameters[0].value();
    const float dampingValue = m_parameters[1].value();
    const float mixValue = m_parameters[2].value();

    for (float& sample : buffer)
    {
        const float dry = sample;

        // Os 4 combs rodam em PARALELO, todos a partir da mesma entrada seca
        // — não um alimentando o outro. É a soma deles que cria a densidade;
        // encadeados em série, cada um coloriria o que o anterior já
        // coloriu, e o resultado ficaria mais metálico, não mais denso.
        float wet = 0.0f;

        for (Comb& comb : m_combs)
        {
            wet += processComb(comb, dry, feedback, dampingValue);
        }

        // Dividido pela quantidade de combs: somar 4 sinais correlacionados
        // sem compensar quadruplicaria o volume da cauda em relação ao seco.
        wet /= static_cast<float>(kCombCount);

        // Os 2 allpass rodam em SÉRIE — aqui sim um alimenta o outro — porque
        // cada um só espalha no tempo o que já saiu do anterior, sem
        // introduzir ressonância nova.
        for (Allpass& allpass : m_allpasses)
        {
            wet = processAllpass(allpass, wet, kAllpassFeedback);
        }

        sample = dry * (1.0f - mixValue) + wet * mixValue;
    }
}
