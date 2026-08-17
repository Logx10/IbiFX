#include "SoftClipper.h"

#include <cmath>

SoftClipper::SoftClipper()
{
    m_parameters.emplace_back("drive", "Drive", 0.0f, 100.0f, 1.0f);
}

void SoftClipper::setDrive(float newDrive)
{
    m_parameters[0].setValue(newDrive);
}

float SoftClipper::drive() const
{
    return m_parameters[0].value();
}

const char* SoftClipper::name() const
{
    return "SoftClipper";
}

void SoftClipper::process(std::vector<float>& buffer)
{
    const float driveValue = m_parameters[0].value();

    for (float& sample : buffer)
    {
        // O teto em ±1 não é imposto por código: é propriedade da tanh.
        sample = std::tanh(driveValue * sample);
    }
}
