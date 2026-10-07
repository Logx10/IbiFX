#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "GearLibrary.h"
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
//
// O NAVEGADOR DE EQUIPAMENTOS (painel da direita)
// Mostra a GearLibrary em abas Stomp / Amp / Cab / Rack, como o navegador
// do AmpliTube. Clicar num item muda m_rig, e o rig inteiro vira um Preset
// novo aplicado na cadeia (applyRig()). Uma troca de equipamento é
// estrutural (módulos entram e saem da cadeia), então ela passa pelo mesmo
// caminho de parar/aplicar/religar de loadPreset(), não pela fila de
// comandos — e por isso os knobs mexidos à mão voltam aos valores da
// receita a cada troca.
class DesktopUI
{
public:
    // O engine precisa ter a cadeia montada antes de entrar aqui.
    // irsDirectory: onde o navegador procura cabinets.
    explicit DesktopUI(LiveEngine& engine, std::string irsDirectory = "irs");

    // Avisa que a cadeia do engine JÁ foi montada a partir deste rig (por
    // --amp/--stomp... na linha de comando), para a janela abrir com ele
    // ativo no navegador e agrupado no pedalboard. Não reaplica nada. Os
    // ids precisam estar na forma completa (ver resolveRig()).
    void adoptRig(const Rig& rig);

    // Avisa que a cadeia do engine JÁ foi montada a partir deste preset
    // (por --preset na linha de comando). Como loadPreset(), mas sem
    // reaplicar: reagrupa os módulos em painéis se o preset veio de um rig
    // e marca o preset como o atual na lista.
    void adoptPreset(const Preset& loaded, const std::string& path);

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

    // O caminho comum de loadPreset() e applyRig(): para o motor, aplica o
    // preset, religa e recaptura os alvos dos knobs. Devolve false (com a
    // mensagem em error) se algo falhar.
    bool replaceChain(const Preset& preset, std::string& error);

    // Monta next como preset e o aplica na cadeia, guardando em
    // m_rigSegments qual faixa de módulos pertence a cada equipamento. Só
    // vira m_rig se der certo — uma IR que não carrega deixa o rig antigo.
    void applyRig(const Rig& next);

    // Painel da direita: abas de categoria, filtro por character e a lista
    // de equipamentos.
    void drawGearBrowser();

    // Faixa com o rig atual (stomps, amp, cab, rack), com botões para
    // remover e reordenar stomps/rack.
    void drawRigStrip();

    // Os pedais da cadeia, agrupados por equipamento quando há um rig ativo.
    void drawPedalboard();

    // Desenha um equipamento com painel (GearModel::controls) como UMA
    // peça só: um ampli com a faixa de knobs de 0 a 10, ou um pedal com
    // um footswitch que liga/desliga todos os módulos da receita juntos.
    void drawGearPanel(std::size_t segmentIndex, const GearModel& model);

    // Índice, dentro de m_targets, de um parâmetro de um módulo da cadeia
    // procurado pelo id. Devolve false se o módulo não tiver esse id.
    bool flatIndexFor(std::size_t moduleIndex, const std::string& parameterId,
                      std::size_t& parameterIndex, std::size_t& flatIndex) const;

    // Captura os valores ATUAIS da cadeia (não m_targets) num preset novo
    // e grava em presets/<nome>.ibifxpreset.
    void savePreset(const std::string& name);

    void drawPresetPanel();

    // Título de seção no estilo da janela: cor de destaque, um pouco maior
    // que o texto comum, com uma linha fina embaixo — separa visualmente
    // "Presets", "Níveis" e "Pedalboard" sem precisar de uma caixa cheia.
    void drawSectionHeader(const char* label) const;

    // Desenha um módulo como um pedal de verdade: corpo colorido, nome no
    // topo, knobs em grade e um footswitch redondo embaixo (bypass). Lado
    // a lado na mesma linha, como um pedalboard — drawFrame() decide a
    // quebra de linha, este método só desenha UM pedal.
    void drawPedal(std::size_t moduleIndex, std::size_t firstFlatIndex);

    // Barra de nível em dB, igual em espírito a PedalboardUI::meter(), só
    // desenhada com ImGui::ProgressBar em vez de caracteres.
    void drawMeter(const char* label, float peakLinear) const;

    // A faixa de ferramentas, três cartões lado a lado: afinador, gravador
    // de reamp e modo prática. Nada disso passa pela cadeia — são as peças
    // que o LiveEngine já mantém sempre presentes (tuner(),
    // reampRecorder(), practiceSession()).
    void drawToolsRow();
    void drawTunerPanel();
    void drawRecorderPanel();
    void drawPracticePanel();

