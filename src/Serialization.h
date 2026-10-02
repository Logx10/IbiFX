#pragma once

#include <string>

#include "Preset.h"

// Serialization — conversão entre Preset e um texto humano e depurável
// (AI_GUIDELINES §35).
//
// POR QUE NÃO JSON
// JSON pediria uma dependência nova, e o projeto não tem nenhuma além do
// miniaudio (vendorizado, um único header) — o próprio WavFile.h escreve seu
// parser de RIFF à mão pelo mesmo motivo. O formato aqui segue a mesma
// escolha: orientado a linha, cada uma começando com uma palavra que diz o
// que ela é, legível sem ferramenta nenhuma e pequeno o bastante para caber
// em poucas dezenas de linhas de parser.
//
// O FORMATO, EM UM EXEMPLO
//
//     preset Lead Classico
//     module NoiseGate 0
//     param threshold 0.02
//     param release 0.15
//     module Cabinet 0
//     ir audio/ir/bassman.wav
//     param mix 1
//
// `module <tipo> <0 ou 1 de bypass>` abre um módulo novo; as linhas `param`
// e `ir` seguintes pertencem a ele, até o próximo `module`. Linhas em branco
// e começadas com '#' são ignoradas.
namespace preset
{
// Converte um Preset no seu texto. Sempre termina em '\n'.
std::string serialize(const Preset& p);

// Lê um Preset a partir do texto no formato acima.
//
// Lança std::runtime_error em linha malformada (module/param/ir sem os
// campos esperados, ou palavra-chave desconhecida). Um tipo de módulo que
// não existe não é erro AQUI — só vira erro em preset::apply(), que é quem
// de fato tenta construir o módulo.
Preset deserialize(const std::string& text);
}
