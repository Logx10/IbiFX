#include "Parameter.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

Parameter::Parameter(std::string id,
                     std::string label,
                     float minValue,
                     float maxValue,
                     float defaultValue)
    : m_id(std::move(id))
    , m_label(std::move(label))
    , m_minValue(minValue)
    , m_maxValue(maxValue)
    , m_defaultValue(std::clamp(defaultValue, minValue, maxValue))
    , m_value(m_defaultValue)
{
    // Faixa invertida é erro de programação, não entrada de usuário: nenhum
    // valor seria válido e todo clamp devolveria lixo. Vale falhar alto.
    if (minValue > maxValue)
    {
        throw std::invalid_argument("Parameter '" + m_id + "' com minValue maior que maxValue");
    }

    if (m_id.empty())
    {
        throw std::invalid_argument("Parameter sem id");
    }
}

const std::string& Parameter::id() const
{
    return m_id;
}

const std::string& Parameter::label() const
{
    return m_label;
}

float Parameter::minValue() const
{
    return m_minValue;
}

float Parameter::maxValue() const
{
    return m_maxValue;
}

float Parameter::defaultValue() const
{
    return m_defaultValue;
}

float Parameter::value() const
{
    return m_value;
}

void Parameter::setValue(float newValue)
{
    m_value = std::clamp(newValue, m_minValue, m_maxValue);
}

float Parameter::normalized() const
{
    const float range = m_maxValue - m_minValue;

    // Faixa de largura zero não tem posição relativa; 0 é a resposta menos
    // surpreendente, e evita a divisão.
    if (range == 0.0f)
    {
        return 0.0f;
    }

    return (m_value - m_minValue) / range;
}

void Parameter::setNormalized(float normalizedValue)
{
    const float clamped = std::clamp(normalizedValue, 0.0f, 1.0f);

    setValue(m_minValue + clamped * (m_maxValue - m_minValue));
}

void Parameter::reset()
{
    m_value = m_defaultValue;
}
