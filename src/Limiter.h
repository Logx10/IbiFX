#pragma once

#include <vector>

#include "AudioModule.h"
#include "SmoothedValue.h"

// Limiter — proteção final contra estouro, não um efeito de cor.
//
// O PROBLEMA QUE ELE RESOLVE
// O Delay soma o ataque de uma nota nova com a cauda que ainda está ecoando
// da nota anterior (ver o comentário em Delay.cpp, linha da escrita no
// buffer circular). Essa soma não tem teto embutido. Com a cadeia padrão —
// SoftClipper empurrando o sinal perto de ±1 ANTES do Delay — a segunda nota
// em diante soma um ataque quase saturado com um eco que ainda não morreu, e
// ultrapassa 1.0. Nada depois do Delay contém isso, e o estouro vira corte
// duro na hora de gravar o .wav ou tocar pela placa de som.
//
// Qualquer módulo futuro que realimente sinal (reverb, chorus) carrega o
// mesmo risco. Em vez de corrigir dentro de cada um, este módulo garante o
// teto no fim da cadeia, uma vez só — é o que todo equipamento de áudio de
// verdade tem na saída.
//
// COMO RESOLVE
//
//     |amostra| <= threshold:  sai igual, sem nenhuma alteração
//     |amostra| >  threshold:  sai comprimida por tanh, sem nunca passar de 1.0
//
// Só o EXCESSO acima do threshold entra na tanh — não a amostra inteira. É a
// diferença para o SoftClipper, que colore o sinal inteiro o tempo todo
// (ele é um distorcedor). Aqui, sinal dentro da faixa segura passa intocado;
// só o pico que ameaçava estourar é amaciado.
//
// A curva tanh garante o teto por definição matemática, não por um `if` que
// corta reto: tanh nunca ultrapassa 1, então threshold + (1-threshold)*tanh(x)
// nunca ultrapassa threshold + (1-threshold) = 1. E como tanh(0) = 0 e a
// derivada de tanh em 0 é 1, a transição no threshold é suave — sem o clique
// que um corte reto (Clipper) produziria ali.
//
// ONDE ELE VIVE NA CADEIA
// Sempre por último. Limitar antes de outro módulo só protegeria até aquele
// ponto — o que vem depois poderia estourar de novo.
class Limiter : public AudioModule
{
public:
    Limiter();

    // Nível a partir do qual a compressão começa a agir. Abaixo disso, o
    // sinal não é tocado.
    void setThreshold(float newThreshold);
    float threshold() const;

    const char* name() const override;

    // Configura a rampa de suavização para este sample rate.
    void prepare(double sampleRate, int blockSize) override;

    // Salta o threshold para o valor de destino, sem rampa.
    void reset() override;

    void process(std::vector<float>& buffer) override;

private:
    SmoothedValue m_smoothedThreshold;
};
