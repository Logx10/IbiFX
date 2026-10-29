#pragma once

#include <string>
#include <vector>

// A aparência com imagens de um equipamento do catálogo.
//
// É só apresentação: o equipamento (receita, knobs, faixas) continua sendo
// o GearModel da GearLibrary, e o som não muda tendo ou não uma skin. Um
// equipamento sem skin é desenhado pelo painel vetorial de sempre
// (DesktopUI::drawGearPanel), então as imagens podem chegar uma de cada vez.
//
// As imagens ficam em assets/gear/<gearId>/ (body.png, knob.png) e as peças
// comuns a todos em assets/gear/_shared/ (LED e footswitch). Elas são
// preparadas por tools/gear_assets.py, que também mede as posições abaixo
// ("holes"). Ver docs/prompt-assets.md.
//
// POSIÇÕES EM FRAÇÃO DO CORPO
// x e y são o centro da peça como fração da largura e da altura do
// body.png; diâmetros são fração da largura. Assim a skin vale em qualquer
// tamanho de desenho, e reprocessar a imagem em outra resolução não muda
// nenhum número.
struct GearSkin
{
    struct Spot
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    // Um knob da imagem, ligado ao controle do GearModel pelo id do
    // parâmetro, não pela posição na lista — reordenar os controles no
    // catálogo não troca os knobs de lugar.
    struct Knob
    {
        const char* parameterId = "";
        Spot center;
    };

    const char* gearId = "";

    // Largura do corpo no editor, em pixels de tela. O body.png é maior
    // (480 px), para ficar nítido em tela Retina.
    float displayWidth = 300.0f;

    float knobDiameter = 0.0f;
    std::vector<Knob> knobs;

    float ledDiameter = 0.0f;

    // LED que acende com o equipamento ligado.
    Spot statusLed;

    // LEDs desenhados no corpo que ainda não têm dado do motor para
    // acender (o REDUCTION do Noise Gate precisaria do estado do gate).
    // Ficam apagados.
    std::vector<Spot> unlitLeds;

    bool hasFootswitch = false;
    Spot footswitch;
    float footswitchWidth = 0.0f;
};

// nullptr se o equipamento ainda não tem imagens.
const GearSkin* findGearSkin(const std::string& gearId);
