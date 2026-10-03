#include "DesktopUI.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

#include "Preset.h"
#include "PresetManager.h"

namespace
{
// Onde os presets salvos pela janela vivem, relativo ao diretório de
// trabalho — mesma convenção de audio/ no CLI.
constexpr const char* kPresetsDir = "presets";

constexpr int kWindowWidth = 760;
constexpr int kWindowHeight = 900;

// Dimensões do corpo de um pedal — ver drawPedal() para o layout completo.
constexpr float kPedalWidth = 168.0f;
constexpr float kPedalPadding = 14.0f;
constexpr float kPedalKnobRadius = 17.0f;
constexpr float kPedalHeaderHeight = 30.0f;
constexpr float kPedalKnobRowHeight = 76.0f;
constexpr float kPedalFootswitchAreaHeight = 86.0f;
constexpr float kFootswitchRadius = 20.0f;

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

DesktopUI::DesktopUI(LiveEngine& engine)
    : m_engine(engine)
{
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
    ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), overlay);
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
    // a mais pra achar escondido num canto. O LED acima acende quando o
    // pedal está ATIVO.
    const float footswitchCenterX = origin.x + kPedalWidth * 0.5f;
    const float footswitchCenterY = bodyMax.y - kPedalPadding - kFootswitchRadius;
    const float ledRadius = 5.0f;
    const float ledCenterY = footswitchCenterY - kFootswitchRadius - 16.0f;
    const ImVec2 ledCenter(footswitchCenterX, ledCenterY);

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

    ImGui::SetCursorScreenPos(ImVec2(footswitchCenterX - kFootswitchRadius, footswitchCenterY - kFootswitchRadius));
    ImGui::InvisibleButton("##footswitch", ImVec2(kFootswitchRadius * 2.0f, kFootswitchRadius * 2.0f));

    if (ImGui::IsItemClicked())
        m_engine.setBypassed(moduleIndex, !bypassed);

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(bypassed ? "ligar" : "desligar (bypass)");

    const bool footswitchHovered = ImGui::IsItemHovered();
    const ImU32 footswitchColor = ImGui::GetColorU32(
        footswitchHovered ? ImVec4(0.38f, 0.38f, 0.42f, 1.0f) : ImVec4(0.24f, 0.24f, 0.27f, 1.0f));

    const ImVec2 footswitchCenter(footswitchCenterX, footswitchCenterY);
    drawList->AddCircleFilled(footswitchCenter, kFootswitchRadius, footswitchColor, 24);
    drawList->AddCircle(footswitchCenter, kFootswitchRadius, borderColor, 24, 2.0f);
    drawList->AddCircle(footswitchCenter, kFootswitchRadius * 0.55f,
                         ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.5f)), 20, 1.5f);

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

        // Ver o comentário no header sobre por que parar o motor é
        // necessário aqui: preset::apply() reconstrói a cadeia inteira.
        m_engine.stop();
        preset::apply(loaded, m_engine.chain());
        const bool started = m_engine.start(m_mode, m_sampleRate, m_blockSize);

        if (!started)
        {
            m_presetMessage = "erro ao religar o motor: " + m_engine.lastError();
            return;
        }

        captureTargets();
        m_presetMessage = "preset carregado: " + loaded.name;
    }
    catch (const std::exception& error)
    {
        m_presetMessage = std::string("erro: ") + error.what();
    }
}

void DesktopUI::savePreset(const std::string& name)
{
    try
    {
        std::filesystem::create_directories(kPresetsDir);

        const std::string path = std::string(kPresetsDir) + "/" + name + ".ibifxpreset";
        preset::save(preset::capture(name, m_engine.chain()), path);

        m_presetMessage = "preset salvo: " + path;
        refreshPresetList();
    }
    catch (const std::exception& error)
    {
        m_presetMessage = std::string("erro: ") + error.what();
    }
}

void DesktopUI::drawPresetPanel()
{
    ImGui::TextUnformatted("Presets");

    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##presetName", "nome do preset", m_presetNameBuffer, sizeof(m_presetNameBuffer));

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

    if (m_presetFiles.empty())
    {
        ImGui::TextDisabled("(nenhum preset em presets/ ainda)");
    }
    else
    {
        for (const std::string& path : m_presetFiles)
        {
            const std::string label = std::filesystem::path(path).stem().string();

            if (ImGui::Selectable(label.c_str()))
                loadPreset(path);
        }
    }

    if (!m_presetMessage.empty())
        ImGui::TextUnformatted(m_presetMessage.c_str());
}

void DesktopUI::drawFrame()
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGui::Begin("IbiFX", nullptr,
                  ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::TextUnformatted(m_engine.isRunning() ? "tocando" : "parado");
    ImGui::Text("%s   %d Hz", m_engine.deviceName().c_str(), static_cast<int>(m_engine.sampleRate()));

    ImGui::Separator();
    drawPresetPanel();
    ImGui::Separator();
    drawMeter("entrada", m_engine.inputPeak());
    drawMeter("saida", m_engine.outputPeak());
    ImGui::Separator();
    ImGui::Spacing();

    // Os pedais, lado a lado, como um pedalboard de verdade — quebrando
    // pra próxima linha quando não cabe mais nenhum na largura da janela.
    // É o mesmo idioma de quebra que o próprio demo do ImGui usa pra
    // botões: desenha, tenta continuar na mesma linha, e desiste se não
    // sobrar espaço.
    const ModuleChain& chain = m_engine.chain();
    std::size_t flat = 0;

    for (std::size_t i = 0; i < chain.size(); ++i)
    {
        const AudioModule& module = chain.moduleAt(i);
        const std::size_t firstFlat = flat;
        flat += module.parameterCount();

        // Escopa os ids de todos os widgets deste pedal (knobs, reset,
        // footswitch), pra dois módulos do mesmo tipo (dois Gain, por
        // exemplo) não colidirem só por terem o mesmo name().
        ImGui::PushID(static_cast<int>(i));
        drawPedal(i, firstFlat);
        ImGui::PopID();

        if (i + 1 < chain.size())
        {
            ImGui::SameLine();

            if (ImGui::GetContentRegionAvail().x < kPedalWidth)
                ImGui::NewLine();
        }
    }

    ImGui::End();
}

int DesktopUI::run(AudioDevice::Mode mode, double sampleRate, int blockSize)
{
    // Guardados para que loadPreset() consiga religar o motor com a mesma
    // configuração depois de pará-lo.
    m_mode = mode;
    m_sampleRate = sampleRate;
    m_blockSize = blockSize;

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
        }

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

    m_engine.stop();
    shutdownWindow();

    return 0;
}
