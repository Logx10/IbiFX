#pragma once

#include <vector>

#include "AudioModule.h"

// Compressor — reduz a diferença de volume entre tocar fraco e forte.
//
// O PROBLEMA QUE ELE RESOLVE
// Uma guitarra tocada com dinâmica varia muito de volume: um dedilhado leve
// e uma palhetada forte podem diferir em 20 dB ou mais. Isso é desejável na
// música, mas em cima de uma cadeia com distorção e delay pode soar
// inconsistente — as notas fracas somem na mistura, as fortes saturam demais.
// O compressor nivela essa diferença: reduz o volume das partes ALTAS,
// aproximando-as das partes normais, sem tocar no que já está baixo.
//
// POR QUE ESTE MÓDULO TRABALHA EM DECIBéIS, E NÃO EM AMPLITUDE LINEAR COMO
// OS OUTROS
// "Ratio 4:1" é a definição PADRÃO de compressão, e ela só faz sentido em
// dB: significa que para cada 4 dB que o sinal sobe acima do threshold, a
// saída sobe só 1 dB. Fazer essa conta em amplitude linear exigiria uma
// fórmula não-padrão que ninguém reconheceria como "ratio 4:1" — por isso,
// diferente do Limiter e do NoiseGate, aqui vale a pena converter.
//
//     dB = 20 * log10(amplitude)       amplitude = 10 ^ (dB / 20)
//
// A CONTA, PASSO A PASSO
//
//     envelope (amplitude) -> converte pra dB
//     excesso = envelopeDb - threshold          (quanto passou do limite)
//     excessoComprimido = excesso / ratio        (quanto DEVERIA ter passado)
//     reducaoDb = excesso - excessoComprimido    (o que precisa cortar)
//     ganho = 10 ^ (-reducaoDb / 20)              (de volta pra amplitude)
//
// Com ratio = 1.0, excessoComprimido == excesso, reducaoDb == 0: nenhuma
// compressão, o sinal passa intacto — é o "desligado" natural deste módulo.
// Com ratio = 20, quase todo o excesso é cortado: perto de um limitador.
//
// ENVELOPE: O MESMO DESENHO DO NoiseGate, PARA OUTRO FIM
// Comprimir amostra a amostra sem suavização reagiria a cada ciclo da onda,
// não ao VOLUME da nota — grave especialmente sofreria, porque um ciclo
// inteiro de uma nota grave dura vários milissegundos. Por isso o nível é
// medido por um filtro de um polo com ataque e release, igual ao NoiseGate,
// só que aqui os dois são parâmetros ajustáveis: ataque rápido pega
// transientes mais cedo (mais "compressão", menos "punch"); ataque lento
// deixa o ataque da nota passar sem redução, e só comprime o sustain.
//
// O QUE FICOU DE FORA DA PRIMEIRA VERSÃO, DE PROPÓSITO
// - Makeup gain: comprimir reduz o volume médio, e um compressor de verdade
//   compensa isso ganhando de volta depois. Faltando aqui porque o
//   GainProcessor já existe e resolve isso encadeado depois, sem duplicar
//   lógica — ver se compensa um parâmetro dedicado só quando o encadeamento
//   manual se mostrar um incômodo real.
// - Soft knee: a transição no threshold aqui é abrupta (hard knee). Um knee
//   suave existe em compressores profissionais para a transição não ser
//   perceptível, mas é uma curva a mais para aprender depois de a mais
//   simples estar funcionando e compreendida.
class Compressor : public AudioModule
{
public:
    Compressor();

    // Nível, em decibéis, acima do qual a compressão começa a agir.
    void setThreshold(float thresholdDb);
    float threshold() const;

    // Quantos dB de entrada viram 1 dB de saída, acima do threshold.
    // 1.0 = sem compressão. Maior = mais compressão.
    void setRatio(float newRatio);
    float ratio() const;

    // Segundos até o detector de nível reagir a um AUMENTO de volume.
    void setAttack(float seconds);
    float attack() const;

    // Segundos até o detector de nível reagir a uma QUEDA de volume.
    void setRelease(float seconds);
    float release() const;

    const char* name() const override;

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    void process(std::vector<float>& buffer) override;

private:
    double m_sampleRate = 44100.0;

    // Nível detectado do sinal, em amplitude linear — não em dB. Convertido
    // pra dB só na hora de calcular a redução, porque o filtro de um polo
    // que o atualiza precisa operar sobre a mesma grandeza que |amostra|.
    float m_envelope = 0.0f;
};
