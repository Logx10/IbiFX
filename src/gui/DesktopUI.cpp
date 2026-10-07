#include "DesktopUI.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

#include "Preset.h"
#include "PresetManager.h"

namespace
{
// Onde os presets salvos pela janela vivem, relativo ao diretório de
// trabalho — mesma convenção de audio/ no CLI.
constexpr const char* kPresetsDir = "presets";

// Onde o seletor de backing track procura .wav (a mesma pasta de áudio de
// teste do CLI) e onde as gravações da janela são escritas.
constexpr const char* kAudioDir = "audio";
constexpr const char* kRecordingsDir = "recordings";

// Altura comum dos três cartões de ferramentas, para a fileira ficar
// alinhada mesmo com conteúdos de tamanhos diferentes.
constexpr float kToolCardHeight = 250.0f;

// Desvio, em cents, que ainda conta como "afinado" no afinador — o mesmo
// limiar que afinadores de pedal costumam usar para acender o verde.
constexpr float kInTuneCents = 5.0f;

constexpr int kWindowWidth = 1200;
constexpr int kWindowHeight = 900;

// Dimensões do corpo de um pedal — ver drawPedal() para o layout completo.
constexpr float kPedalWidth = 168.0f;
constexpr float kPedalPadding = 14.0f;
constexpr float kPedalKnobRadius = 17.0f;
constexpr float kPedalHeaderHeight = 30.0f;
constexpr float kPedalKnobRowHeight = 92.0f;
constexpr float kPedalFootswitchAreaHeight = 86.0f;
constexpr float kFootswitchRadius = 20.0f;

// Largura do navegador de equipamentos, à direita.
constexpr float kBrowserWidth = 330.0f;

constexpr ImVec4 kAccentColor(0.90f, 0.55f, 0.15f, 1.0f);

// O painel de um ampli: uma célula por knob, numa fileira só.
constexpr float kAmpKnobCellWidth = 86.0f;
constexpr float kAmpKnobRadius = 20.0f;

// Largura do painel de um equipamento — drawPedalboard() precisa dela
// antes de desenhar, para decidir se ainda cabe na mesma fileira.
float gearPanelWidth(const GearModel& model)
{
    if (model.category == GearCategory::Amp)
        return kAmpKnobCellWidth * static_cast<float>(model.controls.size()) + 2.0f * kPedalPadding;
    return kPedalWidth;
}

// As fileiras do pedalboard, na ordem do sinal. Amp e cabinet dividem
// uma: no mundo real, é a cabeça em cima da caixa.
int rowFor(GearCategory category)
{
    switch (category)
    {
    case GearCategory::Stomp: return 0;
    case GearCategory::Amp:
    case GearCategory::Cabinet: return 1;
    case GearCategory::Rack: return 2;
    }
    return 3;
}

const char* rowTitle(int row)
{
    switch (row)
    {
    case 0: return "Stomps";
    case 1: return "Amp + Cab";
    case 2: return "Rack";
    }
    return "";
}

// Um knob giratório, no estilo de um pedal de guitarra de verdade — o
// ImGui não tem widget pronto pra isso, só sliders retos. Arrastar
// verticalmente gira o knob: pra cima aumenta, pra baixo diminui, como
// segurar o knob de verdade e girar o pulso.
//
// O ARCO VAI DE 135° A 405° (=45°)
// É a mesma convenção de um knob físico: o batente mínimo fica às 7 horas,
// o máximo às 5 horas, passando por cima (12 horas) no meio do caminho —
// nunca um giro completo, que não teria como indicar "ponta a ponta".
constexpr float kKnobAngleMin = 3.14159265f * 0.75f;
constexpr float kKnobAngleMax = 3.14159265f * 2.25f;

bool knob(const char* id, float* value, float minValue, float maxValue, float radius)
{
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const ImVec2 topLeft = ImGui::GetCursorScreenPos();
    const ImVec2 center(topLeft.x + radius, topLeft.y + radius);

    ImGui::InvisibleButton(id, ImVec2(radius * 2.0f, radius * 2.0f));

    const bool active = ImGui::IsItemActive();
    bool changed = false;

    if (active && io.MouseDelta.y != 0.0f)
    {
        // A faixa inteira do parâmetro cabe em 200 pixels de arraste —
        // rápido o bastante pra não cansar, fino o bastante pra não
        // passar direto pelo valor que se queria.
        const float speed = (maxValue - minValue) / 200.0f;
        *value = std::clamp(*value - io.MouseDelta.y * speed, minValue, maxValue);
        changed = true;
    }

    const float fraction = (maxValue > minValue) ? (*value - minValue) / (maxValue - minValue) : 0.0f;
    const float angle = kKnobAngleMin + fraction * (kKnobAngleMax - kKnobAngleMin);

    const ImU32 body = ImGui::GetColorU32(ImGuiCol_FrameBgHovered);
    const ImU32 track = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 fill = ImGui::GetColorU32(
        (active || ImGui::IsItemHovered()) ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);

    drawList->AddCircleFilled(center, radius, body, 32);

    drawList->PathArcTo(center, radius - 2.0f, kKnobAngleMin, kKnobAngleMax, 32);
    drawList->PathStroke(track, ImDrawFlags_None, 3.0f);

    drawList->PathArcTo(center, radius - 2.0f, kKnobAngleMin, angle, 32);
    drawList->PathStroke(fill, ImDrawFlags_None, 3.0f);

    const ImVec2 pointerStart(center.x + std::cos(angle) * radius * 0.3f,
                               center.y + std::sin(angle) * radius * 0.3f);
    const ImVec2 pointerEnd(center.x + std::cos(angle) * radius * 0.85f,
                             center.y + std::sin(angle) * radius * 0.85f);
    drawList->AddLine(pointerStart, pointerEnd, fill, 2.5f);

    return changed;
}

// Desenha o footswitch de um pedal (o botão redondo e o LED acima dele,
// aceso quando o pedal está ATIVO) centrado em center. Devolve true no
// clique — quem chama decide o que o bypass significa.
bool footswitch(ImVec2 center, bool bypassed)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.65f));

    const float ledRadius = 5.0f;
    const ImVec2 ledCenter(center.x, center.y - kFootswitchRadius - 16.0f);

    if (!bypassed)
    {
        // Um halo suave ao redor, pra parecer que o LED emite luz, não só
        // uma bolinha colorida.
        drawList->AddCircleFilled(ledCenter, ledRadius * 2.4f,
                                   ImGui::GetColorU32(ImVec4(1.0f, 0.25f, 0.1f, 0.20f)), 16);
    }

    const ImU32 ledColor = bypassed
        ? ImGui::GetColorU32(ImVec4(0.22f, 0.07f, 0.05f, 1.0f))
        : ImGui::GetColorU32(ImVec4(1.0f, 0.3f, 0.12f, 1.0f));
    drawList->AddCircleFilled(ledCenter, ledRadius, ledColor, 16);

    ImGui::SetCursorScreenPos(ImVec2(center.x - kFootswitchRadius, center.y - kFootswitchRadius));
    ImGui::InvisibleButton("##footswitch", ImVec2(kFootswitchRadius * 2.0f, kFootswitchRadius * 2.0f));

    const bool clicked = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();

    if (hovered)
        ImGui::SetTooltip(bypassed ? "ligar" : "desligar (bypass)");

    const ImU32 color = ImGui::GetColorU32(
        hovered ? ImVec4(0.38f, 0.38f, 0.42f, 1.0f) : ImVec4(0.24f, 0.24f, 0.27f, 1.0f));

    drawList->AddCircleFilled(center, kFootswitchRadius, color, 24);
    drawList->AddCircle(center, kFootswitchRadius, borderColor, 24, 2.0f);
    drawList->AddCircle(center, kFootswitchRadius * 0.55f,
                         ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.5f)), 20, 1.5f);

    return clicked;
}

// Deriva uma cor estável a partir do NOME do módulo (o mesmo texto que
// AudioModule::name() devolve) — não do tipo concreto. A UI continua sem
// saber o que cada módulo FAZ (ver o comentário no topo de DesktopUI.h);
// isto só dá a cada pedal uma identidade visual distinta e permanente,
// igual à cor de um pedal de verdade, sem precisar de uma tabela
// module->cor pra manter atualizada a cada módulo novo.
ImU32 colorForModuleName(const char* name, bool bypassed)
{
    std::uint32_t hash = 2166136261u;   // FNV-1a

    for (const char* c = name; *c != '\0'; ++c)
    {
        hash ^= static_cast<unsigned char>(*c);
        hash *= 16777619u;
    }

    const float hue = static_cast<float>(hash % 360u) / 360.0f;

    // Em bypass, o pedal fica visualmente "apagado" — menos saturado e
    // mais escuro — o que reforça o LED apagado no footswitch.
    const float saturation = bypassed ? 0.12f : 0.50f;
    const float value = bypassed ? 0.22f : 0.48f;

    float r = 0.0f, g = 0.0f, b = 0.0f;
    ImGui::ColorConvertHSVtoRGB(hue, saturation, value, r, g, b);

    return ImGui::GetColorU32(ImVec4(r, g, b, 1.0f));
}

// Mesma formatação de PedalboardUI::formatValue(): frequência em hertz e
// tempo em milissegundos não precisam de casas decimais; o resto precisa.
// O id do parâmetro é o que diz qual é o caso, não o tipo do módulo.
std::string formatValue(const Parameter& parameter, float value)
{
    char buffer[32];

    if (parameter.id() == "frequency")
        std::snprintf(buffer, sizeof(buffer), "%.0f Hz", static_cast<double>(value));
    else if (parameter.id() == "time")
        std::snprintf(buffer, sizeof(buffer), "%.0f ms", static_cast<double>(value) * 1000.0);
    else
        std::snprintf(buffer, sizeof(buffer), "%.2f", static_cast<double>(value));

    return buffer;
}

