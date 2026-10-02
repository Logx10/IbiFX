#include "PresetManager.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "Serialization.h"

namespace preset
{
void save(const Preset& p, const std::string& path)
{
    std::ofstream out(path);

    if (!out)
        throw std::runtime_error("nao foi possivel criar o arquivo de preset: " + path);

    out << serialize(p);

    if (!out)
        throw std::runtime_error("falha ao escrever o preset: " + path);
}

Preset load(const std::string& path)
{
    std::ifstream in(path);

    if (!in)
        throw std::runtime_error("nao foi possivel abrir o arquivo de preset: " + path);

    std::ostringstream buffer;
    buffer << in.rdbuf();

    return deserialize(buffer.str());
}
}
