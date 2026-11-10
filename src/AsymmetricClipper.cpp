#include "AsymmetricClipper.h"

#include <cmath>

namespace
{
constexpr float kPi = 3.14159265f;

// Corte do capacitor de saída — ver "O CAPACITOR DE SAÍDA" no .h. Bem
// abaixo da nota mais grave da guitarra: só o DC sai.
constexpr float kDcBlockerHz = 10.0f;
}

AsymmetricClipper::AsymmetricClipper()
{
    m_parameters.emplace_back("drive", "Drive", 0.0f, 100.0f, 1.0f);

    // 0.3 dá uma assimetria audível sem exagerar — grande o bastante para
    // os dois semiciclos soarem visivelmente diferentes, pequeno o
    // bastante para não colapsar a curva inteira pra um lado só.
    m_parameters.emplace_back("bias", "Bias", -1.0f, 1.0f, 0.3f);
}

void AsymmetricClipper::setDrive(float newDrive)
{
    m_parameters[0].setValue(newDrive);
}

float AsymmetricClipper::drive() const
{
    return m_parameters[0].value();
}

void AsymmetricClipper::setBias(float newBias)
{
    m_parameters[1].setValue(newBias);
}

float AsymmetricClipper::bias() const
{
    return m_parameters[1].value();
}

const char* AsymmetricClipper::name() const
{
    return "AsymmetricClipper";
}

void AsymmetricClipper::prepare(double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    m_smoothedDrive.prepare(m_sampleRate, kDefaultRampSeconds, m_parameters[0].value());
    m_smoothedBias.prepare(m_sampleRate, kDefaultRampSeconds, m_parameters[1].value());
}

void AsymmetricClipper::reset()
{
    m_smoothedDrive.snapTo(m_parameters[0].value());
    m_smoothedBias.snapTo(m_parameters[1].value());
    m_oversampler.reset();
    m_dcX1 = 0.0f;
    m_dcY1 = 0.0f;
}

void AsymmetricClipper::process(std::vector<float>& buffer)
{
    m_smoothedDrive.setTarget(m_parameters[0].value());
    m_smoothedBias.setTarget(m_parameters[1].value());

    // Passa-alta de um polo na forma y[n] = g*(x[n] - x[n-1]) + r*y[n-1] —
    // o capacitor de saída (ver o .h). Uma conta por bloco: só depende do
    // sample rate.
    const float dcR = std::exp(-2.0f * kPi * kDcBlockerHz / static_cast<float>(m_sampleRate));
    const float dcG = 0.5f * (1.0f + dcR);

    for (float& sample : buffer)
    {
        const float driveValue = m_smoothedDrive.nextValue();
        const float biasValue = m_smoothedBias.nextValue();

        // O termo subtraído é a saída da curva em x=0: sem ele, entrada
        // zero produziria uma saída diferente de zero sempre que bias != 0.
        const float centerOutput = std::tanh(driveValue * biasValue);

        // Sem esta divisão, a curva pode passar de ±1 — ver o comentário
        // no .h sobre por que subtrair duas tanh não preserva o teto que uma
        // tanh sozinha garante.
        const float normalization = 1.0f + std::fabs(centerOutput);

        const auto curve = [driveValue, biasValue, centerOutput, normalization](float x)
        {
            return (std::tanh(driveValue * (x + biasValue)) - centerOutput) / normalization;
        };

        sample = m_oversampling ? m_oversampler.process(sample, curve) : curve(sample);

        // O capacitor de saída remove o DC dinâmico que a assimetria gera
        // tocando — na taxa do motor, porque DC não tem o que serrilhar. O
        // clamp devolve a garantia exata de faixa que o transiente do
        // passa-alta pode furar por uma fração de milésimo.
        if (m_dcBlocker)
        {
            const float blocked = dcG * (sample - m_dcX1) + dcR * m_dcY1;
            m_dcX1 = sample;
            m_dcY1 = blocked;

            sample = blocked > 1.0f ? 1.0f : (blocked < -1.0f ? -1.0f : blocked);
        }
    }
}

void AsymmetricClipper::setOversampling(bool enabled)
{
    m_oversampling = enabled;
    m_oversampler.reset();
}

bool AsymmetricClipper::oversampling() const
{
    return m_oversampling;
}

void AsymmetricClipper::setDcBlocker(bool enabled)
{
    m_dcBlocker = enabled;
    m_dcX1 = 0.0f;
    m_dcY1 = 0.0f;
}

bool AsymmetricClipper::dcBlocker() const
{
    return m_dcBlocker;
}