// Piso em dB abaixo do qual o sinal já é inaudível na prática — mesmo
// valor e mesmo motivo de PedalboardUI::meter().
constexpr float kFloorDb = -60.0f;

// Converte amplitude linear em decibéis, com o piso acima. O ouvido
// percebe volume de forma logarítmica, não linear — ver o comentário em
// PedalboardUI.h sobre o motivo do medidor ser em dB.
float amplitudeToDb(float peak)
{
    const float db = peak > 0.0f ? 20.0f * std::log10(peak) : kFloorDb;
    return std::max(db, kFloorDb);
}

// Segundos como "mm:ss" — para posição de transporte e tempo de gravação.
std::string formatClock(double seconds)
{
    const int total = std::max(0, static_cast<int>(seconds));
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", total / 60, total % 60);
    return buffer;
}

// "20261006-142530": prefixo de arquivo de gravação. Ordena em ordem
// cronológica e nunca repete entre duas gravações separadas por um
// segundo ou mais.
std::string fileTimestamp()
{
    const std::time_t now = std::time(nullptr);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y%m%d-%H%M%S", std::localtime(&now));
    return buffer;
}

double secondsSince(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

// Botão com a cor de "gravando" — vermelho, como o REC de qualquer gravador.
bool recordButton(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f))
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.12f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.16f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.20f, 0.15f, 1.0f));
    const bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return clicked;
}

// O ponteiro do afinador: uma régua de -50 a +50 cents com o centro
// marcado e uma agulha na posição do desvio. Verde dentro de kInTuneCents,
// âmbar perto, vermelho longe — a mesma leitura de um afinador de pedal.
void centsMeter(float cents, bool valid)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const float width = ImGui::GetContentRegionAvail().x;
    const float height = 22.0f;
    const ImVec2 topLeft = ImGui::GetCursorScreenPos();
    const ImVec2 bottomRight(topLeft.x + width, topLeft.y + height);
    const float centerX = topLeft.x + width * 0.5f;

    drawList->AddRectFilled(topLeft, bottomRight, ImGui::GetColorU32(ImGuiCol_FrameBg), 4.0f);

    // Marcas a cada 10 cents; a do centro mais alta.
    for (int mark = -50; mark <= 50; mark += 10)
    {
        const float x = centerX + (static_cast<float>(mark) / 50.0f) * (width * 0.5f - 4.0f);
        const float inset = (mark == 0) ? 2.0f : 7.0f;
        drawList->AddLine(ImVec2(x, topLeft.y + inset), ImVec2(x, bottomRight.y - inset),
                          ImGui::GetColorU32(ImGuiCol_TextDisabled), mark == 0 ? 2.0f : 1.0f);
    }

    if (valid)
    {
        const float clamped = std::clamp(cents, -50.0f, 50.0f);
        const float x = centerX + (clamped / 50.0f) * (width * 0.5f - 4.0f);
        const float distance = std::fabs(cents);

        ImVec4 color(0.85f, 0.2f, 0.2f, 1.0f);
        if (distance <= kInTuneCents)
            color = ImVec4(0.3f, 0.85f, 0.4f, 1.0f);
        else if (distance <= 15.0f)
            color = kAccentColor;

        drawList->AddRectFilled(ImVec2(x - 3.0f, topLeft.y + 1.0f), ImVec2(x + 3.0f, bottomRight.y - 1.0f),
                                ImGui::GetColorU32(color), 2.0f);
    }

    ImGui::Dummy(ImVec2(width, height));
}

// Tema escuro com um acento âmbar — a cor de LED indicador e de VU meter
// de equipamento de áudio de verdade — no lugar do cinza de fábrica do
// ImGui. Chamado uma vez, logo após CreateContext().
void applyTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;

    // Espaçamento mais folgado que o padrão do ImGui — sliders e checkboxes
    // colados uns nos outros parecem uma lista crua, não uma pedaleira.
    style.WindowPadding = ImVec2(16.0f, 16.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    const ImVec4 background(0.09f, 0.09f, 0.11f, 1.00f);
    const ImVec4 panel(0.16f, 0.16f, 0.19f, 1.00f);
    const ImVec4 panelHovered(0.22f, 0.22f, 0.26f, 1.00f);
    const ImVec4 panelActive(0.27f, 0.27f, 0.32f, 1.00f);
    const ImVec4 accent(0.90f, 0.55f, 0.15f, 1.00f);
    const ImVec4 accentHovered(1.00f, 0.63f, 0.20f, 1.00f);
    const ImVec4 accentActive(1.00f, 0.70f, 0.30f, 1.00f);
    const ImVec4 text(0.92f, 0.92f, 0.94f, 1.00f);
    const ImVec4 textDisabled(0.50f, 0.50f, 0.54f, 1.00f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = text;
    colors[ImGuiCol_TextDisabled] = textDisabled;
    colors[ImGuiCol_WindowBg] = background;
    colors[ImGuiCol_ChildBg] = background;
    colors[ImGuiCol_PopupBg] = panel;
    colors[ImGuiCol_Border] = ImVec4(0.05f, 0.05f, 0.06f, 1.00f);
    colors[ImGuiCol_FrameBg] = panel;
    colors[ImGuiCol_FrameBgHovered] = panelHovered;
    colors[ImGuiCol_FrameBgActive] = panelActive;
    colors[ImGuiCol_TitleBg] = background;
    colors[ImGuiCol_TitleBgActive] = background;
    colors[ImGuiCol_MenuBarBg] = panel;
    colors[ImGuiCol_ScrollbarBg] = background;
    colors[ImGuiCol_ScrollbarGrab] = panel;
    colors[ImGuiCol_ScrollbarGrabHovered] = panelHovered;
    colors[ImGuiCol_ScrollbarGrabActive] = panelActive;
    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = accent;
    colors[ImGuiCol_SliderGrabActive] = accentActive;
    colors[ImGuiCol_Button] = panel;
    colors[ImGuiCol_ButtonHovered] = panelHovered;
    colors[ImGuiCol_ButtonActive] = accentActive;
    colors[ImGuiCol_Header] = panel;
    colors[ImGuiCol_HeaderHovered] = panelHovered;
    colors[ImGuiCol_HeaderActive] = panelActive;
    colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = accent;
    colors[ImGuiCol_SeparatorActive] = accentActive;
    // O medidor sobrescreve esta cor por chamada em drawMeter() (verde,
    // amarelo ou vermelho conforme o nível) — isto aqui é só o padrão.
    colors[ImGuiCol_PlotHistogram] = accent;
    colors[ImGuiCol_PlotHistogramHovered] = accentHovered;
}
}

DesktopUI::DesktopUI(LiveEngine& engine, std::string irsDirectory)
    : m_engine(engine)
    , m_irsDirectory(std::move(irsDirectory))
{
    m_library.scanImpulseResponses(m_irsDirectory);
}

void DesktopUI::adoptRig(const Rig& rig)
{
    m_rig = rig;
    m_rigActive = true;
    m_rigSegments.clear();
    m_expandedSegments.clear();

    std::size_t firstModule = 0;
    for (const GearModel* model : m_library.rigModels(rig))
    {
        std::string label = std::string(gearCategoryName(model->category)) + ": " + model->name;
        if (!model->microphone.empty())
            label += " / " + model->microphone;

        m_rigSegments.push_back({label, model->id, firstModule, model->modules.size()});
        firstModule += model->modules.size();
    }

    // A cadeia não é mais o preset carregado — deixar o nome dele em
    // destaque seria mentir sobre o que está tocando.
    m_currentPresetName.clear();
    m_currentPresetPath.clear();
    m_rigMessage.clear();
}

std::size_t DesktopUI::firstFlatIndexForModule(std::size_t moduleIndex) const
{
    std::size_t flat = 0;
    const ModuleChain& chain = m_engine.chain();

    for (std::size_t i = 0; i < moduleIndex; ++i)
        flat += chain.moduleAt(i).parameterCount();

    return flat;
}

void DesktopUI::captureTargets()
{
    m_targets.clear();

    const ModuleChain& chain = m_engine.chain();

    for (std::size_t i = 0; i < chain.size(); ++i)
    {
        const AudioModule& module = chain.moduleAt(i);

        for (std::size_t p = 0; p < module.parameterCount(); ++p)
            m_targets.push_back(module.parameterAt(p).value());
    }
}

void DesktopUI::captureModuleTargets(std::size_t moduleIndex, std::size_t firstFlatIndex)
{
    const AudioModule& module = m_engine.chain().moduleAt(moduleIndex);

    for (std::size_t p = 0; p < module.parameterCount(); ++p)
        m_targets[firstFlatIndex + p] = module.parameterAt(p).defaultValue();
}

