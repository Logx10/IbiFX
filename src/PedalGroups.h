#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

// PedalGroups — os grupos A, B e C de equipamentos que ligam e desligam
// juntos com um clique (ou uma tecla), como o "solo" de uma pedaleira:
// distorção + delay entram de uma vez.
//
// POR QUE SÓ UM HEADER COM FUNÇÕES PEQUENAS
// Um grupo não processa áudio e não tem estado próprio: é só uma marca em
// cada item da cadeia ("este pedal está no grupo A") e uma regra para o
// clique. A marca mora em Preset::ChainItemState::groups; o liga/desliga
// usa o mesmo setBypassed() dos footswitches, pela fila de comandos do
// LiveEngine. Nada aqui toca a thread de áudio.
//
// A MARCA É UMA MÁSCARA DE BITS
// Um pedal pode estar em mais de um grupo (o delay no "solo" e na
// "ambiência"). Com três grupos, três bits bastam: bit 0 = A, bit 1 = B,
// bit 2 = C. groups == 0 é "em nenhum grupo".
namespace pedal_groups
{
constexpr std::size_t kCount = 3;
constexpr std::array<char, kCount> kLetters = {'A', 'B', 'C'};

constexpr unsigned bit(std::size_t group)
{
    return 1u << group;
}

// A máscara como texto, na ordem das letras: 0b101 -> "AC". Vazio se 0.
inline std::string toLetters(unsigned mask)
{
    std::string letters;
    for (std::size_t g = 0; g < kCount; ++g)
    {
        if ((mask & bit(g)) != 0)
            letters += kLetters[g];
    }
    return letters;
}

// O índice do grupo de uma letra ('A' -> 0). Devolve kCount se a letra
// não for de grupo nenhum.
inline std::size_t indexOf(char letter)
{
    for (std::size_t g = 0; g < kCount; ++g)
    {
        if (kLetters[g] == letter)
            return g;
    }
    return kCount;
}

// O inverso de toLetters(). Devolve false (e não mexe em mask) se aparecer
// uma letra que não é de grupo.
inline bool fromLetters(const std::string& letters, unsigned& mask)
{
    unsigned result = 0;
    for (char letter : letters)
    {
        const std::size_t g = indexOf(letter);
        if (g == kCount)
            return false;
        result |= bit(g);
    }
    mask = result;
    return true;
}

// A regra do clique no grupo, a partir de quais membros estão ligados.
// Devolve true quando o clique deve LIGAR todos: basta um desligado. Só
// desliga quando todos já estão ligados. No palco isso é previsível: com o
// grupo "meio ligado", apertar sempre leva ao som completo do grupo.
inline bool shouldTurnOn(const std::vector<bool>& memberActive)
{
    for (bool active : memberActive)
    {
        if (!active)
            return true;
    }
    return false;
}
}
