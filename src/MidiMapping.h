#pragma once

#include <cstddef>
#include <optional>
#include <unordered_map>

#include "CommandQueue.h"
#include "MidiMessage.h"

// MidiMapping — a camada "Mapping" do pipeline do AI_GUIDELINES §37:
//
//     Controller -> Mapping -> Command -> IbiFX
//
// O Controller (um MidiDevice de verdade, ou um MidiMessage escrito à mão
// em teste) entrega uma mensagem; esta classe decide o que ela SIGNIFICA
// para esta cadeia especificamente — duas pedaleiras podem mapear o mesmo
// CC 7 para coisas diferentes. O resultado é um Command, o mesmo tipo que
// a CommandQueue já usa para a UI: o MIDI não inventa um caminho novo até
// o áudio, reaproveita o que a Fase 7 já construiu.
//
// POR QUE UM CONTROL CHANGE VIRA SetParameterNormalized, NÃO SetParameter
// Um Control Change entrega 0 a 127. O próprio Parameter.h já antecipava
// isso desde a Fase 5: "MIDI CC entrega 0 a 127... a forma normalizada é a
// moeda comum entre esses mundos". Mapear um CC só precisa do id do
// parâmetro, nunca da sua faixa — é exatamente o que setNormalized() foi
// desenhado para fazer, e por isso o mapeamento nem guarda min/max.
//
// PROGRAM CHANGE FICA PARA DEPOIS
// O próprio AI_GUIDELINES (§37) lista Program Change em "suportar
// futuramente", separado da lista principal (CC é a prioridade). Trocar de
// preset é uma mudança ESTRUTURAL da cadeia (ModuleChain::clear()+add()),
// que não cabe num Command atravessando a fila de tempo real — precisaria
// do mesmo caminho de parar/trocar/religar que DesktopUI::loadPreset() usa,
// fora da thread de áudio. Fica para quando um mapeamento de Program
// Change para preset for pedido de verdade.
class MidiMapping
{
public:
    // Liga um Control Change a um parâmetro: toda mensagem com esse número
    // de CC vira um Command::SetParameterNormalized para esse módulo e
    // parâmetro. Substitui qualquer mapeamento anterior do mesmo CC.
    void mapControlChangeToParameter(int ccNumber, std::size_t moduleIndex, std::size_t parameterIndex);

    // Liga um Control Change ao bypass de um módulo. Convenção comum de
    // pedaleira MIDI: valor >= 64 liga o bypass, abaixo disso desliga —
    // meio curso de um pedal de expressão ou de um switch físico enviando
    // 0/127. Substitui qualquer mapeamento anterior do mesmo CC.
    void mapControlChangeToBypass(int ccNumber, std::size_t moduleIndex);

    // Desfaz o mapeamento desse número de CC, se houver algum.
    void unmapControlChange(int ccNumber);

    // Quantos CCs estão mapeados agora.
    std::size_t mappingCount() const;

    // Traduz uma mensagem MIDI no Command correspondente, se houver
    // mapeamento para ela. NoteOn, NoteOff e Program Change sempre
    // devolvem std::nullopt — ver o comentário da classe sobre Program
    // Change.
    std::optional<Command> translate(const MidiMessage& message) const;

private:
    enum class TargetKind
    {
        Parameter,
        Bypass
    };

    struct Target
    {
        TargetKind kind;
        std::size_t moduleIndex;
        std::size_t parameterIndex = 0;
    };

    std::unordered_map<int, Target> m_controlChangeTargets;
};