bool DesktopUI::initWindow()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "erro: SDL_Init falhou: " << SDL_GetError() << "\n";
        return false;
    }

    // O tamanho pedido não pode passar da área útil da tela (a que exclui
    // a barra de tarefas): numa tela menor que 900px de altura, uma janela
    // de 900px nasceria parcialmente fora da área visível.
    //
    // A MARGEM COBRE A DECORAÇÃO DA JANELA
    // SDL_CreateWindow recebe o tamanho da ÁREA DE CONTEÚDO, não da janela
    // inteira — a barra de título e as bordas que o Windows desenha em
    // volta disso ainda somam por cima. Sem desconto, pedir uma altura
    // igual à área útil inteira empurra a barra de título pra cima do
    // topo da tela, de onde os botões de minimizar e fechar ficam
    // inacessíveis (foi exatamente o que aconteceu na primeira versão
    // desta janela).
    int windowWidth = kWindowWidth;
    int windowHeight = kWindowHeight;

    SDL_Rect usableBounds{};
    const bool hasUsableBounds = SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usableBounds);

    if (hasUsableBounds)
    {
        windowWidth = std::min(windowWidth, usableBounds.w - 40);
        windowHeight = std::min(windowHeight, usableBounds.h - 80);
    }

    // RESIZABLE: a cadeia inteira pode não caber na altura pedida — melhor
    // deixar a pessoa redimensionar (ou usar a barra de rolagem, que o
    // ImGui já desenha sozinho) do que travar um tamanho fixo.
    m_window = SDL_CreateWindow("IbiFX", windowWidth, windowHeight, SDL_WINDOW_RESIZABLE);

    if (m_window == nullptr)
    {
        std::cerr << "erro: SDL_CreateWindow falhou: " << SDL_GetError() << "\n";
        return false;
    }

    // Posiciona explicitamente perto do canto superior esquerdo da área
    // útil, em vez de confiar na centralização automática do SDL — foi
    // justamente ela que colocou a barra de título fora da tela antes.
    if (hasUsableBounds)
        SDL_SetWindowPosition(m_window, usableBounds.x + 20, usableBounds.y + 40);

    m_renderer = SDL_CreateRenderer(m_window, nullptr);

    if (m_renderer == nullptr)
    {
        std::cerr << "erro: SDL_CreateRenderer falhou: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_SetRenderVSync(m_renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    // Sem ibifx.ini: a janela é fixa, nao ha layout de paineis pra lembrar
    // entre execucoes, e gravar um arquivo por engano no diretorio de
    // trabalho seria uma surpresa desagradavel.
    io.IniFilename = nullptr;

    // 13px (o padrao do ImGui) e pensado pra ferramenta de desenvolvedor
    // compacta, nao pra uma pedaleira que vai ser lida a distancia. A
    // fonte embutida e um bitmap rasterizado neste tamanho, entao ela sai
    // nitida — nao e um upscale borrado de algo menor.
    ImFontConfig fontConfig;
    fontConfig.SizePixels = 19.0f;
    io.Fonts->AddFontDefault(&fontConfig);

    applyTheme();

    ImGui_ImplSDL3_InitForSDLRenderer(m_window, m_renderer);
    ImGui_ImplSDLRenderer3_Init(m_renderer);

    return true;
}

void DesktopUI::shutdownWindow()
{
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (m_renderer != nullptr)
        SDL_DestroyRenderer(m_renderer);

    if (m_window != nullptr)
        SDL_DestroyWindow(m_window);

    SDL_Quit();
}

void DesktopUI::drawMeter(const char* label, float peakLinear) const
{
    const float db = amplitudeToDb(peakLinear);
    const float fraction = std::clamp((db - kFloorDb) / (0.0f - kFloorDb), 0.0f, 1.0f);

    // As mesmas tres faixas da PedalboardUI: verde saudavel, amarelo perto
    // do teto, vermelho onde vai cortar.
    ImVec4 color(0.2f, 0.75f, 0.2f, 1.0f);
    if (db > -3.0f)
        color = ImVec4(0.85f, 0.2f, 0.2f, 1.0f);
    else if (db > -12.0f)
        color = ImVec4(0.85f, 0.75f, 0.1f, 1.0f);

    char overlay[32];
    std::snprintf(overlay, sizeof(overlay), "%.1f dB", db);

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    // Largura negativa = "tudo menos isto": sobra espaço para o rótulo à
    // direita, que com -1 ficava empurrado para fora da janela.
    ImGui::ProgressBar(fraction, ImVec2(-90.0f, 0.0f), overlay);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
}

void DesktopUI::drawPedal(std::size_t moduleIndex, std::size_t firstFlatIndex)
{
    const ModuleChain& chain = m_engine.chain();
    const AudioModule& module = chain.moduleAt(moduleIndex);
    const bool bypassed = chain.isBypassed(moduleIndex);
    const std::size_t paramCount = module.parameterCount();

    // Até 2 knobs por linha — um pedal de 1 ou 2 controles fica numa fileira
    // só; de 3 ou 4, duas fileiras. É o mesmo arranjo de um pedal físico
    // com vários potenciômetros na tampa.
    const int columns = (paramCount <= 1) ? 1 : 2;
    const int rows = (paramCount == 0)
        ? 0
        : static_cast<int>((paramCount + static_cast<std::size_t>(columns) - 1) / static_cast<std::size_t>(columns));

    const float bodyHeight = kPedalHeaderHeight
                            + static_cast<float>(rows) * kPedalKnobRowHeight
                            + kPedalFootswitchAreaHeight;

    // O corpo do pedal é uma CHILD WINDOW, não desenho livre por cima da
    // janela principal. É o jeito robusto de compor vários sub-widgets
    // (nome, knobs, footswitch) posicionados à mão com
    // SetCursorScreenPos(): a child tem seu próprio sistema de
    // coordenadas, então nada que aconteça dentro dela confunde o
    // rastreamento de limites da janela "IbiFX" por fora. Sem isso, o
    // ImGui acusa "Code uses SetCursorPos()/SetCursorScreenPos() to
    // extend window/parent boundaries" — foi exatamente o que a primeira
    // versão desta função, com Dummy() + reposicionamento manual, fazia.
    //
    // Depois do EndChild(), a child inteira vira UM item só pro layout da
    // janela principal — SameLine() funciona normalmente, sem ambiguidade.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);

    ImGui::BeginChild("pedal", ImVec2(kPedalWidth, bodyHeight), ImGuiChildFlags_None,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 bodyMax(origin.x + kPedalWidth, origin.y + bodyHeight);

    const ImU32 bodyColor = colorForModuleName(module.name(), bypassed);
    const ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.65f));

    drawList->AddRectFilled(origin, bodyMax, bodyColor, 10.0f);
    drawList->AddRect(origin, bodyMax, borderColor, 10.0f, 0, 2.0f);

    // Nome do pedal, centralizado no topo — a etiqueta serigrafada.
    const ImVec2 nameSize = ImGui::CalcTextSize(module.name());
    ImGui::SetCursorScreenPos(ImVec2(origin.x + (kPedalWidth - nameSize.x) * 0.5f, origin.y + 8.0f));
    ImGui::TextUnformatted(module.name());

    // Reset discreto no canto — não é um controle de uso constante, só
    // pra voltar ao padrão.
    ImGui::SetCursorScreenPos(ImVec2(bodyMax.x - 30.0f, origin.y + 5.0f));
    if (ImGui::SmallButton("R"))
    {
        m_engine.resetModule(moduleIndex);
        captureModuleTargets(moduleIndex, firstFlatIndex);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("reset");

    // Os knobs, em grade.
    const float gridTop = origin.y + kPedalHeaderHeight;
    const float cellWidth = kPedalWidth / static_cast<float>(columns);

    for (std::size_t p = 0; p < paramCount; ++p)
    {
        const Parameter& parameter = module.parameterAt(p);
        const std::size_t index = firstFlatIndex + p;

        const int column = static_cast<int>(p) % columns;
        const int row = static_cast<int>(p) / columns;

        const float cellCenterX = origin.x + cellWidth * (static_cast<float>(column) + 0.5f);
        const float cellTop = gridTop + static_cast<float>(row) * kPedalKnobRowHeight;

        ImGui::PushID(static_cast<int>(p));

        const ImVec2 labelSize = ImGui::CalcTextSize(parameter.label().c_str());
        ImGui::SetCursorScreenPos(ImVec2(cellCenterX - labelSize.x * 0.5f, cellTop));
        ImGui::TextUnformatted(parameter.label().c_str());

        const float knobTop = cellTop + labelSize.y + 4.0f;
        ImGui::SetCursorScreenPos(ImVec2(cellCenterX - kPedalKnobRadius, knobTop));

        float value = m_targets[index];

        // Desenha o ALVO local, não o parameter.value() lido agora — ver o
        // comentário de m_targets no header.
        if (knob("##knob", &value, parameter.minValue(), parameter.maxValue(), kPedalKnobRadius))
        {
            m_targets[index] = value;
            m_engine.setParameter(moduleIndex, p, value);
        }

        const std::string formatted = formatValue(parameter, m_targets[index]);
        const ImVec2 valueSize = ImGui::CalcTextSize(formatted.c_str());
        ImGui::SetCursorScreenPos(
            ImVec2(cellCenterX - valueSize.x * 0.5f, knobTop + kPedalKnobRadius * 2.0f + 4.0f));
        ImGui::TextUnformatted(formatted.c_str());

        ImGui::PopID();
    }

    // O footswitch: um botão redondo grande, apertável com o mouse do
    // mesmo jeito que se aperta com o pé. É o bypass em si — não um botão
    // a mais pra achar escondido num canto.
    if (footswitch(ImVec2(origin.x + kPedalWidth * 0.5f, bodyMax.y - kPedalPadding - kFootswitchRadius), bypassed))
        m_engine.setBypassed(moduleIndex, !bypassed);

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

void DesktopUI::refreshPresetList()
{
    m_presetFiles.clear();

    std::error_code error;

    if (!std::filesystem::exists(kPresetsDir, error))
        return;

    for (const auto& entry : std::filesystem::directory_iterator(kPresetsDir, error))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".ibifxpreset")
            m_presetFiles.push_back(entry.path().string());
    }

    std::sort(m_presetFiles.begin(), m_presetFiles.end());
}

void DesktopUI::loadPreset(const std::string& path)
{
    try
    {
        const Preset loaded = preset::load(path);

        std::string error;
        if (!replaceChain(loaded, error))
        {
            m_presetMessage = error;
            return;
        }

        adoptPreset(loaded, path);
    }
    catch (const std::exception& error)
    {
        m_presetMessage = std::string("erro: ") + error.what();
    }
}