    // audio/*.wav, para o seletor de backing track.
    void refreshBackingTrackList();

    // Troca o arquivo da backing track. Para o motor antes, pelo mesmo
    // motivo de replaceChain(): BackingTrackPlayer::load() substitui o
    // vetor de amostras que a thread de áudio está lendo.
    void loadBackingTrack(const std::string& path);

    // Copia um .wav qualquer para irs/Importados/, relê o catálogo e já põe
    // esse cabinet no rig. Chamado pelo botão "Importar IR..." e por
    // arrastar um arquivo para a janela.
    void importImpulseResponse(const std::string& sourcePath);

    // Apaga do disco o .wav de um cabinet (pelo id do catálogo), tira esse
    // cabinet do rig se for o atual e relê irs/. A pasta do gabinete some
    // junto quando fica vazia.
    void deleteImpulseResponse(const std::string& cabinetId);

    // Abre o seletor de arquivos do sistema. Ele é assíncrono e o SDL pode
    // chamar a resposta de OUTRA thread, então o caminho escolhido só é
    // depositado em m_pendingIrPath — quem importa é o laço da janela, no
    // próximo quadro (ver run()).
    void openImpulseResponseDialog();
    static void onImpulseResponseChosen(void* userdata, const char* const* files, int filter);

    // Para as gravações em andamento, para os arquivos serem escritos antes
    // de a janela fechar.
    void stopAllRecordings();

    LiveEngine& m_engine;
    std::string m_irsDirectory;

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

    // Nome do preset carregado por último (Preset::name, não o nome do
    // arquivo) — só para mostrar em destaque. Vazio antes de qualquer load.
    std::string m_currentPresetName;

    // Caminho exato do último preset carregado ou salvo — usado para
    // realçar a linha certa na lista (comparar por CAMINHO, não por nome:
    // o nome dentro do preset pode não ter nenhuma relação com o nome do
    // arquivo, como "Slash (Marshall lead/rhythm, GNR)" vs "slash.ibifxpreset").
    std::string m_currentPresetPath;

    GearLibrary m_library;
    Rig m_rig;

    // false enquanto a cadeia veio de flags ou de um preset — aí ela não
    // corresponde a m_rig, e o pedalboard não tem como agrupar por
    // equipamento.
    bool m_rigActive = false;

    // Um equipamento do rig visto como faixa de módulos da cadeia:
    // [firstModule, firstModule + moduleCount).
    struct RigSegment
    {
        std::string label;

        // Vazio para o grupo "Saida" (o Limiter), que não é um equipamento.
        std::string gearId;

        std::size_t firstModule = 0;
        std::size_t moduleCount = 0;
    };
    std::vector<RigSegment> m_rigSegments;

    // Segmentos que a pessoa abriu para ver "por dentro" — os módulos crus
    // da receita em vez do painel. Zerado a cada applyRig().
    std::set<std::size_t> m_expandedSegments;

    GearCategory m_browserCategory = GearCategory::Amp;

    // Vazio = todos os "character" da categoria aberta.
    std::string m_browserCharacter;

    std::string m_rigMessage;

    // Caminho escolhido no seletor de IR, esperando o laço da janela —
    // ver openImpulseResponseDialog().
    std::mutex m_pendingIrMutex;
    std::string m_pendingIrPath;

    // Cabinet esperando confirmação de exclusão no popup. Apagar um arquivo
    // não tem desfazer, então o "x" da lista só pergunta.
    std::string m_irPendingDelete;

    // --- Ferramentas ---
    // Cópias locais do que o engine guarda em atômicos: ImGui::Checkbox e
    // os sliders precisam de um bool/float para escrever.
    bool m_tunerOn = false;
    bool m_tunerMuted = true;

    std::chrono::steady_clock::time_point m_reampStartedAt;
    std::string m_reampMessage;

    bool m_metronomeOn = false;
    float m_bpm = 120.0f;
    float m_metronomeVolume = 0.5f;
    float m_backingVolume = 1.0f;
    bool m_loopOn = false;
    std::uint64_t m_loopStart = 0;
    std::uint64_t m_loopEnd = 0;

    std::vector<std::string> m_backingFiles;
    std::string m_backingTrackPath;

    std::chrono::steady_clock::time_point m_sessionStartedAt;
    std::string m_practiceMessage;
};
