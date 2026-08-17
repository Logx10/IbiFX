#pragma once

#include <vector>

#include "AudioModule.h"

// HighPassFilter — deixa passar o agudo e corta o grave.
//
// POR QUE ELE MUDA TUDO NUMA DISTORÇÃO
// Saturação é uma operação não-linear, e não-linearidade MISTURA as
// frequências que entram: duas notas tocadas juntas geram soma e diferença
// entre elas, além dos harmônicos de cada uma. Chama-se intermodulação.
//
// Grave forte entrando na distorção é o pior caso disso. Ele tem muita
// energia e ocupa a curva inteira do saturador, então empasta tudo que vem
// junto — o resultado é aquele som "borrado", sem definição de ataque.
//
// Cortar o grave ANTES da distorção resolve. É por isso que todo amplificador
// de guitarra tem um filtro assim entre a entrada e o preamp, e é a diferença
// entre um crunch apertado e uma pasta.
//
// Note a ordem: DEPOIS da distorção o filtro não conserta nada, porque a
// mistura já aconteceu. Filtrar é prevenção, não remédio.
//
// O FILTRO DE UM POLO
// O mais simples que existe, e o primeiro com MEMÓRIA de verdade além do
// Delay. A cada amostra:
//
//     saida = a * (saida_anterior + entrada - entrada_anterior)
//
// A ideia por trás: `entrada - entrada_anterior` é a VARIAÇÃO do sinal entre
// duas amostras. Sinal grave varia devagar, então essa diferença é pequena;
// sinal agudo varia rápido, e a diferença é grande. O filtro literalmente
// mede o quanto o sinal mudou, e é por isso que agudo passa e grave não.
//
// O coeficiente `a` vem da frequência de corte e do sample rate, e é por isso
// que este módulo precisa do prepare().
//
// A inclinação é de 6 dB por oitava — suave. Não é uma parede: uma oitava
// abaixo do corte o sinal cai à metade, duas oitavas a um quarto. Para
// limpar o grave antes da distorção isso basta e sobra; filtros mais íngremes
// existem, custam mais e são assunto para quando houver motivo.
//
// SEM PREPARE, PASSA DIRETO
// Como o Delay, sem sample rate não há como calcular o coeficiente. O sinal
// atravessa intacto em vez de virar lixo.
class HighPassFilter : public AudioModule
{
public:
    HighPassFilter();

    // Frequência de corte em Hz. Abaixo dela o sinal começa a ser atenuado.
    void setFrequency(float hz);
    float frequency() const;

    const char* name() const override;

    // Calcula o coeficiente a partir do sample rate.
    void prepare(double sampleRate, int blockSize) override;

    // Esquece a amostra anterior, evitando um estalo ao religar.
    void reset() override;

    // Filtra todas as amostras do buffer.
    void process(std::vector<float>& buffer) override;

private:
    double m_sampleRate = 0.0;

    // O estado que atravessa os blocos: a última entrada e a última saída.
    float m_previousInput = 0.0f;
    float m_previousOutput = 0.0f;
};