void DesktopUI::adoptPreset(const Preset& loaded, const std::string& path)
{
    // Se a cadeia veio de um rig, os módulos voltam agrupados em painéis de
    // equipamento, com os knobs como foram salvos (adoptRig() só rotula,
    // não reaplica a receita). Primeiro o rig gravado no arquivo; num
    // preset antigo, sem ele, tenta deduzir pela cadeia.
    Rig rig;
    bool hasRig = false;

    if (!loaded.rig.empty() && m_library.matchesRig(loaded, loaded.rig))
    {
        rig = loaded.rig;
        hasRig = true;
    }
    else
    {
        hasRig = m_library.inferRig(loaded, rig);
    }

    if (hasRig)
    {
        adoptRig(rig);
    }
    else
    {
        m_rigActive = false;
        m_rigSegments.clear();
    }

    m_currentPresetName = loaded.name;
    m_currentPresetPath = path;
    m_presetMessage = "preset carregado: " + loaded.name;
}

bool DesktopUI::replaceChain(const Preset& newPreset, std::string& error)
{
    // Ensaio numa cadeia de rascunho primeiro: se um tipo for desconhecido
    // ou uma IR não carregar, o erro sai aqui, com o motor ainda tocando a
    // cadeia antiga intacta — e não depois de clear(), com ela pela metade.
    try
    {
        ModuleChain rehearsal;
        preset::apply(newPreset, rehearsal);
    }
    catch (const std::exception& e)
    {
        error = std::string("erro: ") + e.what();
        return false;
    }

    // Ver o comentário no header sobre por que parar o motor é necessário
    // aqui: preset::apply() reconstrói a cadeia inteira.
    m_engine.stop();
    preset::apply(newPreset, m_engine.chain());

    if (!m_engine.start(m_mode, m_sampleRate, m_blockSize))
    {
        error = "erro ao religar o motor: " + m_engine.lastError();
        return false;
    }

    captureTargets();
    return true;
}

void DesktopUI::applyRig(const Rig& next)
{
    try
    {
        // buildPreset() lança num id inválido ANTES de replaceChain() parar
        // o motor.
        std::string error;
        if (!replaceChain(m_library.buildPreset(next, "Rig"), error))
        {
            m_rigMessage = error;
            return;
        }

        adoptRig(next);
    }
    catch (const std::exception& e)
    {
        m_rigMessage = std::string("erro: ") + e.what();
    }
}

void DesktopUI::drawGearBrowser()
{
    drawSectionHeader("Equipamentos");

    // As abas, na ordem do sinal — a mesma do rig.
    constexpr GearCategory kTabs[] = {GearCategory::Stomp, GearCategory::Amp,
                                      GearCategory::Cabinet, GearCategory::Rack};
    constexpr const char* kTabLabels[] = {"Stomp", "Amp", "Cab", "Rack"};

    for (int t = 0; t < 4; ++t)
    {
        if (t > 0)
            ImGui::SameLine();

        const bool selected = (kTabs[t] == m_browserCategory);
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, kAccentColor);

        if (ImGui::Button(kTabLabels[t], ImVec2(68.0f, 0.0f)))
        {
            m_browserCategory = kTabs[t];
            m_browserCharacter.clear();
        }

        if (selected)
            ImGui::PopStyleColor();
    }

    const std::vector<const GearModel*> models = m_library.modelsIn(m_browserCategory);

    // Filtro por character ("Overdrive", "Clean"...), na ordem em que
    // aparecem no catálogo. Cabinets não têm: lá o agrupamento é por
    // gabinete.
    if (m_browserCategory != GearCategory::Cabinet)
    {
        std::vector<std::string> characters;
        for (const GearModel* model : models)
        {
            if (std::find(characters.begin(), characters.end(), model->character) == characters.end())
                characters.push_back(model->character);
        }

        ImGui::Spacing();
        if (ImGui::RadioButton("Todos", m_browserCharacter.empty()))
            m_browserCharacter.clear();

        for (const std::string& character : characters)
        {
            ImGui::SameLine();
            if (ImGui::GetContentRegionAvail().x < ImGui::CalcTextSize(character.c_str()).x + 40.0f)
                ImGui::NewLine();

            if (ImGui::RadioButton(character.c_str(), m_browserCharacter == character))
                m_browserCharacter = character;
        }
    }

    ImGui::Separator();

    const float footerHeight = ImGui::GetFrameHeightWithSpacing() + 4.0f;
    ImGui::BeginChild("##gearList", ImVec2(0.0f, -footerHeight));

    // Amp e cabinet são um só por rig — "nenhum" é uma escolha válida.
    if (m_browserCategory == GearCategory::Amp || m_browserCategory == GearCategory::Cabinet)
    {
        const bool isAmp = (m_browserCategory == GearCategory::Amp);
        const std::string& current = isAmp ? m_rig.amp : m_rig.cabinet;

        if (ImGui::Selectable("(nenhum)", m_rigActive && current.empty()))
        {
            Rig next = m_rig;
            (isAmp ? next.amp : next.cabinet).clear();
            applyRig(next);
        }
    }

    if (models.empty() && m_browserCategory == GearCategory::Cabinet)
    {
        ImGui::Spacing();
        ImGui::TextWrapped("Nenhum cabinet encontrado. Clique em \"Importar IR...\" "
                           "(ou arraste um .wav para a janela), ou coloque IRs em "
                           "irs/<gabinete>/<microfone>.wav e clique em \"Reler irs/\".");
    }

    const std::string* lastCabinetName = nullptr;

    for (const GearModel* model : models)
    {
        if (!m_browserCharacter.empty() && model->character != m_browserCharacter)
            continue;

        // Cabinets agrupados: o nome do gabinete uma vez, os microfones
        // embaixo dele.
        if (model->category == GearCategory::Cabinet
            && (lastCabinetName == nullptr || *lastCabinetName != model->name))
        {
            ImGui::Spacing();
            ImGui::TextDisabled("%s", model->name.c_str());
            lastCabinetName = &model->name;
        }

        bool selected = false;
        switch (model->category)
        {
        case GearCategory::Amp: selected = (m_rig.amp == model->id); break;
        case GearCategory::Cabinet: selected = (m_rig.cabinet == model->id); break;
        case GearCategory::Stomp:
            selected = std::find(m_rig.stomps.begin(), m_rig.stomps.end(), model->id) != m_rig.stomps.end();
            break;
        case GearCategory::Rack:
            selected = std::find(m_rig.rack.begin(), m_rig.rack.end(), model->id) != m_rig.rack.end();
            break;
        }
        selected = selected && m_rigActive;

        ImGui::PushID(model->id.c_str());

        // Uma amostra da cor do equipamento — a mesma família de cor que o
        // pedal dele vai ter no pedalboard.
        const ImVec4 swatch = ImGui::ColorConvertU32ToFloat4(colorForModuleName(model->name.c_str(), false));
        ImGui::ColorButton("##swatch", swatch, ImGuiColorEditFlags_NoTooltip, ImVec2(16.0f, 16.0f));
        ImGui::SameLine();

        const bool isCabinet = (model->category == GearCategory::Cabinet);
        const std::string label = isCabinet ? model->microphone : model->name;

        // Cabinets ganham um "x" no fim da linha para excluir a IR — o
        // Selectable encolhe para deixar espaço para ele.
        const float deleteWidth = isCabinet ? ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x : 0.0f;

        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_None,
                              ImVec2(ImGui::GetContentRegionAvail().x - deleteWidth, 0.0f)))
        {
            Rig next = m_rig;
            switch (model->category)
            {
            case GearCategory::Amp: next.amp = model->id; break;
            case GearCategory::Cabinet: next.cabinet = model->id; break;
            case GearCategory::Stomp: next.stomps.push_back(model->id); break;
            case GearCategory::Rack: next.rack.push_back(model->id); break;
            }
            applyRig(next);
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\n%s", model->character.c_str(), model->description.c_str());

        if (isCabinet)
        {
            ImGui::SameLine();

            if (ImGui::SmallButton("x"))
                m_irPendingDelete = model->id;

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("excluir esta IR");
        }

        ImGui::PopID();
    }

    ImGui::EndChild();

    // O popup é aberto aqui fora, no mesmo nível de ID do BeginPopupModal
    // abaixo — de dentro do PushID da linha, o ImGui não o acharia.
    if (!m_irPendingDelete.empty() && !ImGui::IsPopupOpen("Excluir IR"))
        ImGui::OpenPopup("Excluir IR");

    if (ImGui::BeginPopupModal("Excluir IR", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const GearModel* doomed = m_library.find(m_irPendingDelete);

        if (doomed != nullptr)
        {
            ImGui::Text("Excluir \"%s\" (%s)?", doomed->microphone.c_str(), doomed->name.c_str());
            ImGui::TextDisabled("O arquivo .wav sera apagado do disco.");
        }

        ImGui::Spacing();

        if (recordButton("Excluir", ImVec2(110.0f, 0.0f)))
        {
            deleteImpulseResponse(m_irPendingDelete);
            m_irPendingDelete.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancelar", ImVec2(110.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            m_irPendingDelete.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (ImGui::Button("Importar IR..."))
        openImpulseResponseDialog();

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("escolhe um .wav de IR (ou arraste o arquivo para a janela)");

    ImGui::SameLine();

    if (ImGui::Button("Reler irs/"))
    {
        const std::size_t found = m_library.scanImpulseResponses(m_irsDirectory);
        m_rigMessage = std::to_string(found) + " cabinet(s) em " + m_irsDirectory;
    }

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("procura cabinets de novo em irs/");
}

void DesktopUI::drawRigStrip()
{
    drawSectionHeader("Rig");

    if (!m_rigActive)
    {
        ImGui::TextWrapped("A cadeia atual veio das flags ou de um preset. Escolha "
                           "um equipamento no painel da direita para montar um rig.");
    }
    else
    {
        // As ações só são registradas durante o desenho e aplicadas no fim:
        // mexer no vetor enquanto ele é percorrido invalidaria o laço.
        Rig next = m_rig;
        bool changed = false;

        auto drawList = [&](const char* title, std::vector<std::string>& ids) {
            ImGui::TextDisabled("%s", title);

            if (ids.empty())
            {
                ImGui::SameLine();
                ImGui::TextDisabled("-");
            }

            for (std::size_t k = 0; k < ids.size() && !changed; ++k)
            {
                const GearModel* model = m_library.find(ids[k]);
                const std::string name = (model != nullptr) ? model->name : ids[k];

                ImGui::SameLine();
                if (ImGui::GetContentRegionAvail().x < ImGui::CalcTextSize(name.c_str()).x + 80.0f)
                    ImGui::NewLine();

                ImGui::PushID(title);
                ImGui::PushID(static_cast<int>(k));

                if (k > 0)
                {
                    if (ImGui::SmallButton("<"))
                    {
                        std::swap(ids[k], ids[k - 1]);
                        changed = true;
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("mover para antes");
                    ImGui::SameLine();
                }

                ImGui::TextUnformatted(name.c_str());
                ImGui::SameLine();

                if (!changed && ImGui::SmallButton("x"))
                {
                    ids.erase(ids.begin() + static_cast<std::ptrdiff_t>(k));
                    changed = true;
                }

                ImGui::PopID();
                ImGui::PopID();
            }
        };

        auto drawSingle = [&](const char* title, std::string& id) {
            ImGui::TextDisabled("%s", title);
            ImGui::SameLine();

            const GearModel* model = m_library.find(id);
            if (model == nullptr)
            {
                ImGui::TextDisabled("-");
                return;
            }

            std::string name = model->name;
            if (!model->microphone.empty())
                name += " / " + model->microphone;

            ImGui::TextUnformatted(name.c_str());
            ImGui::SameLine();

            ImGui::PushID(title);
            if (ImGui::SmallButton("x"))
            {
                id.clear();
                changed = true;
            }
            ImGui::PopID();
        };

        drawList("Stomps:", next.stomps);
        drawSingle("Amp:   ", next.amp);
        drawSingle("Cab:   ", next.cabinet);
        drawList("Rack:  ", next.rack);

        if (changed)
            applyRig(next);
    }

    if (!m_rigMessage.empty())
    {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_rigMessage.c_str());
    }
}

void DesktopUI::drawPedalboard()
{
    const ModuleChain& chain = m_engine.chain();

    // Sem rig ativo, a cadeia inteira é um grupo só, sem título. Com rig, um
    // grupo por equipamento, e o que sobrar no fim (o Limiter) vira "Saida".
    std::vector<RigSegment> groups;
    if (m_rigActive)
    {
        groups = m_rigSegments;

        const std::size_t covered = groups.empty() ? 0 : groups.back().firstModule + groups.back().moduleCount;
        if (covered < chain.size())
            groups.push_back({"Saida", "", covered, chain.size() - covered});
    }
    else
    {
        groups.push_back({"", "", 0, chain.size()});
    }

    // Painéis da mesma fileira (stomps; amp + cab; rack) ficam lado a lado,
    // quebrando linha quando não cabem. Um grupo aberto "por dentro" e a
    // saída começam linha própria, com título.
    int previousRow = -1;

    for (std::size_t g = 0; g < groups.size(); ++g)
    {
        const RigSegment& group = groups[g];
        const GearModel* model = group.gearId.empty() ? nullptr : m_library.find(group.gearId);
        const bool asPanel = model != nullptr && !model->controls.empty()
                             && m_expandedSegments.count(g) == 0;

        if (asPanel)
        {
            const int row = rowFor(model->category);

            if (row == previousRow)
            {
                ImGui::SameLine();
                if (ImGui::GetContentRegionAvail().x < gearPanelWidth(*model))
                    ImGui::NewLine();
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, kAccentColor);
                ImGui::TextUnformatted(rowTitle(row));
                ImGui::PopStyleColor();
            }

            ImGui::PushID(static_cast<int>(g));
            drawGearPanel(g, *model);
            ImGui::PopID();

            previousRow = row;
            continue;
        }

        previousRow = -1;

        if (!group.label.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, kAccentColor);
            ImGui::TextUnformatted(group.label.c_str());
            ImGui::PopStyleColor();

            // Um equipamento aberto "por dentro" pode voltar ao painel.
            if (model != nullptr && !model->controls.empty())
            {
                ImGui::SameLine();
                ImGui::PushID(static_cast<int>(g));
                if (ImGui::SmallButton("fechar"))
                    m_expandedSegments.erase(g);
                ImGui::PopID();
            }
        }

        // Lado a lado, como um pedalboard de verdade — quebrando pra
        // próxima linha quando não cabe mais nenhum na largura. É o mesmo
        // idioma de quebra que o próprio demo do ImGui usa pra botões.
        for (std::size_t k = 0; k < group.moduleCount; ++k)
        {
            const std::size_t i = group.firstModule + k;
            if (i >= chain.size())
                break;

            // Escopa os ids de todos os widgets deste pedal (knobs, reset,
            // footswitch), pra dois módulos do mesmo tipo (dois Gain, por
            // exemplo) não colidirem só por terem o mesmo name().
            ImGui::PushID(static_cast<int>(i));
            drawPedal(i, firstFlatIndexForModule(i));
            ImGui::PopID();

            if (k + 1 < group.moduleCount)
            {
                ImGui::SameLine();

                if (ImGui::GetContentRegionAvail().x < kPedalWidth)
                    ImGui::NewLine();
            }
        }

        ImGui::Spacing();
    }
}

