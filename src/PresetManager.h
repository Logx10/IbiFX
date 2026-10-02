#pragma once

#include <string>

#include "Preset.h"

// PresetManager — grava e lê presets em disco, usando o texto de
// Serialization.h.
//
// Erros de arquivo (caminho inexistente, sem permissão, diretório sem
// escrita) viram std::runtime_error — mesmo espírito de wav::read/write:
// falhar aqui é barato, falhar depois com um preset incompleto carregado em
// silêncio custaria uma sessão de depuração.
namespace preset
{
// Grava o preset num arquivo de texto.
void save(const Preset& p, const std::string& path);

// Lê um preset de um arquivo de texto.
Preset load(const std::string& path);
}
