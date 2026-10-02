#include "Serialization.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace
{
std::string trim(const std::string& text)
{
    const std::size_t begin = text.find_first_not_of(" \t\r");

    if (begin == std::string::npos)
        return "";

    const std::size_t end = text.find_last_not_of(" \t\r");
    return text.substr(begin, end - begin + 1);
}
}

namespace preset
{
std::string serialize(const Preset& p)
{
    std::ostringstream out;

    // max_digits10 (9, para float) é o número de dígitos que garante ida e
    // volta exata — menos que isso arriscaria o preset carregar com um
    // parâmetro ligeiramente diferente do que foi salvo.
    out << std::setprecision(9);

    out << "preset " << p.name << '\n';

    for (const Preset::ModuleState& state : p.modules)
    {
        out << "module " << state.type << ' ' << (state.bypassed ? 1 : 0) << '\n';

        if (!state.irPath.empty())
            out << "ir " << state.irPath << '\n';

        for (const auto& [id, value] : state.parameters)
            out << "param " << id << ' ' << value << '\n';
    }

    return out.str();
}

Preset deserialize(const std::string& text)
{
    Preset result;
    std::istringstream in(text);
    std::string line;

    // Aponta para o último módulo acrescentado em result.modules, para que
    // as linhas "param"/"ir" seguintes saibam a quem pertencem. Só é lido
    // logo após um push_back, nunca atravessando um — por isso continua
    // válido mesmo com o vector realocando por baixo.
    Preset::ModuleState* current = nullptr;

    while (std::getline(in, line))
    {
        const std::string trimmed = trim(line);

        if (trimmed.empty() || trimmed[0] == '#')
            continue;

        std::istringstream lineStream(trimmed);
        std::string keyword;
        lineStream >> keyword;

        if (keyword == "preset")
        {
            std::string rest;
            std::getline(lineStream, rest);
            result.name = trim(rest);
        }
        else if (keyword == "module")
        {
            std::string type;
            int bypassed = 0;

            if (!(lineStream >> type >> bypassed))
                throw std::runtime_error("preset malformado: 'module' precisa de tipo e bypass (0/1)");

            Preset::ModuleState state;
            state.type = type;
            state.bypassed = (bypassed != 0);
            result.modules.push_back(std::move(state));
            current = &result.modules.back();
        }
        else if (keyword == "ir")
        {
            if (current == nullptr)
                throw std::runtime_error("preset malformado: 'ir' antes de qualquer 'module'");

            std::string rest;
            std::getline(lineStream, rest);
            current->irPath = trim(rest);
        }
        else if (keyword == "param")
        {
            if (current == nullptr)
                throw std::runtime_error("preset malformado: 'param' antes de qualquer 'module'");

            std::string id;
            float value = 0.0f;

            if (!(lineStream >> id >> value))
                throw std::runtime_error("preset malformado: 'param' precisa de id e valor");

            current->parameters.emplace_back(id, value);
        }
        else
        {
            throw std::runtime_error("preset malformado: linha desconhecida '" + trimmed + "'");
        }
    }

    return result;
}
}
