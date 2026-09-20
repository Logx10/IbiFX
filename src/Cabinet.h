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
    void loadImpulseResponseFile(const std::string& path);

    // Quanto do sinal convolvido entra na saída.
    void setMix(float amount);
    float mix() const;

    const char* name() const override;

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    void process(std::vector<float>& buffer) override;

private:
    ConvolutionEngine m_engine;
};
