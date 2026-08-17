#pragma once

#include <vector>

#include "AudioModule.h"
#include "SmoothedValue.h"

// GainProcessor — o processador mais simples que existe em áudio.
//
// Guarda um valor de ganho e o aplica a cada amostra do buffer. Ganho é
// volume dito de forma técnica: um multiplicador.
//
//     gain = 1.0  -> não muda nada
//     gain = 0.5  -> metade
//     gain = 2.0  -> dobro
//     gain = 0.0  -> silêncio
//
// É o bloco mais básico do DSP, e mesmo assim aparece em toda parte: dentro
// do compressor, do drive, do mixer e do controle de volume.
//
// Como função de transferência, é uma reta: f(x) = gain * x. Não impõe limite
// nenhum ao resultado — empurrar amostras para fora de [-1, +1] é permitido e
// intencional. Cortar é trabalho do Clipper.
//
// A faixa do parâmetro vai de -8 a 8. O lado negativo é deliberado: ganho
// negativo inverte a polaridade da onda, o que é recurso real e está coberto
// por teste.
//
// O ganho é suavizado: mudá-lo no meio de um bloco criaria um degrau na onda,
// ouvido como clique. Sem prepare(), a suavização fica inativa e o valor
// salta — que é o comportamento certo para processamento offline.
class GainProcessor : public AudioModule
{
public:
    GainProcessor();

    // Define o ganho de destino. A mudança é aplicada gradualmente.
    void setGain(float newGain);

    // Devolve o ganho de destino, não o valor instantâneo da rampa.
    float gain() const;

    const char* name() const override;

    // Configura a rampa de suavização para este sample rate.
    void prepare(double sampleRate, int blockSize) override;

    // Salta o ganho para o valor de destino, sem rampa.
    void reset() override;

    // Multiplica todas as amostras do buffer pelo ganho suavizado.
    void process(std::vector<float>& buffer) override;

private:
    SmoothedValue m_smoothedGain;
};
