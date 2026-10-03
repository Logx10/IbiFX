#pragma once

// MidiMessage — um evento MIDI já decodificado, independente de como ele
// chegou (porta de hardware real, arquivo, ou escrito à mão em teste).
//
// O byte bruto do protocolo (status byte, nibble de canal, running status)
// fica inteiramente dentro da camada de plataforma que algum dia lerá uma
// porta de verdade — esta struct é o que sobra depois de decodificado, e é
// só isso que o resto do projeto (MidiMapping, testes) precisa enxergar.
// Mesma divisão que AudioDevice já faz para áudio: a complicação do
// protocolo morre numa borda, o core só vê dado limpo.
struct MidiMessage
{
    enum class Type
    {
        NoteOn,
        NoteOff,
        ControlChange,
        ProgramChange
    };

    Type type = Type::ControlChange;

    // 0 a 15 — o protocolo numera canais de 1 a 16, mas aqui fica base-zero
    // como todo índice do projeto.
    int channel = 0;

    // NoteOn/NoteOff: número da nota.
    // ControlChange: número do controlador (0 a 127).
    // ProgramChange: número do programa (0 a 127).
    int data1 = 0;

    // NoteOn/NoteOff: velocity.
    // ControlChange: valor do controlador (0 a 127).
    // ProgramChange: não se aplica, fica em 0.
    int data2 = 0;
};
