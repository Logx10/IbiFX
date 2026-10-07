#pragma once

#include <string>
#include <utility>
#include <vector>

class ModuleChain;

// Preset — o estado completo de uma cadeia de módulos: ordem, bypass e
// parâmetros (AI_GUIDELINES §35).
//
// POR QUE É SÓ DADO, SEM LÓGICA PRÓPRIA
// Mesmo desenho do WavFile: a struct guarda o que foi lido ou o que vai ser
// escrito, e quem sabe ir e vir de uma ModuleChain são as funções livres do
// namespace preset, logo abaixo. Texto (Serialization.h) e arquivo
// (PresetManager.h) são outros dois assuntos, em outros dois arquivos.
//
// UM MÓDULO NÃO É ESPECIAL POR SER "O AMPLI" OU "O CABINET"
// A lista é plana: cada entrada é só um tipo, um bypass e os parâmetros que
// tinha. Preamp, ToneStack, PowerAmp e Cabinet entram do mesmo jeito que
// Delay ou Reverb — é o próprio buildChain() em main.cpp que decide o que
// cada um significa musicalmente, não o preset.
//
// A EXCEÇÃO: A IR DO CABINET
// Cabinet carrega a impulse response de um arquivo, e esse caminho não é um
// Parameter — não tem faixa numérica nem faz sentido suavizar. ModuleState
// guarda esse caso à parte (irPath), em vez de forçar Parameter a
// representar texto.

// A escolha de equipamentos de um rig da GearLibrary, por id. Vazio = sem
// amp / sem cabinet. Mora aqui, e não em GearLibrary.h, porque o Preset
// também guarda um (ver Preset::rig) e GearLibrary.h já inclui este arquivo.
struct Rig
{
    std::vector<std::string> stomps;
    std::string amp;
    std::string cabinet;
    std::vector<std::string> rack;

    bool empty() const
    {
        return stomps.empty() && amp.empty() && cabinet.empty() && rack.empty();
    }
};

struct Preset
{
    // Um módulo salvo: o tipo (o mesmo texto que AudioModule::name()
    // devolve), se estava em bypass, e o valor de cada parâmetro pelo id.
    struct ModuleState
    {
        std::string type;
        bool bypassed = false;
        std::vector<std::pair<std::string, float>> parameters;

        // Só usado por Cabinet. Vazio para qualquer outro módulo.
        std::string irPath;
    };

    std::string name;

    // Na ordem em que os módulos aparecem na cadeia.
    std::vector<ModuleState> modules;

    // De qual rig da GearLibrary os módulos vieram, se vieram de um. É só
    // um rótulo: preset::apply() ignora, quem aplica continua sendo a
    // lista de módulos acima (com os knobs como a pessoa deixou, não como
    // a receita manda). Serve para a janela desktop reagrupar os módulos
    // em painéis de equipamento ao carregar. Vazio em presets montados por
    // flags ou salvos antes de existir.
    Rig rig;
};

namespace preset
{
// Lê o estado atual da cadeia — ordem, bypass e parâmetros de cada módulo —
// num Preset novo.
Preset capture(const std::string& name, const ModuleChain& chain);

// Esvazia a cadeia e a reconstrói a partir do preset.
//
// Chamada do domínio de controle, nunca da thread de áudio: clear() e add()
// realocam, como já documentado em ModuleChain. Trocar de preset com o áudio
// rodando exige montar a cadeia nova fora e trocá-la por ponteiro atômico —
// o mesmo limite que já existia antes deste arquivo, não algo que ele
// introduz.
//
// Lança std::runtime_error se algum tipo de módulo salvo não for
// reconhecido.
void apply(const Preset& preset, ModuleChain& chain);
}
