#pragma once

#include <vector>

#include "AudioModule.h"
#include "Oversampler.h"

// PowerAmp — o estágio final, que "respira" com a intensidade de quem toca.
//
// A DIFERENÇA PARA O Preamp
// O Preamp reage amostra a amostra: cada uma passa pela cascata e sai
// saturada de acordo com o valor DELA mesma, sem memória de nada antes.
// Um amplificador de válvula de verdade tem outro comportamento no estágio
// de saída: a fonte de alimentação tem capacitores que armazenam energia, e
// sob demanda sustentada de corrente — tocar palhetadas fortes e contínuas —
// a tensão disponível CAI um pouco, momento a momento. É o fenômeno chamado
// "sag" (afundamento). Com menos tensão disponível, o estágio de saída
// satura mais fácil, comprimindo o som. E como a tensão leva um tempo pra
// se recuperar depois que a demanda cai, o efeito tem memória: uma seção
// tocada forte deixa a amplitude seguinte um pouco mais comprimida, mesmo
// que ela mesma seja mais fraca. É a origem do "peso"/"borracha" que se
// descreve em amplificadores valvulados sob carga pesada — power amp
// comprime a MÚSICA, não a amostra.
//
// COMO MODELAMOS ISSO
// Um envelope de nível (mesmo tipo de filtro de um polo do NoiseGate e do
// Compressor, ver OnePole.h) acompanha o volume RECENTE do sinal — não o
// instantâneo. Esse envelope age em DUAS frentes, porque a queda de tensão
// real também age em duas:
//
// 1. Aumenta o "drive" efetivo da curva: quanto mais alto o volume
//    sustentado ultimamente, mais fácil a próxima amostra satura.
//
//        driveEfetivo = drive * (1 + sag * envelope)
//
// 2. ABAIXA O TETO da saída: menos tensão na fonte é, literalmente, menos
//    excursão disponível no estágio de saída. O teto cai até 25%
//    (kSagDipDepth no .cpp) com sag e envelope no máximo:
//
//        teto = 1 - kSagDipDepth * sag * min(envelope, 1)
//        saída = teto * tanh(driveEfetivo * entrada / teto)
//
//    A divisão por teto DENTRO da tanh mantém o ganho de sinal pequeno
//    igual a driveEfetivo (a derivada em 0 não muda) — só o teto desce.
//    É essa queda momentânea de teto, que se recupera quando a demanda
//    passa, que produz a sensação de "espremer e voltar" ("squish") de um
//    power amp valvulado de verdade — o drive sozinho comprimia, mas não
//    afundava.
//
// sag = 0 desliga o efeito inteiramente (driveEfetivo = drive, teto = 1 —
// vira um único estágio de saturação comum, sem memória nenhuma).
//
// TEMPOS BEM MAIS LENTOS QUE OS DE UM NoiseGate OU Compressor
// O envelope de sag usa ataque de 50 ms e release de 300 ms — de propósito
// muito mais lentos que os do NoiseGate (2 ms) ou do detector de nível do
// Compressor (ajustável, mas tipicamente dezenas de ms). Um gate ou um
// compressor reagem à ENVOLTÓRIA de uma nota individual; o sag reage ao
// comportamento de VÁRIOS segundos de música — mais perto da inércia
// térmica/elétrica de uma fonte de alimentação de verdade do que da
// dinâmica de uma única nota. Não são ajustáveis ainda: são constantes
// estruturais do efeito, não controles que o músico giraria.
//
// POR QUE A SAÍDA CONTINUA EM [-1, +1]
// A saída final é sempre teto * tanh(...), com |tanh| < 1 e teto entre
// 0,75 e 1 (o envelope entra na conta do teto limitado a 1, então o teto
// nunca desce além de kSagDipDepth) — o produto fica sempre dentro de
// [-teto, +teto], que está dentro de [-1, +1]. Mesma garantia matemática
// do SoftClipper e do Preamp, só que com um teto que respira.
//
// PRESENCE (parâmetro "presence", 0 a 1)
// O knob que dá a "mordida" de um Marshall. No circuito real ele não é um
// EQ comum: ele alivia a realimentação negativa do estágio de potência nos
// agudos. Com menos realimentação, os agudos ganham mais ganho DENTRO do
// power amp — saem mais altos e saturam mais. Por isso aqui o realce de
// agudos (a partir de ~2,5 kHz, até ~+9,5 dB) acontece ANTES da tanh, e
// não depois: um presence aplicado depois da saturação seria só um treble
// a mais; antes, ele muda o caráter da distorção. Em 0 (o padrão) não faz
// nada, e o PowerAmp soa como sempre soou.
class PowerAmp : public AudioModule
{
public:
    PowerAmp();

    // Saturação de base, sem nenhum sag — o que sobra com sag = 0.
    void setDrive(float newDrive);
    float drive() const;

    // Quanto o volume sustentado recente empurra o drive efetivo pra cima.
    // 0 = sem efeito de sag. 1 = o volume máximo recente pode dobrar o
    // drive efetivo.
    void setSag(float amount);
    float sag() const;

    // Realce de agudos no estágio de potência — ver o comentário da classe.
    void setPresence(float amount);
    float presence() const;

    const char* name() const override;

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    void process(std::vector<float>& buffer) override;

    // Liga/desliga o oversampling 4× (ver Oversampler.h) — ligado por
    // padrão. Desligar serve aos testes que conferem a curva amostra a
    // amostra: ligado, a saída sai filtrada e ~32 amostras atrasada.
    void setOversampling(bool enabled);
    bool oversampling() const;

private:
    double m_sampleRate = 44100.0;

    // Nível recente do sinal, em amplitude — não a amostra atual. Escrito e
    // lido só dentro do process(), caminha suavemente pelo mesmo motivo do
    // gain do NoiseGate.
    float m_envelope = 0.0f;

    // O grave do sinal (passa-baixa de um polo); sinal - isto = o agudo que
    // o presence realça.
    float m_presenceLow = 0.0f;

    Oversampler m_oversampler;
    bool m_oversampling = true;
};
