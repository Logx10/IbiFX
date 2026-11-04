#pragma once

#include <vector>

#include "AudioModule.h"
#include "SmoothedValue.h"

// ParallelMarker — os marcadores que abrem, dividem e fecham um trecho de
// caminhos paralelos dentro da fila do ModuleChain (vários amps, cada um
// com a sua caixa, como no AmpliTube).
//
// POR QUE MARCADORES NUMA FILA, E NÃO UM GRAFO
// O AudioGraph faz qualquer roteamento, mas o motor ao vivo, os presets, o
// MIDI e a GUI endereçam tudo por posição numa lista plana de módulos.
// Trocar isso tudo seria reescrever o projeto. Com marcadores, a lista
// continua plana — o trecho paralelo é só uma faixa dela:
//
//     stomps  Split  AmpA CabA  Branch  AmpB CabB  Branch  AmpC CabC  Merge  rack
//             |--- caminho A ---|------ caminho B ---|--- caminho C ---|
//
// e quem sabe rotear é o ModuleChain (ver process() lá): no Split ele
// guarda uma cópia do sinal; em cada Branch soma o caminho que terminou
// num acumulador e devolve a cópia para o próximo; no Merge entrega a soma.
// O próprio process() do marcador não faz nada.
//
// O MIXER DE CADA CAMINHO MORA NO MARCADOR QUE O ABRE
// Split e Branch têm level (0 a 2, o volume com que o caminho entra na
// soma) e invert (inverter a fase). Inverter importa de verdade: dois amps
// com caixas e microfones diferentes chegam com fases diferentes, e somá-
// los pode cancelar o grave — num rig real, o técnico inverte um deles.
// Merge não tem parâmetros.
//
// Desligar (bypass) o Split ou um Branch silencia aquele caminho — e o
// ModuleChain nem processa os módulos dele, o que economiza CPU.
class ParallelMarker : public AudioModule
{
public:
    enum class Kind
    {
        Split,
        Branch,
        Merge
    };

    explicit ParallelMarker(Kind kind);

    Kind kind() const;

    // Split e Branch: volume do caminho na soma, e inversão de fase.
    void setLevel(float level);
    float level() const;
    void setInvert(bool invert);
    bool invert() const;

    // Soma lane, com o volume (suavizado) e a fase deste caminho, em sum.
    // Chamada pelo ModuleChain quando o caminho termina. Não aloca.
    void addLaneTo(const std::vector<float>& lane, std::vector<float>& sum);

    const char* name() const override;

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    // Não faz nada: o roteamento é do ModuleChain.
    void process(std::vector<float>& buffer) override;

private:
    float targetGain() const;

    Kind m_kind;

    // Ganho com sinal: level, negativo se invert. Suavizado para girar o
    // volume ou inverter a fase sem estalo.
    SmoothedValue m_smoothedGain;
};
