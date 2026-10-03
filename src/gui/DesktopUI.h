#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "LiveEngine.h"

struct SDL_Window;
struct SDL_Renderer;

// DesktopUI — a mesma pedaleira da PedalboardUI, desenhada com Dear ImGui
// numa janela SDL em vez de caracteres de terminal (Fase 12, AI_GUIDELINES
// §36, decisão em docs/adr/0001-portabilidade-desktop-e-web.md).
//
// ONDE ELA SE ENCAIXA
// É uma CAMADA sobre o engine, como o princípio 2 do ARCHITECTURE pede —
// lê o estado para desenhar e envia comandos para alterar, sem calcular
// nenhuma amostra. As mesmas duas regras da PedalboardUI valem aqui:
//
//   interface -> áudio    fila de comandos (setParameter, setBypassed)
//   áudio -> interface    valores atômicos (picos)
//
// POR QUE NÃO REAPROVEITA PedalboardUI POR LINK
// PedalboardUI é construída em cima de Terminal (termios, texto, teclado)
// do início ao fim — não sobra nada ali que não seja desenho de caractere.
// O que de fato é reaproveitável é conceitual: percorrer a cadeia
// genericamente por AudioModule::parameterAt(), guardar um "alvo" local
// por parâmetro (ver m_targets abaixo) e nunca travar a thread de áudio.
// Esta classe replica essas mesmas regras com Dear ImGui no lugar de
// Terminal.
class DesktopUI
{
public:
    // O engine precisa ter a cadeia montada antes de entrar aqui.
    explicit DesktopUI(LiveEngine& engine);

    // Roda o laço da janela até ela ser fechada. Devolve o código de saída.
    int run(AudioDevice::Mode mode, double sampleRate, int blockSize);

private:
    bool initWindow();
    void shutdownWindow();

    // Preenche m_targets a partir dos valores atuais de todos os
    // parâmetros da cadeia.
    void captureTargets();

    // Repõe, só para os parâmetros de um módulo, os valores padrão — usado
    // depois de um resetModule(), que some para o padrão do lado do áudio
    // sem a interface saber quando o comando foi de fato aplicado.
    void captureModuleTargets(std::size_t moduleIndex, std::size_t firstFlatIndex);

    // Posição, na lista achatada m_targets, do primeiro parâmetro de um
    // módulo — soma o parameterCount() de todos os módulos anteriores.
    std::size_t firstFlatIndexForModule(std::size_t moduleIndex) const;

    void drawFrame();

    // Lista de presets, em disco (presets/*.ibifxpreset), para a seção de
    // presets da janela.
    void refreshPresetList();

    // Troca a cadeia INTEIRA pela do preset. Para o motor antes, porque
    // preset::apply() chama ModuleChain::clear()+add() — realocar o vetor
    // de módulos com a thread de áudio percorrendo ele é o tipo de corrida
    // que ModuleChain.h documenta como proibido. Parar/trocar/religar é o
    // caminho seguro já existente (LiveEngine::stop()/start()), ao custo de
    // um corte breve no som durante a troca — aceitável para uma ação
    // deliberada como esta, bem diferente de girar um knob.
    void loadPreset(const std::string& path);

    // Captura os valores ATUAIS da cadeia (não m_targets) num preset novo
    // e grava em presets/<nome>.ibifxpreset.
    void savePreset(const std::string& name);

    void drawPresetPanel();

    // Desenha um módulo como um pedal de verdade: corpo colorido, nome no
    // topo, knobs em grade e um footswitch redondo embaixo (bypass). Lado
    // a lado na mesma linha, como um pedalboard — drawFrame() decide a
    // quebra de linha, este método só desenha UM pedal.
    void drawPedal(std::size_t moduleIndex, std::size_t firstFlatIndex);

    // Barra de nível em dB, igual em espírito a PedalboardUI::meter(), só
    // desenhada com ImGui::ProgressBar em vez de caracteres.
    void drawMeter(const char* label, float peakLinear) const;

    LiveEngine& m_engine;

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    // A POSIÇÃO DOS CONTROLES É ESTADO DA INTERFACE, NÃO DO ÁUDIO.
    //
    // Mesmo motivo do m_targets da PedalboardUI: os comandos atravessam
    // uma fila assíncrona, e arrastar um slider manda muitos valores por
    // segundo. Se a interface relesse parameter.value() a cada quadro para
    // calcular o próximo, cada leitura pegaria um valor ainda não
    // atualizado pela thread de áudio, e o controle pareceria "preso".
    // Guardando o alvo aqui, o slider sempre parte de onde a MÃO o deixou.
    std::vector<float> m_targets;

    bool m_quit = false;

    // Guardados para que loadPreset() consiga religar o motor com a mesma
    // configuração depois de pará-lo — mesmo padrão de PedalboardUI.
    AudioDevice::Mode m_mode = AudioDevice::Mode::Null;
    double m_sampleRate = 48000.0;
    int m_blockSize = 128;

    // presets/*.ibifxpreset encontrados em disco, recarregado após salvar.
    std::vector<std::string> m_presetFiles;

    // Buffer de texto do campo "nome do preset" — ImGui::InputText precisa
    // de um buffer de tamanho fixo, não de um std::string.
    char m_presetNameBuffer[64] = "";

    // Resultado da última ação de preset (carregado/salvo/erro), mostrado
    // na própria janela até a próxima ação substituir.
    std::string m_presetMessage;
};