bool DesktopUI::flatIndexFor(std::size_t moduleIndex, const std::string& parameterId,
                             std::size_t& parameterIndex, std::size_t& flatIndex) const
{
    const ModuleChain& chain = m_engine.chain();
    if (moduleIndex >= chain.size())
        return false;

    const AudioModule& module = chain.moduleAt(moduleIndex);
    for (std::size_t p = 0; p < module.parameterCount(); ++p)
    {
        if (module.parameterAt(p).id() == parameterId)
        {
            parameterIndex = p;
            flatIndex = firstFlatIndexForModule(moduleIndex) + p;
            return true;
        }
    }
    return false;
}

void DesktopUI::drawGearPanel(std::size_t segmentIndex, const GearModel& model)
{
    const RigSegment& segment = m_rigSegments[segmentIndex];
    const ModuleChain& chain = m_engine.chain();

    const bool isAmp = model.category == GearCategory::Amp;
    const bool bypassed = chain.isBypassed(segment.firstModule);
    const std::size_t count = model.controls.size();

    // Ampli: uma fileira de knobs larga. Pedal: grade de 2 colunas, como
    // drawPedal().
    const int columns = isAmp ? static_cast<int>(count) : (count <= 1 ? 1 : 2);
    const int rows = static_cast<int>((count + static_cast<std::size_t>(columns) - 1) / static_cast<std::size_t>(columns));
    const float knobRadius = isAmp ? kAmpKnobRadius : kPedalKnobRadius;
    const float cellWidth = isAmp ? kAmpKnobCellWidth : kPedalWidth / static_cast<float>(columns);

    const float headerHeight = isAmp ? 52.0f : (model.microphone.empty() ? kPedalHeaderHeight : kPedalHeaderHeight + 24.0f);
    const float width = gearPanelWidth(model);
    const float knobsHeight = static_cast<float>(rows) * (kPedalKnobRowHeight + (isAmp ? 8.0f : 0.0f));
    const float footerHeight = isAmp ? 44.0f : kPedalFootswitchAreaHeight;
    const float height = headerHeight + knobsHeight + footerHeight;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("gear", ImVec2(width, height), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 bodyMax(origin.x + width, origin.y + height);
    const ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.65f));

    // Onde ficam os knobs: no ampli, uma placa dourada sobre o corpo
    // preto (o "tolex"); no pedal, o próprio corpo colorido.
    const ImVec2 plateMin(origin.x + kPedalPadding * 0.5f, origin.y + headerHeight - 6.0f);
    const ImVec2 plateMax(bodyMax.x - kPedalPadding * 0.5f, origin.y + headerHeight + knobsHeight);

    if (isAmp)
    {
        const float dim = bypassed ? 0.45f : 1.0f;
        drawList->AddRectFilled(origin, bodyMax, ImGui::GetColorU32(ImVec4(0.07f, 0.07f, 0.08f, 1.0f)), 10.0f);
        drawList->AddRectFilled(plateMin, plateMax,
                                ImGui::GetColorU32(ImVec4(0.74f * dim, 0.62f * dim, 0.38f * dim, 1.0f)), 4.0f);
        drawList->AddRect(plateMin, plateMax, borderColor, 4.0f, 0, 1.5f);
    }
    else
    {
        drawList->AddRectFilled(origin, bodyMax, colorForModuleName(model.name.c_str(), bypassed), 10.0f);
    }
    drawList->AddRect(origin, bodyMax, borderColor, 10.0f, 0, 2.0f);

    // O nome serigrafado no topo — maior no ampli, como o logo na grade. O
    // cabinet mostra o microfone embaixo, que é o que diferencia duas IRs
    // do mesmo gabinete.
    ImGui::SetWindowFontScale(isAmp ? 1.45f : 1.0f);
    const ImVec2 nameSize = ImGui::CalcTextSize(model.name.c_str());
    ImGui::SetCursorScreenPos(ImVec2(origin.x + (width - nameSize.x) * 0.5f, origin.y + 8.0f));
    ImGui::TextUnformatted(model.name.c_str());
    ImGui::SetWindowFontScale(1.0f);

    if (!model.microphone.empty())
    {
        const ImVec2 micSize = ImGui::CalcTextSize(model.microphone.c_str());
        ImGui::SetCursorScreenPos(ImVec2(origin.x + (width - micSize.x) * 0.5f, origin.y + kPedalHeaderHeight));
        ImGui::TextDisabled("%s", model.microphone.c_str());
    }

    // Os dois botões de canto: abrir por dentro e voltar aos valores da
    // receita. O reset NÃO é resetModule(): isso devolveria cada módulo ao
    // padrão DELE, e não ao da receita deste equipamento. No ampli ficam no
    // topo; no pedal, embaixo, ao lado do footswitch — no topo de um corpo
    // estreito eles cobririam o nome.
    const float buttonsY = isAmp ? origin.y + 5.0f : bodyMax.y - 30.0f;
    ImGui::SetCursorScreenPos(ImVec2(origin.x + 6.0f, buttonsY));
    if (ImGui::SmallButton("+"))
        m_expandedSegments.insert(segmentIndex);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("ver os modulos por dentro");

    ImGui::SetCursorScreenPos(ImVec2(bodyMax.x - 30.0f, buttonsY));
    if (ImGui::SmallButton("R"))
    {
        for (std::size_t m = 0; m < model.modules.size(); ++m)
        {
            for (const auto& [id, value] : model.modules[m].parameters)
            {
                std::size_t parameterIndex = 0, flatIndex = 0;
                if (flatIndexFor(segment.firstModule + m, id, parameterIndex, flatIndex))
                {
                    m_targets[flatIndex] = value;
                    m_engine.setParameter(segment.firstModule + m, parameterIndex, value);
                }
            }
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("voltar aos valores do equipamento");

    // Texto escuro sobre a placa dourada do ampli.
    if (isAmp)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.10f, 0.08f, 0.05f, 1.0f));

    const float gridLeft = isAmp ? origin.x + kPedalPadding : origin.x;

    for (std::size_t c = 0; c < count; ++c)
    {
        const GearControl& control = model.controls[c];
        const std::size_t moduleIndex = segment.firstModule + control.moduleOffset;

        std::size_t parameterIndex = 0, flatIndex = 0;
        if (!flatIndexFor(moduleIndex, control.parameterId, parameterIndex, flatIndex))
            continue;

        const int column = static_cast<int>(c) % columns;
        const int row = static_cast<int>(c) / columns;
        const float cellCenterX = gridLeft + cellWidth * (static_cast<float>(column) + 0.5f);
        const float cellTop = origin.y + headerHeight + static_cast<float>(row) * kPedalKnobRowHeight;

        ImGui::PushID(static_cast<int>(c));

        const ImVec2 labelSize = ImGui::CalcTextSize(control.label.c_str());
        ImGui::SetCursorScreenPos(ImVec2(cellCenterX - labelSize.x * 0.5f, cellTop));
        ImGui::TextUnformatted(control.label.c_str());

        const float knobTop = cellTop + labelSize.y + 4.0f;
        ImGui::SetCursorScreenPos(ImVec2(cellCenterX - knobRadius, knobTop));

        float value = std::clamp(m_targets[flatIndex], control.minValue, control.maxValue);
        if (knob("##knob", &value, control.minValue, control.maxValue, knobRadius))
        {
            m_targets[flatIndex] = value;
            m_engine.setParameter(moduleIndex, parameterIndex, value);
        }

        // Escala de 0 a 10, como a serigrafia de um ampli de verdade — o
        // valor interno (drive 2.8, ganho 0.2...) não diz nada a quem toca.
        char scale[16];
        std::snprintf(scale, sizeof(scale), "%.1f",
                      static_cast<double>(10.0f * (value - control.minValue) / (control.maxValue - control.minValue)));
        const ImVec2 scaleSize = ImGui::CalcTextSize(scale);
        ImGui::SetCursorScreenPos(ImVec2(cellCenterX - scaleSize.x * 0.5f, knobTop + knobRadius * 2.0f + 4.0f));
        ImGui::TextUnformatted(scale);

        ImGui::PopID();
    }

    if (isAmp)
        ImGui::PopStyleColor();

    // Liga/desliga o equipamento inteiro: todos os módulos da receita
    // juntos, senão o "pedal" ficaria meio ligado.
    bool toggle = false;
    if (isAmp)
    {
        ImGui::SetCursorScreenPos(ImVec2(bodyMax.x - 74.0f, bodyMax.y - 36.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, bypassed ? ImVec4(0.25f, 0.25f, 0.28f, 1.0f)
                                                        : ImVec4(0.75f, 0.12f, 0.08f, 1.0f));
        toggle = ImGui::Button(bypassed ? "OFF" : "ON", ImVec2(60.0f, 0.0f));
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(origin.x + kPedalPadding, bodyMax.y - 32.0f));
        ImGui::TextDisabled("%s", model.character.c_str());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", model.description.c_str());
    }
    else
    {
        toggle = footswitch(ImVec2(origin.x + width * 0.5f, bodyMax.y - kPedalPadding - kFootswitchRadius), bypassed);
    }

    if (toggle)
    {
        for (std::size_t m = 0; m < segment.moduleCount; ++m)
            m_engine.setBypassed(segment.firstModule + m, !bypassed);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void DesktopUI::savePreset(const std::string& name)
{
    try
    {
        std::filesystem::create_directories(kPresetsDir);

        const std::string path = std::string(kPresetsDir) + "/" + name + ".ibifxpreset";

        Preset captured = preset::capture(name, m_engine.chain());

        // Com um rig ativo, o preset leva junto de qual rig a cadeia veio —
        // é o que deixa loadPreset() reagrupar os painéis depois.
        if (m_rigActive)
            captured.rig = m_rig;

        preset::save(captured, path);

        m_currentPresetName = name;
        m_currentPresetPath = path;
        m_presetMessage = "preset salvo: " + path;
        refreshPresetList();
    }
    catch (const std::exception& error)
    {
        m_presetMessage = std::string("erro: ") + error.what();
    }
}

void DesktopUI::drawSectionHeader(const char* label) const
{
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, kAccentColor);
    ImGui::SetWindowFontScale(1.08f);
    ImGui::TextUnformatted(label);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    // Uma linha fina na cor de destaque, só embaixo do título — não a
    // largura inteira da janela, pra marcar "isto é um cabeçalho", não
    // "isto corta a janela em duas metades" (esse já é o papel do
    // Separator() ao redor de cada cartão).
    const ImVec2 textEnd = ImGui::GetItemRectMax();
    const ImVec2 textStart = ImGui::GetItemRectMin();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(textStart.x, textEnd.y + 2.0f), ImVec2(textStart.x + 120.0f, textEnd.y + 2.0f),
        ImGui::GetColorU32(kAccentColor), 2.0f);

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
}

