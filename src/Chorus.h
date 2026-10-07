#pragma once

#include <cstddef>
#include <vector>

#include "AudioModule.h"
#include "SmoothedValue.h"

// Chorus — o primeiro efeito de modulação do IbiFX: soma ao sinal uma cópia
// dele mesmo atrasada por um tempo que OSCILA.
//
// DE ONDE VEM O "CORO"
// Duas guitarras tocando a mesma parte nunca estão perfeitamente juntas:
// uma adianta e atrasa alguns milissegundos em relação à outra, e a
// afinação das duas difere um pouco. O pedal imita isso com uma só: uma
// cópia atrasada ~5 a 12 ms (curto demais para ouvir como eco) cujo atraso
// sobe e desce devagar. Atraso crescendo = a cópia "anda mais devagar" =
// soa um pouco mais grave; atraso diminuindo = um pouco mais aguda. Somada
// à original, essa cópia levemente desafinada que vai e volta é a ondulação
// característica do chorus.
//
// O QUE O OSCILADOR (LFO) CONTROLA
// Um seno lento (rate, de 0,1 a 5 Hz) decide o atraso a cada amostra:
//
//     atraso = kBaseDelay + depth × kMaxSweep × (0,5 + 0,5 × sen(2π·fase))
//
// depth 0 vira um atraso fixo de kBaseDelay — sem modulação, só um leve
// "dobrado" no som. depth 1 varre o atraso inteiro, de 5 a 12 ms.
//
// ATRASO FRACIONÁRIO: INTERPOLAÇÃO DE HERMITE
// O atraso do chorus quase nunca cai numa amostra inteira — ele desliza
// continuamente. Ler só amostras inteiras faria o atraso andar em degraus,
// e cada degrau soaria como um estalo. Interpolar entre as vizinhas resolve;
// a interpolação linear (média ponderada de duas) seria a mais simples, mas
// abafa os agudos de um jeito que varia junto com a fração — o próprio
// chorus ficaria "piscando" de brilho. Hermite usa quatro vizinhas e
// preserva bem mais do agudo, a um custo pequeno.
//
// MONO, COMO TODO O RESTO
// Um chorus de pedal estéreo manda a cópia para um lado e a original para
// o outro; aqui a cadeia é mono (ver LiveEngine.h), então as duas são
// somadas no mesmo canal — que é exatamente o que um chorus mono de pedal
// clássico faz.
//
// SEM REALIMENTAÇÃO
// A cópia atrasada não volta para o buffer (isso seria um flanger). Com
// mix até 1 e entrada em [-1, +1], a saída fica perto de [-1, +1] — só a
// interpolação de Hermite pode passar um pouco disso numa transição
// brusca, e o Limiter no fim da cadeia existe para isso.
class Chorus : public AudioModule
{
public:
    Chorus();

    const char* name() const override;

    // Aloca o buffer para o maior atraso possível. Domínio de controle.
    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    void process(std::vector<float>& buffer) override;

    // Velocidade da ondulação, em Hz.
    void setRate(float hertz);
    float rate() const;

    // Quanto do atraso o LFO varre, de 0 (fixo) a 1 (de 5 a 12 ms).
    void setDepth(float amount);
    float depth() const;

    // 0 = só o sinal original, 1 = só a cópia modulada. 0,5 é o chorus
    // clássico: as duas no mesmo volume.
    void setMix(float amount);
    float mix() const;

    std::size_t bufferSize() const;

private:
    double m_sampleRate = 44100.0;

    std::vector<float> m_circular;
    std::size_t m_writePosition = 0;

    // Fase do LFO, em ciclos (0 a 1). double: somar um incremento minúsculo
    // em float, milhões de vezes, acumularia erro e o rate derivaria.
    double m_phase = 0.0;

    SmoothedValue m_smoothedDepth;
    SmoothedValue m_smoothedMix;
};
