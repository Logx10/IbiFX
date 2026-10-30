#include "PresetManager.h"

#include <filesystem>
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

    Preset loaded = deserialize(buffer.str());

    // Uma IR relativa normalmente é escrita a partir da raiz do projeto
    // (irs/...). Ao abrir o preset de outra pasta, procure-a ao lado do
    // preset e nos diretórios acima. Caminhos absolutos continuam intactos.
    namespace fs = std::filesystem;
    for (auto& module : loaded.modules)
    {
        if (module.irPath.empty() || fs::path(module.irPath).is_absolute()
            || fs::exists(module.irPath))
            continue;

        for (fs::path directory = fs::absolute(path).parent_path(); !directory.empty();
             directory = directory.parent_path())
        {
            const fs::path candidate = directory / module.irPath;
            if (fs::exists(candidate))
            {
                module.irPath = candidate.lexically_normal().string();
                break;
            }
            if (directory == directory.root_path())
                break;
        }
    }

    return loaded;
}
}