void DesktopUI::drawPresetPanel()
{
    drawSectionHeader("Presets");

    if (!m_currentPresetName.empty())
    {
        ImGui::TextDisabled("atual:");
        ImGui::SameLine();
        ImGui::TextUnformatted(m_currentPresetName.c_str());
    }
    else
    {
        ImGui::TextDisabled("nenhum preset carregado ainda");
    }

    ImGui::Spacing();

    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##presetName", "nome para salvar...", m_presetNameBuffer, sizeof(m_presetNameBuffer));

    ImGui::SameLine();

    // Nome vazio não vira arquivo "presets/.ibifxpreset" sem identidade
    // nenhuma — o botão só funciona com algo digitado.
    const bool canSave = m_presetNameBuffer[0] != '\0';

    ImGui::BeginDisabled(!canSave);
    if (ImGui::Button("Salvar"))
        savePreset(m_presetNameBuffer);
    ImGui::EndDisabled();

    ImGui::SameLine();

    if (ImGui::Button("Atualizar lista"))
        refreshPresetList();

    ImGui::Spacing();

    // A lista fica dentro de uma child com fundo próprio e borda — um
    // "cartão" de verdade, não só texto solto flutuando na janela. Altura
    // fixa pequena: com muitos presets, ela ganha barra de rolagem
    // própria em vez de empurrar o resto da janela pra baixo.
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::BeginChild("##presetList", ImVec2(0.0f, 92.0f), ImGuiChildFlags_Borders);

    if (m_presetFiles.empty())
    {
        ImGui::TextDisabled("(nenhum preset em presets/ ainda)");
    }
    else
    {
        for (const std::string& path : m_presetFiles)
        {
            const std::string label = std::filesystem::path(path).stem().string();
            const bool isCurrent = (path == m_currentPresetPath);

            // O preset em uso fica marcado, não só mais uma linha igual
            // às outras — Selected força o destaque mesmo sem estar sob o
            // mouse.
            if (ImGui::Selectable(label.c_str(), isCurrent))
                loadPreset(path);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    if (!m_presetMessage.empty())
    {
        ImGui::Spacing();
        ImGui::TextUnformatted(m_presetMessage.c_str());
    }
}

void DesktopUI::refreshBackingTrackList()
{
    m_backingFiles.clear();

    std::error_code error;

    if (!std::filesystem::exists(kAudioDir, error))
        return;

    for (const auto& entry : std::filesystem::directory_iterator(kAudioDir, error))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".wav")
            m_backingFiles.push_back(entry.path().string());
    }

    std::sort(m_backingFiles.begin(), m_backingFiles.end());
}

