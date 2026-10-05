#pragma once

#include <string>
#include <vector>

#include "Preset.h"

// GearLibrary — o catálogo de equipamentos do IbiFX: pedais, amplificadores,
// gabinetes (cada um com seus microfones) e efeitos de rack, no espírito do
// navegador de "gear" do AmpliTube.
//
// UM EQUIPAMENTO NÃO É UM MÓDULO NOVO DE DSP
// "Brit 800" não tem algoritmo próprio: é o Preamp, o ToneStack e o PowerAmp
// que já existem, numa receita específica de valores (mais ganho, mais médio,
// menos sag). Por isso um GearModel é só DADO — uma lista de
// Preset::ModuleState, o mesmo formato que o preset já sabe aplicar numa
// ModuleChain. A biblioteca não cria módulos; ela monta um Preset, e quem o
// aplica é o preset::apply() de sempre. Um equipamento novo é uma entrada
// nova no catálogo, não uma classe nova.
//
// AS QUATRO CATEGORIAS E A ORDEM DA CADEIA
// Seguem a ordem física de um rig de guitarra:
//
//     Stomp (pedais no chão) -> Amp -> Cabinet (+ microfone) -> Rack (estúdio)
//
// Rack é onde moram delay e reverb de estúdio: depois do microfone, como num
// estúdio de verdade, molhando o som já gravado do gabinete.
//
// CABINETS VÊM DO DISCO, NÃO DO CÓDIGO
// Pedais e amplis são receitas de parâmetros e cabem no código. Um gabinete
// com microfone é uma impulse response gravada — um arquivo .wav que o
// código não tem como inventar. scanImpulseResponses() lê uma pasta assim:
//
//     irs/
//       4x12 Brit/
//         SM57 On-Axis.wav       -> cabinet "4x12 Brit", microfone "SM57 On-Axis"
//         Ribbon Room.wav
//       1x12 Open Back/
//         Condenser.wav
//
// Uma pasta = um gabinete; um .wav dentro dela = um microfone (ou posição)
// gravado naquele gabinete.
//
// NOMES INSPIRADOS, NÃO DE MARCA
// Os nomes lembram os equipamentos clássicos (como o próprio AmpliTube faz
// com os modelos não licenciados), mas não usam marcas registradas.
enum class GearCategory
{
    Stomp,
    Amp,
    Cabinet,
    Rack
};

// Texto para exibição: "Stomp", "Amp", "Cabinet", "Rack".
const char* gearCategoryName(GearCategory category);

// Um knob do painel de um equipamento: qual parâmetro de qual módulo da
// receita ele gira, e em que faixa. A faixa é a do KNOB, não a do
// Parameter: o "Master" de um ampli gira o ganho de 0 a um teto pequeno,
// nunca até -8 (que inverteria a fase), mesmo que o Gain aceite isso.
struct GearControl
{
    std::string label;

    // Posição do módulo dentro de GearModel::modules.
    std::size_t moduleOffset = 0;

    std::string parameterId;
    float minValue = 0.0f;
    float maxValue = 1.0f;
};

struct GearModel
{
    // Identificador estável ("amp.brit-800"). É o que um rig guarda.
    std::string id;

    std::string name;
    GearCategory category = GearCategory::Stomp;

    // Subtipo para filtrar no navegador: "Overdrive", "Clean", "High Gain"...
    std::string character;

    std::string description;

    // Só cabinets: o microfone/posição. Vazio nas outras categorias.
    std::string microphone;

    // O que este equipamento vira dentro da cadeia, em ordem.
    std::vector<Preset::ModuleState> modules;

    // Os knobs do painel. Quem tem um módulo só (um Delay, um Noise Gate)
    // ganha um knob por parâmetro automaticamente, na faixa do próprio
    // Parameter — ver o construtor de GearLibrary.
    std::vector<GearControl> controls;
};

// A escolha de equipamentos de um rig, por id. Vazio = sem amp / sem cabinet.
struct Rig
{
    std::vector<std::string> stomps;
    std::string amp;
    std::string cabinet;
    std::vector<std::string> rack;
};

class GearLibrary
{
public:
    // Já nasce com os pedais, amplis e racks embutidos. Cabinets só depois
    // de scanImpulseResponses().
    GearLibrary();

    // Procura cabinets em directory (ver o formato acima) e os acrescenta ao
    // catálogo. Pode ser chamada de novo: troca os cabinets anteriores pelos
    // novos. Pasta inexistente não é erro — só não acrescenta nada.
    // Devolve quantos cabinets foram encontrados.
    std::size_t scanImpulseResponses(const std::string& directory);

    const std::vector<GearModel>& models() const;

    // Todos os modelos de uma categoria, na ordem do catálogo.
    std::vector<const GearModel*> modelsIn(GearCategory category) const;

    // nullptr se o id não existir.
    const GearModel* find(const std::string& id) const;

    // Os equipamentos do rig na ordem em que entram na cadeia: stomps, amp,
    // cabinet e rack. Lança std::runtime_error se um id não existir ou
    // estiver na categoria errada (um amp na lista de stomps, por exemplo).
    std::vector<const GearModel*> rigModels(const Rig& rig) const;

    // Monta o preset do rig inteiro, na ordem de rigModels(), sempre
    // terminando no Limiter (mesma garantia de buildChain()). Lança nos
    // mesmos casos de rigModels().
    Preset buildPreset(const Rig& rig, const std::string& name) const;

private:
    const GearModel& require(const std::string& id, GearCategory expected) const;

    std::vector<GearModel> m_models;
};
