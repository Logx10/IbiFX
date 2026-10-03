// Testes do MidiMapping — a camada "Mapping" do pipeline Controller ->
// Mapping -> Command -> IbiFX (AI_GUIDELINES §37).
//
// A infra de verificação vive em test_helpers.h.

#include <iostream>

#include "MidiMapping.h"
#include "test_helpers.h"

namespace
{
MidiMessage controlChange(int channel, int ccNumber, int value)
{
    MidiMessage message;
    message.type = MidiMessage::Type::ControlChange;
    message.channel = channel;
    message.data1 = ccNumber;
    message.data2 = value;
    return message;
}
}

// ---------------------------------------------------------------------

// Sem mapeamento nenhum, qualquer Control Change é ignorado.
void testUnmappedControlChangeIsIgnored()
{
    std::cout << "control change sem mapeamento e ignorado\n";

    MidiMapping mapping;
    const auto command = mapping.translate(controlChange(0, 7, 100));

    check(!command.has_value(), "nenhum comando gerado");
}

// Um CC mapeado para parâmetro vira SetParameterNormalized, com o valor
// 0..127 convertido para 0..1.
void testControlChangeMapsToParameter()
{
    std::cout << "control change mapeado vira SetParameterNormalized\n";

    MidiMapping mapping;
    mapping.mapControlChangeToParameter(7, 2, 0);

    const auto command = mapping.translate(controlChange(0, 7, 127));

    check(command.has_value(), "comando foi gerado");
    check(command->type == Command::Type::SetParameterNormalized, "tipo e SetParameterNormalized");
    check(command->moduleIndex == 2, "modulo e o mapeado");
    check(command->parameterIndex == 0, "parametro e o mapeado");
    checkClose(command->value, 1.0f, "127 vira 1.0");
}

// 0 e 64 (meio curso) convertem para as pontas e o meio da faixa
// normalizada, não só o extremo superior.
void testControlChangeValueScaling()
{
    std::cout << "escala do valor do CC\n";

    MidiMapping mapping;
    mapping.mapControlChangeToParameter(1, 0, 0);

    checkClose(mapping.translate(controlChange(0, 1, 0))->value, 0.0f, "0 vira 0.0");

    const float meio = mapping.translate(controlChange(0, 1, 64))->value;
    check(meio > 0.49f && meio < 0.51f, "64 fica perto do meio (0.5)");
}

// Um CC mapeado para bypass segue a convenção: >= 64 liga, abaixo desliga.
void testControlChangeMapsToBypass()
{
    std::cout << "control change mapeado vira SetBypass\n";

    MidiMapping mapping;
    mapping.mapControlChangeToBypass(64, 3);

    const auto ligado = mapping.translate(controlChange(0, 64, 127));
    check(ligado.has_value(), "comando gerado (valor alto)");
    check(ligado->type == Command::Type::SetBypass, "tipo e SetBypass");
    check(ligado->moduleIndex == 3, "modulo e o mapeado");
    checkClose(ligado->value, 1.0f, "valor alto liga bypass (1.0)");

    const auto desligado = mapping.translate(controlChange(0, 64, 10));
    check(desligado.has_value(), "comando gerado (valor baixo)");
    checkClose(desligado->value, 0.0f, "valor baixo desliga bypass (0.0)");

    // O batente exato: 64 liga, 63 nao.
    checkClose(mapping.translate(controlChange(0, 64, 64))->value, 1.0f, "64 exato liga");
    checkClose(mapping.translate(controlChange(0, 64, 63))->value, 0.0f, "63 nao liga");
}

// Remapear o mesmo CC substitui o mapeamento anterior, não acumula os dois.
void testRemappingReplacesTarget()
{
    std::cout << "remapear substitui o alvo anterior\n";

    MidiMapping mapping;
    mapping.mapControlChangeToParameter(10, 0, 0);
    mapping.mapControlChangeToBypass(10, 5);

    check(mapping.mappingCount() == 1, "continua 1 mapeamento, nao 2");

    const auto command = mapping.translate(controlChange(0, 10, 127));
    check(command->type == Command::Type::SetBypass, "o mapeamento novo venceu");
    check(command->moduleIndex == 5, "aponta pro modulo do mapeamento novo");
}

// unmapControlChange() desfaz o mapeamento.
void testUnmapRemovesTarget()
{
    std::cout << "desmapear um CC\n";

    MidiMapping mapping;
    mapping.mapControlChangeToParameter(20, 0, 0);
    check(mapping.mappingCount() == 1, "1 mapeamento antes de desmapear");

    mapping.unmapControlChange(20);
    check(mapping.mappingCount() == 0, "0 mapeamentos depois de desmapear");

    const auto command = mapping.translate(controlChange(0, 20, 100));
    check(!command.has_value(), "CC desmapeado nao gera mais comando");
}

// NoteOn, NoteOff e Program Change nunca geram comando — ver o comentário
// em MidiMapping.h sobre Program Change ficar para depois.
void testNonControlChangeMessagesAreIgnored()
{
    std::cout << "note on/off e program change sao ignorados\n";

    MidiMapping mapping;
    mapping.mapControlChangeToParameter(7, 0, 0);

    MidiMessage noteOn;
    noteOn.type = MidiMessage::Type::NoteOn;
    noteOn.data1 = 7;
    noteOn.data2 = 100;
    check(!mapping.translate(noteOn).has_value(), "NoteOn ignorado mesmo com numero igual ao CC mapeado");

    MidiMessage noteOff;
    noteOff.type = MidiMessage::Type::NoteOff;
    noteOff.data1 = 7;
    check(!mapping.translate(noteOff).has_value(), "NoteOff ignorado");

    MidiMessage programChange;
    programChange.type = MidiMessage::Type::ProgramChange;
    programChange.data1 = 5;
    check(!mapping.translate(programChange).has_value(), "ProgramChange ignorado (fica para depois)");
}

int main()
{
    std::cout << "\n=== testes do MidiMapping ===\n\n";

    testUnmappedControlChangeIsIgnored();
    testControlChangeMapsToParameter();
    testControlChangeValueScaling();
    testControlChangeMapsToBypass();
    testRemappingReplacesTarget();
    testUnmapRemovesTarget();
    testNonControlChangeMessagesAreIgnored();

    return reportResults();
}