void DesktopUI::loadBackingTrack(const std::string& path)
{
    // Ver o comentário no header: o vetor de amostras da backing track não
    // pode ser trocado com a thread de áudio lendo dele.
    m_engine.stop();

    try
    {
        m_engine.practiceSession().loadBackingTrack(path);
        m_backingTrackPath = path;
        m_practiceMessage = "backing track: " + std::filesystem::path(path).filename().string();
    }
    catch (const std::exception& error)
    {
        m_practiceMessage = std::string("erro: ") + error.what();
    }

    // Religa mesmo se o load falhou — a backing track antiga (ou nenhuma)
    // continua valendo, e o pedalboard não pode ficar mudo por causa disso.
    if (!m_engine.start(m_mode, m_sampleRate, m_blockSize))
        m_practiceMessage = "erro ao religar o motor: " + m_engine.lastError();
}

void DesktopUI::importImpulseResponse(const std::string& sourcePath)
{
    namespace fs = std::filesystem;

    try
    {
        // O SDL entrega caminhos em UTF-8; montar o path a partir de char8_t
        // evita que um acento no nome do arquivo vire lixo no Windows.
        const fs::path source(reinterpret_cast<const char8_t*>(sourcePath.c_str()));

        std::string extension = source.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (extension != ".wav")
        {
            m_rigMessage = "erro: a IR precisa ser um arquivo .wav";
            return;
        }

        // COPIA EM VEZ DE APONTAR PARA O ORIGINAL
        // O catálogo só conhece o que está em irs/ (ver GearLibrary.h), e um
        // preset salvo guarda o caminho da IR — se ele apontasse para a pasta
        // de Downloads, mover o arquivo de lá quebraria o preset sem aviso.
        const fs::path destinationDir = fs::path(m_irsDirectory) / "Importados";
        const fs::path destination = destinationDir / source.filename();

        fs::create_directories(destinationDir);

        std::error_code sameFile;
        if (!fs::equivalent(source, destination, sameFile))
            fs::copy_file(source, destination, fs::copy_options::overwrite_existing);

        m_library.scanImpulseResponses(m_irsDirectory);

        // Acha o cabinet recém-criado pelo caminho da IR, em vez de recalcular
        // o id aqui — a regra de montar o id é da GearLibrary.
        const GearModel* imported = nullptr;
        for (const GearModel* model : m_library.modelsIn(GearCategory::Cabinet))
        {
            if (!model->modules.empty() && fs::path(model->modules[0].irPath) == destination)
                imported = model;
        }

        if (imported == nullptr)
        {
            m_rigMessage = "erro: a IR foi copiada, mas nao apareceu no catalogo";
            return;
        }

        m_browserCategory = GearCategory::Cabinet;

        Rig next = m_rig;
        next.cabinet = imported->id;
        applyRig(next);

        // applyRig() só muda o rig se a IR carregou (sample rate certo etc.);
        // se não, ele já deixou o erro em m_rigMessage.
        if (m_rig.cabinet == imported->id)
            m_rigMessage = "IR importada: " + destination.filename().string();
    }
    catch (const std::exception& error)
    {
        m_rigMessage = std::string("erro ao importar IR: ") + error.what();
    }
}

void DesktopUI::deleteImpulseResponse(const std::string& cabinetId)
{
    namespace fs = std::filesystem;

    const GearModel* model = m_library.find(cabinetId);

    if (model == nullptr || model->category != GearCategory::Cabinet || model->modules.empty())
    {
        m_rigMessage = "erro: cabinet nao encontrado";
        return;
    }

    // Copiados antes do rescan, que destrói o GearModel apontado por model.
    const fs::path irPath = model->modules[0].irPath;
    const std::string label = model->microphone;

    // Se é o cabinet em uso, sai do rig primeiro. O som não depende do
    // arquivo (a IR já está na memória do Cabinet), mas o rig não pode
    // continuar apontando para um id que vai deixar de existir.
    if (m_rig.cabinet == cabinetId)
    {
        Rig next = m_rig;
        next.cabinet.clear();

        if (m_rigActive)
            applyRig(next);
        else
            m_rig = next;
    }

    try
    {
        fs::remove(irPath);

        // Só apaga a pasta do gabinete se ela ficou vazia — fs::remove()
        // não apaga pasta com conteúdo, mas conferir antes deixa a
        // intenção explícita.
        const fs::path cabinetDir = irPath.parent_path();
        std::error_code error;
        if (fs::is_empty(cabinetDir, error))
            fs::remove(cabinetDir, error);

        m_rigMessage = "IR excluida: " + label;
    }
    catch (const std::exception& error)
    {
        m_rigMessage = std::string("erro ao excluir IR: ") + error.what();
    }

    m_library.scanImpulseResponses(m_irsDirectory);
}

void DesktopUI::openImpulseResponseDialog()
{
    // Precisa continuar valendo até a resposta chegar — por isso static.
    static const SDL_DialogFileFilter kFilters[] = {{"Impulse response (.wav)", "wav"}};

    SDL_ShowOpenFileDialog(&DesktopUI::onImpulseResponseChosen, this, m_window, kFilters, 1, nullptr, false);
}

void DesktopUI::onImpulseResponseChosen(void* userdata, const char* const* files, int /*filter*/)
{
    // files == nullptr: erro; files[0] == nullptr: a pessoa cancelou.
    if (files == nullptr || files[0] == nullptr)
        return;

    auto* self = static_cast<DesktopUI*>(userdata);
    const std::lock_guard<std::mutex> lock(self->m_pendingIrMutex);
    self->m_pendingIrPath = files[0];
}

void DesktopUI::stopAllRecordings()
{
    if (m_engine.reampRecorder().isRecording())
        m_engine.reampRecorder().stop();

    if (m_engine.practiceSession().isRecording())
        m_engine.practiceSession().stopRecording();
}

void DesktopUI::drawTunerPanel()
{
    drawSectionHeader("Afinador");

    if (ImGui::Checkbox("ligado", &m_tunerOn))
        m_engine.setTunerEnabled(m_tunerOn);

    ImGui::SameLine();

    if (ImGui::Checkbox("mudo", &m_tunerMuted))
        m_engine.setTunerMuted(m_tunerMuted);

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("silencia a saida enquanto o afinador esta ligado");

    ImGui::Spacing();

    const Tuner& tuner = m_engine.tuner();
    const bool valid = m_tunerOn && tuner.isValid();
    const float cents = valid ? tuner.centsOff() : 0.0f;

    // A nota grande, na cor do ponteiro quando afinada — dá para ler de
    // longe, com a guitarra no colo.
    const std::string note = valid ? tuner.noteName() : std::string("--");
    const bool inTune = valid && std::fabs(cents) <= kInTuneCents;

    ImGui::SetWindowFontScale(2.6f);
    const float noteWidth = ImGui::CalcTextSize(note.c_str()).x;
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - noteWidth) * 0.5f + ImGui::GetCursorPosX());

    if (!m_tunerOn)
        ImGui::TextDisabled("%s", note.c_str());
    else if (inTune)
        ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.4f, 1.0f), "%s", note.c_str());
    else
        ImGui::TextUnformatted(note.c_str());

    ImGui::SetWindowFontScale(1.0f);

    centsMeter(cents, valid);

    if (!m_tunerOn)
        ImGui::TextDisabled("desligado");
    else if (!valid)
        ImGui::TextDisabled("toque uma corda...");
    else
        ImGui::Text("%.1f Hz   %+.0f cents", static_cast<double>(tuner.frequencyHz()), static_cast<double>(cents));
}

void DesktopUI::drawRecorderPanel()
{
    drawSectionHeader("Gravador");

    ImGui::TextDisabled("DI (seco) + processado");

    ReampRecorder& recorder = m_engine.reampRecorder();
    const bool recording = recorder.isRecording();

    ImGui::Spacing();

    if (!recording)
    {
        if (recordButton("Gravar", ImVec2(-1.0f, 34.0f)))
        {
            try
            {
                std::filesystem::create_directories(kRecordingsDir);

                const std::string base = std::string(kRecordingsDir) + "/" + fileTimestamp();
                recorder.start(base + "-di.wav", base + "-processado.wav");

                m_reampStartedAt = std::chrono::steady_clock::now();
                m_reampMessage.clear();
            }
            catch (const std::exception& error)
            {
                m_reampMessage = std::string("erro: ") + error.what();
            }
        }
    }
    else
    {
        if (ImGui::Button("Parar", ImVec2(-1.0f, 34.0f)))
        {
            // stop() espera a thread de disco terminar de escrever os dois
            // arquivos — um engasgo curto na janela, nunca no áudio.
            recorder.stop();
            m_reampMessage = "salvo em " + std::string(kRecordingsDir) + "/";
        }
    }

    ImGui::Spacing();

    if (recording)
    {
        ImGui::TextColored(ImVec4(0.95f, 0.25f, 0.2f, 1.0f), "REC");
        ImGui::SameLine();
        ImGui::TextUnformatted(formatClock(secondsSince(m_reampStartedAt)).c_str());
    }
    else
    {
        ImGui::TextDisabled("parado");
    }

    if (!m_reampMessage.empty())
        ImGui::TextWrapped("%s", m_reampMessage.c_str());
}

