#pragma once

#include <array>
#include <cstddef>
#include <span>

// Apresentação do amp: somente IDs do catálogo, sem alterar DSP/presets.
// Clean compartilha o EQ Rhythm; o Tone global pertence ao master.
namespace ironheart_panel
{
enum class Channel { Clean, Rhythm, Lead };
struct Control
{
    const char* id;
    const char* label;
    const char* help;
    std::size_t moduleOffset = 0;
};

constexpr Channel channel(float lead, float clean) noexcept
{
    return lead >= .5f ? Channel::Lead : clean >= .5f ? Channel::Clean : Channel::Rhythm;
}

struct Selection { float lead, clean; };
constexpr Selection select(Channel selected, float previousClean) noexcept
{
    return {selected == Channel::Lead ? 1.0f : 0.0f,
            selected == Channel::Lead ? previousClean : selected == Channel::Clean ? 1.0f : 0.0f};
}

inline constexpr std::array<Control, 4> cleanControls{{
    {"cleanVolume", "Volume", "Volume do Clean; aumentar tambem pode levar o pre ao breakup."},
    {"rhythmBass", "Graves", "Graves do canal. Clean compartilha este EQ com Rhythm."},
    {"rhythmMid", "Medios", "Medios do canal. Clean compartilha este EQ com Rhythm."},
    {"rhythmTreble", "Agudos", "Agudos do canal. Clean compartilha este EQ com Rhythm."}
}};
inline constexpr std::array<Control, 5> rhythmControls{{
    {"rhythmGain", "Ganho", "Quantidade de saturacao do pre. Comece ajustando este controle."},
    {"rhythmBass", "Graves", "Corpo e graves do canal Rhythm/Clean."},
    {"rhythmMid", "Medios", "Presenca dos medios do canal Rhythm/Clean."},
    {"rhythmTreble", "Agudos", "Agudos do canal Rhythm/Clean."},
    {"rhythmVolume", "Volume", "Nivel do canal depois do EQ. Use para equilibrar com os outros canais."}
}};
inline constexpr std::array<Control, 5> leadControls{{
    {"leadGain", "Ganho", "Quantidade de saturacao do pre Lead. Comece ajustando este controle."},
    {"leadBass", "Graves", "Corpo e graves do canal Lead."},
    {"leadMid", "Medios", "Presenca dos medios do canal Lead."},
    {"leadTreble", "Agudos", "Agudos do canal Lead."},
    {"leadVolume", "Volume", "Nivel do canal depois do EQ. Use para equilibrar com os outros canais."}
}};
inline constexpr std::array<Control, 2> masterControls{{
    {"watts", "Watts", "Saida geral da potencia; altera nivel e compressao. No minimo ainda ha som.", 1},
    {"reverb", "Reverb", "Quantidade de ambiente somada a nota seca.", 1}
}};
inline constexpr std::array<Control, 3> rhythmPulls{{
    {"rhythmDeep", "Deep", "Estende a resposta dos graves no EQ Clean/Rhythm."},
    {"rhythmMidShift", "Mid Shift", "Desloca a regiao dos medios para baixo no EQ Clean/Rhythm."},
    {"rhythmTrebleShift", "Treble Shift", "Amplia a faixa de agudos no EQ Clean/Rhythm."}
}};
inline constexpr std::array<Control, 3> leadPulls{{
    {"leadDeep", "Deep", "Estende a resposta dos graves no EQ Lead."},
    {"leadMidShift", "Mid Shift", "Desloca a regiao dos medios para baixo no EQ Lead."},
    {"leadTrebleShift", "Treble Shift", "Amplia a faixa de agudos no EQ Lead."}
}};
inline constexpr std::array<Control, 3> advancedKnobs{{
    {"boost", "Pre-Boost", "Empurra a entrada antes da saturacao. Atua quando Boost esta ligado."},
    {"tone", "Tone global", "Brilho geral na potencia; afeta os tres canais e complementa o EQ do canal.", 1},
    {"dynamics", "Dynamics", "Resposta dos graves da potencia; maior alivia a realimentacao nos graves.", 1}
}};

inline std::span<const Control> controls(Channel selected) noexcept
{
    if (selected == Channel::Clean) return cleanControls;
    if (selected == Channel::Lead) return leadControls;
    return rhythmControls;
}
inline std::span<const Control> pulls(Channel selected) noexcept
{
    return selected == Channel::Lead ? std::span<const Control>(leadPulls) : std::span<const Control>(rhythmPulls);
}
}
