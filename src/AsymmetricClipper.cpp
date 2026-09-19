#include "AsymmetricClipper.h"

#include <cmath>

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
    m_smoothedDrive.prepare(sampleRate, kDefaultRampSeconds, m_parameters[0].value());
    m_smoothedBias.prepare(sampleRate, kDefaultRampSeconds, m_parameters[1].value());
}

void AsymmetricClipper::reset()
{
    m_smoothedDrive.snapTo(m_parameters[0].value());
    m_smoothedBias.snapTo(m_parameters[1].value());
}

void AsymmetricClipper::process(std::vector<float>& buffer)
{
    m_smoothedDrive.setTarget(m_parameters[0].value());
    m_smoothedBias.setTarget(m_parameters[1].value());

    for (float& sample : buffer)
    {
        const float driveValue = m_smoothedDrive.nextValue();
        const float biasValue = m_smoothedBias.nextValue();

        // O termo subtraído é a saída da curva em x=0: sem ele, entrada
        // zero produziria uma saída diferente de zero sempre que bias != 0.
        const float centerOutput = std::tanh(driveValue * biasValue);
        const float shifted = std::tanh(driveValue * (sample + biasValue)) - centerOutput;

        // Sem esta divisão, "shifted" pode passar de ±1 — ver o comentário
        // no .h sobre por que subtrair duas tanh não preserva o teto que uma
        // tanh sozinha garante.
        sample = shifted / (1.0f + std::fabs(centerOutput));
    }
}