void DesktopUI::drawPracticePanel()
{
    drawSectionHeader("Pratica");

    PracticeSession& session = m_engine.practiceSession();
    const double rate = m_engine.sampleRate() > 0.0 ? m_engine.sampleRate() : m_sampleRate;
    const auto toSeconds = [rate](std::uint64_t samples) { return static_cast<double>(samples) / rate; };

    // --- Transporte ---
    const bool playing = session.isPlaying();

    if (ImGui::Button(playing ? "Pausar" : "Tocar", ImVec2(80.0f, 0.0f)))
    {
        if (playing)
            session.stop();
        else
            session.play();
    }

    ImGui::SameLine();

    if (ImGui::Button("Inicio"))
        session.seek(m_loopOn ? m_loopStart : 0);

    ImGui::SameLine();

    std::string position = formatClock(toSeconds(session.positionSamples()));
    if (session.hasBackingTrack())
        position += " / " + formatClock(toSeconds(session.backingTrackLengthSamples()));

    ImGui::TextUnformatted(position.c_str());

    // --- Metrônomo ---
    if (ImGui::Checkbox("Metronomo", &m_metronomeOn))
        session.setMetronomeEnabled(m_metronomeOn);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);

    if (ImGui::DragFloat("BPM", &m_bpm, 0.5f, 30.0f, 300.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp))
        session.setBpm(m_bpm);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);

    if (ImGui::SliderFloat("##clickVolume", &m_metronomeVolume, 0.0f, 1.0f, "clique %.2f"))
        session.setMetronomeVolume(m_metronomeVolume);

    // --- Backing track ---
    const std::string currentName = m_backingTrackPath.empty()
        ? std::string("(nenhuma backing track)")
        : std::filesystem::path(m_backingTrackPath).filename().string();

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);

    if (ImGui::BeginCombo("##backing", currentName.c_str()))
    {
        // Relê a pasta sempre que a lista abre — quem acabou de copiar um
        // .wav para audio/ não precisa procurar um botão de atualizar.
        refreshBackingTrackList();

        if (m_backingFiles.empty())
            ImGui::TextDisabled("coloque arquivos .wav em %s/", kAudioDir);

        for (const std::string& path : m_backingFiles)
        {
            const std::string label = std::filesystem::path(path).filename().string();

            if (ImGui::Selectable(label.c_str(), path == m_backingTrackPath))
                loadBackingTrack(path);
        }

        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);

    if (ImGui::SliderFloat("##backingVolume", &m_backingVolume, 0.0f, 1.0f, "faixa %.2f"))
        session.setBackingTrackVolume(m_backingVolume);

    // --- Loop A/B ---
    if (ImGui::Checkbox("Loop", &m_loopOn))
        session.setLoopEnabled(m_loopOn);

    ImGui::SameLine();

    if (ImGui::Button("A"))
    {
        m_loopStart = session.positionSamples();
        session.setLoop(m_loopStart, m_loopEnd);
    }

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("marca o inicio do loop na posicao atual");

    ImGui::SameLine();

    if (ImGui::Button("B"))
    {
        m_loopEnd = session.positionSamples();
        session.setLoop(m_loopStart, m_loopEnd);
    }

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("marca o fim do loop na posicao atual");

    ImGui::SameLine();

    if (m_loopEnd > m_loopStart)
        ImGui::Text("%s - %s", formatClock(toSeconds(m_loopStart)).c_str(), formatClock(toSeconds(m_loopEnd)).c_str());
    else
        ImGui::TextDisabled("marque A e B tocando");

    // --- Gravação da sessão ---
    if (!session.isRecording())
    {
        if (recordButton("Gravar sessao"))
        {
            try
            {
                std::filesystem::create_directories(kRecordingsDir);
                session.startRecording(std::string(kRecordingsDir) + "/" + fileTimestamp() + "-sessao.wav");
                m_sessionStartedAt = std::chrono::steady_clock::now();
                m_practiceMessage.clear();
            }
            catch (const std::exception& error)
            {
                m_practiceMessage = std::string("erro: ") + error.what();
            }
        }
    }
    else
    {
        if (ImGui::Button("Parar gravacao"))
        {
            session.stopRecording();
            m_practiceMessage = "sessao salva em " + std::string(kRecordingsDir) + "/";
        }

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.25f, 0.2f, 1.0f), "REC");
        ImGui::SameLine();
        ImGui::TextUnformatted(formatClock(secondsSince(m_sessionStartedAt)).c_str());
    }

    if (!m_practiceMessage.empty())
        ImGui::TextWrapped("%s", m_practiceMessage.c_str());
}

void DesktopUI::drawToolsRow()
{
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float available = ImGui::GetContentRegionAvail().x - 2.0f * spacing;

    // A prática tem o dobro de controles dos outros dois, então leva metade
    // da largura.
    const float tunerWidth = available * 0.26f;
    const float recorderWidth = available * 0.24f;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));

    ImGui::BeginChild("##tunerCard", ImVec2(tunerWidth, kToolCardHeight), ImGuiChildFlags_Borders);
    drawTunerPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##recorderCard", ImVec2(recorderWidth, kToolCardHeight), ImGuiChildFlags_Borders);
    drawRecorderPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##practiceCard", ImVec2(0.0f, kToolCardHeight), ImGuiChildFlags_Borders);
    drawPracticePanel();
    ImGui::EndChild();

    ImGui::PopStyleVar(2);
}

void DesktopUI::drawFrame()
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGui::Begin("IbiFX", nullptr,
                  ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // Duas colunas, como o AmpliTube: o rig à esquerda, o navegador de
    // equipamentos à direita. Cada coluna é uma child com rolagem própria.
    const float mainWidth = ImGui::GetContentRegionAvail().x - kBrowserWidth - ImGui::GetStyle().ItemSpacing.x;
    ImGui::BeginChild("##main", ImVec2(mainWidth, 0.0f));

    // --- Título ---
    // Fonte embutida do ImGui ampliada na hora, sem precisar carregar uma
    // segunda fonte só pra ter um "H1" — SetWindowFontScale muda só o
    // tamanho de desenho, não a nitidez (o glifo já é bitmap, não vetor).
    ImGui::SetWindowFontScale(1.9f);
    ImGui::TextUnformatted("IbiFX");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 14.0f);
    ImGui::TextDisabled("pedalboard digital");

    // --- Status ---
    // A palavra em si muda de cor (verde tocando, cinza parado) — mais
    // simples e mais seguro de alinhar do que desenhar uma bolinha à mão
    // misturada com o layout automático do ImGui.
    const bool running = m_engine.isRunning();
    const ImVec4 statusColor = running ? ImVec4(0.3f, 0.85f, 0.4f, 1.0f) : ImVec4(0.6f, 0.6f, 0.65f, 1.0f);

    ImGui::TextColored(statusColor, "%s", running ? "tocando" : "parado");
    ImGui::SameLine();
    ImGui::Text("   %s   %d Hz", m_engine.deviceName().c_str(), static_cast<int>(m_engine.sampleRate()));

    ImGui::Spacing();
    ImGui::Separator();

    // --- Presets, dentro de um cartão com fundo e borda próprios ---
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
    ImGui::BeginChild("##presetCard", ImVec2(0.0f, 210.0f), ImGuiChildFlags_Borders);
    drawPresetPanel();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::Spacing();

    // --- Níveis, no mesmo estilo de cartão ---
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
    ImGui::BeginChild("##meterCard", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    drawSectionHeader("Niveis");
    drawMeter("entrada", m_engine.inputPeak());
    drawMeter("saida", m_engine.outputPeak());
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::Spacing();

    // --- Afinador, gravador e prática, lado a lado ---
    drawToolsRow();

    ImGui::Spacing();

    // --- Rig, no mesmo estilo de cartão ---
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
    ImGui::BeginChild("##rigCard", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    drawRigStrip();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::Spacing();
    drawSectionHeader("Pedalboard");
    drawPedalboard();

    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
    ImGui::BeginChild("##browser", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    drawGearBrowser();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::End();
}

int DesktopUI::run(AudioDevice::Mode mode, double sampleRate, int blockSize)
{
    // Guardados para que loadPreset() consiga religar o motor com a mesma
    // configuração depois de pará-lo.
    m_mode = mode;
    m_sampleRate = sampleRate;
    m_blockSize = blockSize;

    // A janela abre com o afinador e o metrônomo desligados, e o estado do
    // engine precisa bater com o dos checkboxes desde o primeiro quadro.
    m_engine.setTunerEnabled(m_tunerOn);
    m_engine.setTunerMuted(m_tunerMuted);
    m_engine.practiceSession().setMetronomeEnabled(m_metronomeOn);
    m_engine.practiceSession().setBpm(m_bpm);

    if (!initWindow())
    {
        shutdownWindow();
        return 1;
    }

    if (!m_engine.start(mode, sampleRate, blockSize))
    {
        std::cerr << "erro: " << m_engine.lastError() << "\n";
        shutdownWindow();
        return 1;
    }

    captureTargets();
    refreshPresetList();

    while (!m_quit)
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);

            if (event.type == SDL_EVENT_QUIT)
                m_quit = true;

            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(m_window))
                m_quit = true;

            // Arrastar um .wav para a janela importa como IR.
            if (event.type == SDL_EVENT_DROP_FILE && event.drop.data != nullptr)
                importImpulseResponse(event.drop.data);
        }

        // Resposta do seletor de arquivos, se chegou uma desde o último
        // quadro — ver openImpulseResponseDialog().
        std::string chosenIr;
        {
            const std::lock_guard<std::mutex> lock(m_pendingIrMutex);
            chosenIr.swap(m_pendingIrPath);
        }

        if (!chosenIr.empty())
            importImpulseResponse(chosenIr);

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        drawFrame();

        ImGui::Render();
        SDL_SetRenderDrawColor(m_renderer, 20, 20, 20, 255);
        SDL_RenderClear(m_renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_renderer);
        SDL_RenderPresent(m_renderer);
    }

    // Fechar a janela no meio de uma gravação não pode perder o arquivo.
    stopAllRecordings();

    m_engine.stop();
    shutdownWindow();

    return 0;
}
