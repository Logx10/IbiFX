#pragma once

#include <string>

#include "ModuleChain.h"

// ChainSettings — a configuração do pedalboard padrão do IbiFX: quais
// módulos entram, e com quais valores. A ORDEM é decidida dentro de
// buildChain(), em PedalboardChain.cpp, não aqui.
//
// POR QUE ISTO FICA NO CORE, E NÃO NO CLI
// Mais de um frontend monta essa mesma cadeia — o CLI (flags de linha de
// comando) e a janela desktop (sliders). Cada um decide COMO preencher os
// campos, mas o resultado — quais módulos, em que ordem — precisa ser um
// só. Duplicar buildChain() em cada frontend arriscaria os dois
// divergirem sem ninguém notar: um ganharia um módulo novo e o outro
// ficaria pra trás em silêncio.
//
// Os padrões abaixo são deliberadamente agressivos: foram escolhidos para
// a diferença entre entrada e saída ficar óbvia, não para soar bonito.
struct ChainSettings
{
    float gateThreshold = 0.02f;
    float gateRelease = 0.15f;
    float compThreshold = -20.0f;
    float compRatio = 4.0f;
    float compAttack = 0.01f;
    float compRelease = 0.15f;
    float highPass = 100.0f;
    float gain = 6.0f;
    float drive = 4.0f;
    float bias = 0.3f;
    float delayTime = 0.28f;
    float feedback = 0.45f;
    float mix = 0.35f;
    float reverbDecay = 0.5f;
    float reverbDamping = 0.5f;
    float reverbMix = 0.3f;
    float toneBass = 0.5f;
    float toneMid = 0.5f;
    float toneTreble = 0.5f;
    float preampDrive = 1.0f;
    float powerAmpDrive = 1.0f;
    float powerAmpSag = 0.3f;
    float cabinetMix = 1.0f;

    bool useGate = true;
    bool useCompressor = true;
    bool useHighPass = true;
    bool useDrive = true;
    bool useAsymmetric = false;
    bool usePreamp = false;
    bool useToneStack = false;
    bool useDelay = true;
    bool useReverb = true;
    bool usePowerAmp = false;

    // Vazio = sem cabinet. Diferente das outras flags "use...", esta
    // precisa de um caminho de arquivo, não só de ligar/desligar.
    std::string cabinetIRPath;
};

// Monta a cadeia a partir de settings, na ordem clássica de pedaleira — ver
// os comentários dentro de PedalboardChain.cpp para o raciocínio de cada
// posição.
void buildChain(ModuleChain& chain, const ChainSettings& settings);

// Atalho para buildChain(chain, ChainSettings{}) — a cadeia com todos os
// valores padrão.
void buildDefaultChain(ModuleChain& chain);
