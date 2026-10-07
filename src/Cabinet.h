#pragma once

#include <string>
#include <vector>

#include "AudioModule.h"
#include "ConvolutionEngine.h"

// Cabinet — simula o alto-falante, o gabinete e o microfone, via convolução
// com uma impulse response gravada de verdade.
//
// POR QUE ISTO É DIFERENTE DE UM EQ
// Um EQ tenta imitar o TIMBRE de um gabinete ajustando bandas de frequência
// à mão. Uma impulse response CAPTURA o comportamento real de um gabinete
// específico, com um microfone específico, numa posição específica —
// convolver com ela reproduz matematicamente o que aquele sistema faria com
// o sinal, não uma aproximação por tentativa e erro.
//
// ONDE ISTO FICA NA CADEIA CONCEITUAL
// `Preamp -> Tone Stack -> Power Amp` (Fase 9) termina no sinal elétrico
// que sairia dos alto-falantes de um amplificador. O Cabinet é o próximo
// elo: o que acontece DEPOIS disso, entre o alto-falante de verdade, o ar
// da sala e o microfone que captou tudo — a etapa final antes do sinal
// virar uma gravação.
//
// A MATEMÁTICA VIVE EM ConvolutionEngine — ver o comentário lá, incluindo o
// custo computacional conhecido desta primeira versão (convolução direta,
// não FFT).
//
// mix PADRÃO EM 1.0, DIFERENTE DE Delay/Reverb
// Delay e Reverb normalmente se misturam com o sinal seco. Um cabinet sim
// substitui o timbre inteiro — sem ele, o sinal "cru" da distorção soa
// nasal e sem corpo, porque nunca passou pelo filtro natural de um
// alto-falante real. Por isso o padrão aqui é 100% molhado; mix existe
// para quem quiser misturar por gosto, não porque seja o uso comum.
class Cabinet : public AudioModule
{
public:
    Cabinet();

    // Carrega uma impulse response de um arquivo .wav. Lança
    // std::runtime_error se o arquivo não puder ser lido — ver
    // loadImpulseResponse() em IRLoader.h. Chame isto do domínio de
    // controle (ao montar a cadeia, ao trocar de preset), nunca durante o
    // process().
    //
    // A IR pode estar em qualquer taxa de amostragem: se o Cabinet já foi
    // preparado, ela é convertida aqui mesmo para a taxa do motor; se não,
    // a conversão acontece no prepare(). Ver prepare().
    void loadImpulseResponseFile(const std::string& path);

    // Caminho da última IR carregada, ou vazio se nenhuma foi. Existe para
    // que um preset consiga salvar de volta o arquivo em uso — é o único
    // estado do Cabinet que não é um Parameter.
    const std::string& irPath() const;

    // Quanto do sinal convolvido entra na saída.
    void setMix(float amount);
    float mix() const;

    const char* name() const override;

    // Converte a IR para sampleRate, se ela foi gravada em outra taxa — sem
    // isso, uma IR de 44100 Hz num motor a 48000 Hz soa com as ressonâncias
    // do gabinete ~9% mais agudas. Aloca: domínio de controle, como todo
    // prepare().
    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    void process(std::vector<float>& buffer) override;

private:
    // Entrega ao ConvolutionEngine a IR original convertida para
    // m_sampleRate (ou como está, se a taxa ainda não é conhecida).
    void applyImpulseResponse();

    ConvolutionEngine m_engine;
    std::string m_irPath;

    // A IR como veio do arquivo, e a taxa dela. Guardada para poder
    // reconverter se o motor for preparado de novo em outra taxa — converter
    // a partir de uma IR já convertida acumularia erro.
    std::vector<float> m_originalIr;
    double m_irSampleRate = 0.0;

    // 0 até o primeiro prepare(): taxa do motor ainda desconhecida.
    double m_sampleRate = 0.0;
};
