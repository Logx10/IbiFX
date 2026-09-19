#pragma once

#include <vector>

#include "AudioModule.h"
#include "SmoothedValue.h"

// AsymmetricClipper — a mesma curva do SoftClipper, deslocada do centro.
//
// POR QUE O SoftClipper É SIMÉTRICO, E O QUE ISSO SIGNIFICA NO SOM
// tanh(drive * x) trata picos positivos e negativos exatamente igual:
// f(-x) == -f(x), uma função ímpar. Matematicamente elegante, mas uma
// válvula real não é simétrica assim. Um estágio single-ended (uma válvula
// só, sem par push-pull) opera num PONTO DE POLARIZAÇÃO (bias) fora do
// centro da sua curva característica — e por isso o semiciclo positivo do
// sinal satura de um jeito, o negativo de outro.
//
// Simetria perfeita produz só harmônicos ÍMPARES (3º, 5º, 7º...) — o som
// "duro" de fuzz e de válvulas push-pull. Assimetria acrescenta harmônicos
// PARES (2º, 4º...) — o som descrito como "quente" ou "doce", associado a
// amplificadores single-ended (classe A) e a certos pedais de overdrive.
//
// A CONTA: DESLOCAR A ENTRADA, TIRAR O QUE ISSO ACRESCENTOU, RENORMALIZAR
//
//     B = tanh(drive * bias)
//     f(x) = [tanh(drive * (x + bias)) - B] / (1 + |B|)
//
// bias desloca o ponto onde a curva em S está centrada — é a "polarização".
// B é uma CONSTANTE (não depende de x): sem subtraí-la, x = 0 produziria uma
// saída diferente de zero, um offset DC somado ao sinal — inaudível sozinho,
// mas que come headroom e faz o alto-falante trabalhar deslocado do repouso
// (o mesmo problema que o teste "silêncio continua silêncio" do SoftClipper
// existe pra pegar). Subtraindo B, f(0) = B - B = 0 sempre, não importa o
// bias.
//
// A DIVISÃO POR (1 + |B|) NÃO É OPCIONAL
// tanh(...) sozinha nunca sai de (-1, 1) — é a garantia que o SoftClipper
// usa. Mas aqui SUBTRAÍMOS duas coisas que cada uma pode chegar perto de 1,
// e a DIFERENÇA de dois valores perto de 1 (um positivo, um negativo) pode
// chegar perto de 2: essa garantia se perde exatamente quando ela mais
// importava. Um teste pegou isso — output saindo de [-1, 1] com bias e
// drive altos — antes deste comentário existir. Dividindo pelo maior desvio
// possível (1 + |B|, a distância entre as bordas -1-B e 1-B), o resultado
// volta a caber em (-1, 1) sempre, sem perder a propriedade f(0) = 0 (o
// numerador já é zero ali, e dividir zero por qualquer coisa continua zero).
//
// Com bias = 0, B = tanh(0) = 0 e o denominador vira 1: a fórmula colapsa
// exatamente na do SoftClipper, tanh(drive * x). Os dois módulos não são
// efeitos diferentes, são a mesma curva com um grau de liberdade a mais.
//
// UMA PEGADINHA DO PRÓPRIO DESENHO
// Drive mais alto nem sempre "distorce mais" nos dois semiciclos por igual.
// No lado que tem o MESMO sinal do bias, tanh(drive*(x+bias)) e B convergem
// para a MESMA assíntota conforme drive cresce — a diferença entre eles pode
// ENCOLHER, não crescer. É real, não é bug: é a física de um estágio
// polarizado ficando "faminto" (starved) de um lado. O lado OPOSTO ao bias
// continua saturando mais com mais drive, normalmente.
//
// O drive é suavizado do mesmo jeito que no SoftClipper. O bias TAMBÉM é
// suavizado: girá-lo de repente moveria o ponto de operação da curva
// instantaneamente, produzindo o mesmo tipo de degrau que motivou o
// SmoothedValue em todo parâmetro que multiplica ou desloca o sinal direto.
class AsymmetricClipper : public AudioModule
{
public:
    AsymmetricClipper();

    // Quanto o sinal é empurrado para dentro da curva. Igual ao do
    // SoftClipper: 1.0 não é neutro, a curva sempre colore.
    void setDrive(float newDrive);
    float drive() const;

    // Desloca o ponto de operação da curva. 0.0 = simétrico (igual ao
    // SoftClipper). Positivo empurra o semiciclo positivo pra dentro da
    // parte mais saturada da curva; negativo faz o oposto.
    void setBias(float newBias);
    float bias() const;

    const char* name() const override;

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    void process(std::vector<float>& buffer) override;

private:
    SmoothedValue m_smoothedDrive;
    SmoothedValue m_smoothedBias;
};
