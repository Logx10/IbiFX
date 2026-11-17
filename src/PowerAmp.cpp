#include "PowerAmp.h"

#include <cmath>

#include "OnePole.h"

namespace
{
// Bem mais lentos que os tempos do NoiseGate ou do Compressor — ver o
// comentário no .h sobre por que o sag reage à música, não à nota.
constexpr float kSagAttackSeconds = 0.05f;
constexpr float kSagReleaseSeconds = 0.3f;

// Presence: realce de agudos a partir de ~2,5 kHz, até 2x o agudo original
// somado de volta (~+9,5 dB lá em cima) com o knob no máximo. Ver o
// comentário no .h.
constexpr float kPresenceFrequency = 2500.0f;
constexpr float kPresenceMaxBoost = 2.0f;
constexpr float kPi = 3.14159265f;

// Quanto o teto da saída pode afundar com sag e envelope no máximo — ver
// "COMO MODELAMOS ISSO" no .h. 25% (~-2,5 dB) é um sag pesado de
// retificadora de válvula; mais que isso soa como defeito, não caráter.
constexpr float kSagDipDepth = 0.25f;
}

PowerAmp::PowerAmp()
{
    m_parameters.emplace_back("drive", "Drive", 0.1f, 5.0f, 1.0f);
    m_parameters.emplace_back("sag", "Sag", 0.0f, 1.0f, 0.3f);

    // 0 = neutro: um PowerAmp sem presence soa exatamente como antes de ela
    // existir. Só os amplis que têm o knob de verdade (os Brit) ligam.
    m_parameters.emplace_back("presence", "Presence", 0.0f, 1.0f, 0.0f);
    // Zero mantém presets anteriores idênticos. Receitas novas podem
    // representar o acoplamento final que impede DC de chegar ao falante.
    m_parameters.emplace_back("coupling", "Output Coupling", 0.0f, 40.0f, 0.0f);
}

void PowerAmp::setPresence(float amount)
{
    m_parameters[2].setValue(amount);
}

float PowerAmp::presence() const
{
    return m_parameters[2].value();
}

void PowerAmp::setDrive(float newDrive)
{
    m_parameters[0].setValue(newDrive);
}

float PowerAmp::drive() const
{
    return m_parameters[0].value();
}

void PowerAmp::setSag(float amount)
{
    m_parameters[1].setValue(amount);
}

float PowerAmp::sag() const
{
    return m_parameters[1].value();
}

const char* PowerAmp::name() const
{
    return "PowerAmp";
}

void PowerAmp::prepare(double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    m_driveRamp.prepare(m_sampleRate, kDefaultRampSeconds, drive());
    m_sagRamp.prepare(m_sampleRate, kDefaultRampSeconds, sag());
    m_presenceRamp.prepare(m_sampleRate, kDefaultRampSeconds, presence());
}

void PowerAmp::reset()
{
    m_envelope = 0.0f;
    m_presenceLow = 0.0f;
    m_oversampler.reset();
    m_driveRamp.snapTo(drive());
    m_sagRamp.snapTo(sag());
    m_presenceRamp.snapTo(presence());
    m_dcX = m_dcY = 0.0f;
}

void PowerAmp::process(std::vector<float>& buffer)
{
    m_driveRamp.setTarget(drive());
    m_sagRamp.setTarget(sag());
    m_presenceRamp.setTarget(presence());

    const float attackCoeff = onePoleCoefficient(kSagAttackSeconds, m_sampleRate);
    const float releaseCoeff = onePoleCoefficient(kSagReleaseSeconds, m_sampleRate);

    const float presenceCoeff = onePoleCoefficient(1.0f / (2.0f * kPi * kPresenceFrequency), m_sampleRate);
    const float coupling = m_parameters[3].value();
    const float dcR = std::exp(-2.0f * kPi * coupling / static_cast<float>(m_sampleRate));
    const float dcG = 0.5f * (1.0f + dcR);

    for (float& sample : buffer)
    {
        const float driveValue = m_driveRamp.nextValue();
        const float sagValue = m_sagRamp.nextValue();
        const float presenceGain = m_presenceRamp.nextValue() * kPresenceMaxBoost;
        // Presence ANTES da curva: o agudo realçado também satura mais, como
        // num ampli em que a realimentação negativa foi aliviada nos agudos.
        // agudo = sinal - grave (passa-alta de um polo pela diferença).
        m_presenceLow += presenceCoeff * (sample - m_presenceLow);
        sample += presenceGain * (sample - m_presenceLow);

        const float inputMagnitude = std::fabs(sample);
        const float coeff = inputMagnitude > m_envelope ? attackCoeff : releaseCoeff;

        m_envelope += coeff * (inputMagnitude - m_envelope);

        const float effectiveDrive = driveValue * (1.0f + sagValue * m_envelope);

        // O teto afunda com o mesmo envelope que aperta o drive — as duas
        // faces da queda de tensão (ver "COMO MODELAMOS ISSO" no .h). O
        // envelope entra limitado a 1: com entrada quente ele pode passar
        // disso, e um teto que afundasse sem limite viraria silêncio (ou
        // pior, divisão por zero).
        const float boundedEnvelope = m_envelope > 1.0f ? 1.0f : m_envelope;
        const float ceiling = 1.0f - kSagDipDepth * sagValue * boundedEnvelope;

        // O envelope do sag fica na taxa do motor (ele é lento, não gera
        // harmônico nenhum); só a curva roda sobreamostrada. A divisão por
        // teto dentro da tanh mantém o ganho de sinal pequeno — só o teto
        // desce.
        const auto curve = [effectiveDrive, ceiling](float x)
        { return ceiling * std::tanh(effectiveDrive * x / ceiling); };

        sample = m_oversampling ? m_oversampler.process(sample, curve) : curve(sample);
        if (coupling > 0.0f)
        {
            const float blocked = dcG * (sample - m_dcX) + dcR * m_dcY;
            m_dcX = sample;
            m_dcY = blocked;
            sample = blocked;
        }
    }
}

void PowerAmp::setOversampling(bool enabled)
{
    m_oversampling = enabled;
    m_oversampler.reset();
}

bool PowerAmp::oversampling() const
{
    return m_oversampling;
}
