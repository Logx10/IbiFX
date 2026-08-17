#pragma once

#include <cstddef>
#include <vector>

#include "AudioModule.h"
#include "SmoothedValue.h"

// Delay — eco com realimentação, construído sobre um buffer circular.
//
// O PRIMEIRO MÓDULO COM MEMÓRIA
// Gain, Clipper e SoftClipper decidem cada amostra sozinhos: um bloco de 512
// amostras dá o mesmo resultado que 512 blocos de 1. O Delay quebra isso.
// Para devolver o que aconteceu meio segundo atrás, ele precisa guardar meio
// segundo de áudio — e esse meio segundo atravessa a fronteira dos blocos.
//
// Daí vêm as três novidades: estado que sobrevive entre chamadas de
// process(), dependência do sample rate, e a necessidade de um reset().
//
// O BUFFER CIRCULAR
// A ideia ingênua seria guardar as últimas N amostras e empurrar todas uma
// casa a cada nova. Com meio segundo a 48 kHz isso custaria 24.000 cópias por
// amostra — mais de um bilhão de cópias por segundo para guardar 48.000
// números. Inviável.
//
// A solução é não mover dado nenhum. O array fica parado e o que anda é a
// posição de escrita, dando voltas:
//
//                     writePosition
//                           ↓
//         [  a    b    c    d    e    f    g    h  ]
//            0    1    2    3    4    5    6    7
//                 ↑
//           readPosition   (3 posições atrás)
//
// Escreve em 3, lê de 1. Na amostra seguinte escreve em 4 e lê de 2. Quando a
// escrita chega ao fim, volta para 0 e sobrescreve o valor mais antigo — que
// é exatamente o que se quer, porque ele já passou.
//
// Custo por amostra: uma leitura, uma escrita e um incremento. Constante,
// independente do tamanho do delay.
//
// O QUE VAI GRAVADO NO BUFFER
// Não é a entrada pura, e sim a entrada somada ao eco atenuado:
//
//     circular[write] = entrada + atrasado * feedback
//
// É essa soma que faz o eco voltar de novo daqui a delaySamples, e de novo,
// cada vez menor. Sem ela haveria uma repetição só.
//
// POR QUE O FEEDBACK PARA EM 0.95
// Cada volta multiplica o eco por feedback, formando uma progressão
// geométrica. Com 0.5 os ecos decaem (0.5, 0.25, 0.125...) e somem. Com 1.0
// cada eco volta com a mesma intensidade e nunca some. Acima de 1.0 cada
// volta é mais alta que a anterior e o sinal cresce até estourar. A faixa do
// parâmetro impede isso na origem, como o mínimo 0.0 impediu o teto negativo
// do Clipper.
//
// POR QUE O TEMPO É EM SEGUNDOS
// Segundos independem do sample rate. Um preset salvo a 44,1 kHz precisa soar
// igual a 48 kHz — guardar amostras faria o delay mudar de duração ao trocar
// de placa de áudio. A conversão para amostras acontece no process(), com o
// sample rate recebido no prepare().
//
// SECO E MOLHADO
// O sinal original chama-se dry, o processado chama-se wet. O parâmetro mix
// interpola entre os dois: 0.0 devolve só o seco (delay inaudível), 1.0 só o
// eco, 0.5 metade de cada. Como os pesos somam 1, o volume total se mantém
// aproximadamente estável ao girar o controle.
//
// SEM PREPARE, SEM EFEITO
// Chamado antes do prepare(), o process() devolve o buffer intacto em vez de
// falhar. Ele roda na thread de áudio, onde lançar exceção não é opção, e um
// eco mudo é preferível a um travamento. Há teste cobrindo esse caso.
//
// O QUE É SUAVIZADO, E O QUE NÃO É
// feedback e mix são suavizados: saltar com eles cria degrau na onda, ouvido
// como clique.
//
// O time NÃO é. Suavizá-lo significaria mover a posição de leitura
// gradualmente, o que muda a taxa com que as amostras antigas são lidas — e
// ler mais rápido ou mais devagar é, literalmente, alterar a altura do som.
// O efeito é real e desejado em delays analógicos, onde girar o knob produz
// aquele deslize de afinação característico; mas fazê-lo direito exige
// interpolação entre amostras vizinhas, porque a posição de leitura deixa de
// cair em índices inteiros. É assunto próprio, não um detalhe deste módulo.
// Por ora o time salta, o que produz um pequeno estalo ao ser mudado durante
// o som — comportamento honesto de delay digital simples.
class Delay : public AudioModule
{
public:
    Delay();

    const char* name() const override;

    // Aloca o buffer circular para o maior atraso possível neste sample rate.
    void prepare(double sampleRate, int blockSize) override;

    // Zera o conteúdo do buffer e a posição de escrita, silenciando o eco.
    void reset() override;

    // Aplica o eco a todas as amostras do buffer.
    void process(std::vector<float>& buffer) override;

    // Atraso em segundos.
    void setTime(float seconds);
    float time() const;

    // Quanto do eco realimenta a entrada, de 0.0 a 0.95.
    void setFeedback(float amount);
    float feedback() const;

    // Proporção entre seco e molhado, de 0.0 a 1.0.
    void setMix(float amount);
    float mix() const;

    // Quantas amostras o buffer circular comporta. Zero antes do prepare().
    std::size_t bufferSize() const;

private:
    std::vector<float> m_circular;
    std::size_t m_writePosition = 0;
    double m_sampleRate = 0.0;

    SmoothedValue m_smoothedFeedback;
    SmoothedValue m_smoothedMix;
};
