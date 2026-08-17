#include "AudioModule.h"

#include <stdexcept>

void AudioModule::prepare(double, int)
{
    // Módulo stateless não tem o que preparar. Os parâmetros ficam sem nome
    // porque não são usados aqui, e nomeá-los renderia aviso do compilador.
}

void AudioModule::reset()
{
    // Módulo stateless não acumula nada, então não há o que descartar.
}

std::size_t AudioModule::parameterCount() const
{
    return m_parameters.size();
}

Parameter& AudioModule::parameterAt(std::size_t index)
{
    if (index >= m_parameters.size())
    {
        throw std::out_of_range("AudioModule::parameterAt com indice fora da lista");
    }

    return m_parameters[index];
}

const Parameter& AudioModule::parameterAt(std::size_t index) const
{
    if (index >= m_parameters.size())
    {
        throw std::out_of_range("AudioModule::parameterAt com indice fora da lista");
    }

    return m_parameters[index];
}

Parameter* AudioModule::findParameter(const std::string& id)
{
    for (Parameter& parameter : m_parameters)
    {
        if (parameter.id() == id)
        {
            return &parameter;
        }
    }

    // Ausência não é erro: quem procura pode legitimamente não saber se o
    // módulo tem aquele parâmetro. Cabe ao chamador decidir o que fazer.
    return nullptr;
}

const Parameter* AudioModule::findParameter(const std::string& id) const
{
    for (const Parameter& parameter : m_parameters)
    {
        if (parameter.id() == id)
        {
            return &parameter;
        }
    }

    return nullptr;
}

void AudioModule::resetParameters()
{
    for (Parameter& parameter : m_parameters)
    {
        parameter.reset();
    }
}
