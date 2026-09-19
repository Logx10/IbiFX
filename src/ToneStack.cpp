#include "ToneStack.h"

namespace
{
// Componentes do tone stack do Fender '59 Bassman, exatamente como
// publicados em Yeh & Smith, DAFx-06 (Fig. 1). R1 é o potenciômetro de
// treble, R2 o de bass, R3 o de mid — R4 é fixo, não é um controle.
constexpr double kC1 = 0.25e-9;
constexpr double kC2 = 20e-9;
constexpr double kC3 = 20e-9;
constexpr double kR1 = 250e3;
constexpr double kR2 = 1e6;
constexpr double kR3 = 25e3;
constexpr double kR4 = 56e3;
}

ToneStack::ToneStack()
{
    m_parameters.emplace_back("bass", "Bass", 0.0f, 1.0f, 0.5f);
    m_parameters.emplace_back("mid", "Mid", 0.0f, 1.0f, 0.5f);
    m_parameters.emplace_back("treble", "Treble", 0.0f, 1.0f, 0.5f);
}

void ToneStack::setBass(float amount)
{
    m_parameters[0].setValue(amount);
}

float ToneStack::bass() const
{
    return m_parameters[0].value();
}

void ToneStack::setMid(float amount)
{
    m_parameters[1].setValue(amount);
}

float ToneStack::mid() const
{
    return m_parameters[1].value();
}

void ToneStack::setTreble(float amount)
{
    m_parameters[2].setValue(amount);
}

float ToneStack::treble() const
{
    return m_parameters[2].value();
}

const char* ToneStack::name() const
{
    return "ToneStack";
}

void ToneStack::prepare(double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    updateCoefficients();
}

void ToneStack::reset()
{
    m_x1 = m_x2 = m_x3 = 0.0;
    m_y1 = m_y2 = m_y3 = 0.0;
    updateCoefficients();
}

// Reproduz a Eqn. 1 de Yeh & Smith, DAFx-06: análise nodal simbólica do
// circuito da Fig. 1, verificada pelos autores contra simulação SPICE.
// l = bass, m = mid, t = treble — os mesmos nomes do artigo, cada um de 0 a
// 1. Depois disso, a transformada bilinear (Eqn. 2, §2.3) converte de tempo
// contínuo (b1..b3, a1..a3) para os coeficientes digitais B0..B3, A0..A3.
void ToneStack::updateCoefficients()
{
    const double l = static_cast<double>(m_parameters[0].value());
    const double m = static_cast<double>(m_parameters[1].value());
    const double t = static_cast<double>(m_parameters[2].value());

    // Produtos reaproveitados nas somas abaixo — nomeados como aparecem no
    // artigo (C1C2 = C1*C2, e assim por diante), para que cada linha de
    // b1..a3 abaixo possa ser comparada direto com a Eqn. 1 do artigo.
    const double c1c2 = kC1 * kC2;
    const double c1c3 = kC1 * kC3;
    const double c2c3 = kC2 * kC3;
    const double c1c2c3 = kC1 * kC2 * kC3;
    const double r3sq = kR3 * kR3;

    const double b1 = t * kC1 * kR1
                     + m * kC3 * kR3
                     + l * (kC1 * kR2 + kC2 * kR2)
                     + (kC1 * kR3 + kC2 * kR3);

    const double b2 = t * (c1c2 * kR1 * kR4 + c1c3 * kR1 * kR4)
                     - m * m * (c1c3 * r3sq + c2c3 * r3sq)
                     + m * (c1c3 * kR1 * kR3 + c1c3 * r3sq + c2c3 * r3sq)
                     + l * (c1c2 * kR1 * kR2 + c1c2 * kR2 * kR4 + c1c3 * kR2 * kR4)
                     + l * m * (c1c3 * kR2 * kR3 + c2c3 * kR2 * kR3)
                     + (c1c2 * kR1 * kR3 + c1c2 * kR3 * kR4 + c1c3 * kR3 * kR4);

    const double b3 = l * m * (c1c2c3 * kR1 * kR2 * kR3 + c1c2c3 * kR2 * kR3 * kR4)
                     - m * m * (c1c2c3 * kR1 * r3sq + c1c2c3 * r3sq * kR4)
                     + m * (c1c2c3 * kR1 * r3sq + c1c2c3 * r3sq * kR4)
                     + t * c1c2c3 * kR1 * kR3 * kR4
                     - t * m * c1c2c3 * kR1 * kR3 * kR4
                     + t * l * c1c2c3 * kR1 * kR2 * kR4;

    // a0 é sempre 1 — não depende de nenhum controle nem de componente.
    constexpr double a0 = 1.0;

    const double a1 = (kC1 * kR1 + kC1 * kR3 + kC2 * kR3 + kC2 * kR4 + kC3 * kR4)
                     + m * kC3 * kR3
                     + l * (kC1 * kR2 + kC2 * kR2);

    // Repare: nenhum termo de a1, a2, a3 tem "t" sozinho fora de m/l — é
    // exatamente a propriedade que o artigo aponta (treble não afeta os
    // polos do sistema, só os zeros no numerador b1..b3).
    const double a2 = m * (c1c3 * kR1 * kR3 - c2c3 * kR3 * kR4 + c1c3 * r3sq + c2c3 * r3sq)
                     + l * m * (c1c3 * kR2 * kR3 + c2c3 * kR2 * kR3)
                     - m * m * (c1c3 * r3sq + c2c3 * r3sq)
                     + l * (c1c2 * kR2 * kR4 + c1c2 * kR1 * kR2 + c1c3 * kR2 * kR4 + c2c3 * kR2 * kR4)
                     + (c1c2 * kR1 * kR4 + c1c3 * kR1 * kR4 + c1c2 * kR3 * kR4
                        + c1c2 * kR1 * kR3 + c1c3 * kR3 * kR4 + c2c3 * kR3 * kR4);

    const double a3 = l * m * (c1c2c3 * kR1 * kR2 * kR3 + c1c2c3 * kR2 * kR3 * kR4)
                     - m * m * (c1c2c3 * kR1 * r3sq + c1c2c3 * r3sq * kR4)
                     + m * (c1c2c3 * r3sq * kR4 + c1c2c3 * kR1 * r3sq - c1c2c3 * kR1 * kR3 * kR4)
                     + l * c1c2c3 * kR1 * kR2 * kR4
                     + c1c2c3 * kR1 * kR3 * kR4;

    // Transformada bilinear: s = c(1 - z⁻¹)/(1 + z⁻¹), com c = 2/T.
    const double c = 2.0 * m_sampleRate;
    const double cSquared = c * c;
    const double cCubed = cSquared * c;

    m_b0 = -b1 * c - b2 * cSquared - b3 * cCubed;
    m_b1 = -b1 * c + b2 * cSquared + 3.0 * b3 * cCubed;
    m_b2 = b1 * c + b2 * cSquared - 3.0 * b3 * cCubed;
    m_b3 = b1 * c - b2 * cSquared + b3 * cCubed;

    m_a0 = -a0 - a1 * c - a2 * cSquared - a3 * cCubed;
    m_a1 = -3.0 * a0 - a1 * c + a2 * cSquared + 3.0 * a3 * cCubed;
    m_a2 = -3.0 * a0 + a1 * c + a2 * cSquared - 3.0 * a3 * cCubed;
    m_a3 = -a0 + a1 * c - a2 * cSquared + a3 * cCubed;
}

void ToneStack::process(std::vector<float>& buffer)
{
    // Uma vez por bloco, não por amostra — ver a LIMITAÇÃO no comentário
    // do header.
    updateCoefficients();

    for (float& sample : buffer)
    {
        const double x0 = static_cast<double>(sample);

        const double y0 = (m_b0 * x0 + m_b1 * m_x1 + m_b2 * m_x2 + m_b3 * m_x3
                          - m_a1 * m_y1 - m_a2 * m_y2 - m_a3 * m_y3) / m_a0;

        m_x3 = m_x2;
        m_x2 = m_x1;
        m_x1 = x0;

        m_y3 = m_y2;
        m_y2 = m_y1;
        m_y1 = y0;

        sample = static_cast<float>(y0);
    }
}
