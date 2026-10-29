#include "DesktopUI.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <stb_image.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

#include "GearSkins.h"
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

// Hallmark · pre-emit critique: P5 H5 E4 S5 R5 V5 · macrostructure:
// Workbench · tone: technical/austere.

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

constexpr float kWorkspaceHeaderHeight = 120.0f;
// Tamanhos da signal chain em escala 1. Quando a cadeia não cabe na
// largura, tudo encolhe pelo mesmo fator (até kSignalMinScale) em vez de
// abrir um scroll horizontal.
constexpr float kSignalChainHeight = 136.0f;
constexpr float kSignalCardWidth = 84.0f;
constexpr float kSignalCardHeight = 86.0f;
constexpr float kSignalSlotWidth = 22.0f;
constexpr float kSignalMinScale = 0.35f;

// A miniatura é desenhada numa caixa de 42 x 48 em escala 1.
constexpr float kSignalThumbWidth = 42.0f;
constexpr float kSignalThumbHeight = 48.0f;

// Paleta da identidade visual (docs/identidade-visual.md): Ivory para
// texto, Charcoal para o fundo, Warm Gray para o que é secundário e
// Vintage Gold como único acento.
constexpr ImVec4 kIvory(0.910f, 0.878f, 0.800f, 1.0f);         // #E8E0CC
constexpr ImVec4 kCharcoal(0.067f, 0.075f, 0.082f, 1.0f);      // #111315
constexpr ImVec4 kWarmGray(0.616f, 0.592f, 0.541f, 1.0f);      // #9D978A
constexpr ImVec4 kVintageGold(0.780f, 0.690f, 0.541f, 1.0f);   // #C7B08A

constexpr ImVec4 kAccentColor = kVintageGold;
constexpr ImVec4 kSurfaceRaised(0.118f, 0.122f, 0.125f, 1.0f);
constexpr ImVec4 kSurfaceSelected(0.235f, 0.208f, 0.165f, 1.0f);

constexpr const char* kLogoPath = "assets/brand/ibifx-logo.png";

// Imagens dos equipamentos (GearSkins.h): assets/gear/<id>/ e as peças
// comuns em assets/gear/_shared/.
constexpr const char* kGearAssetsDir = "assets/gear";

// Na imagem do footswitch, o centro da porca fica a 57% da altura: o botão
// aparece em leve perspectiva, subindo acima dela. É a porca que se alinha
// com o furo do corpo.
constexpr float kFootswitchNutCenter = 0.57f;

// O ImGui identifica texturas por ImTextureID; com o backend SDL_Renderer,
// o id é o próprio ponteiro da SDL_Texture.
ImTextureRef textureRef(SDL_Texture* texture)
{
    return ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(texture)));
}

// Unbounded (OFL), sans larga no lugar da Monument Extended da folha da
// marca. Só para rótulos e nomes; o texto corrido segue na fonte padrão.
constexpr const char* kBrandFontPath = "assets/fonts/Unbounded-SemiBold.ttf";
constexpr float kBrandLabelSize = 13.0f;
constexpr ImVec4 kHealthyColor(0.30f, 0.85f, 0.40f, 1.0f);

constexpr const char* kGearDragPayload = "IBIFX_GEAR_ID";
constexpr const char* kChainItemDragPayload = "IBIFX_CHAIN_ITEM";

// O painel de um ampli: uma célula por knob, numa fileira só.
constexpr float kAmpKnobCellWidth = 86.0f;
constexpr float kAmpKnobRadius = 20.0f;

// Largura do painel grande de um equipamento selecionado.
float gearPanelWidth(const GearModel& model)
{
    if (model.category == GearCategory::Amp)
        return kAmpKnobCellWidth * static_cast<float>(model.controls.size()) + 2.0f * kPedalPadding;
    return kPedalWidth;
}

enum class SignalCardKind
{
    Stomp,
    Amp,
    Cabinet,
    Rack,
    Module,
    Output
};

SignalCardKind signalCardKind(GearCategory category)
{
    switch (category)
    {
    case GearCategory::Stomp: return SignalCardKind::Stomp;
    case GearCategory::Amp: return SignalCardKind::Amp;
    case GearCategory::Cabinet: return SignalCardKind::Cabinet;
    case GearCategory::Rack: return SignalCardKind::Rack;
    case GearCategory::Equalizer: return SignalCardKind::Rack;
    }
    return SignalCardKind::Module;
}

ImU32 colorForModuleName(const char* name, bool bypassed);

std::string compactLabel(const std::string& value, std::size_t limit = 18)
{
    if (value.size() <= limit)
        return value;
    return value.substr(0, limit > 3 ? limit - 3 : limit) + "...";
}

// Corta o texto com "..." até caber em maxWidth pixels no tamanho dado.
std::string fitLabel(const std::string& value, float fontSize, float maxWidth)
{
    ImFont* font = ImGui::GetFont();
    if (font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, value.c_str()).x <= maxWidth)
        return value;

    std::string cut = value;
    while (!cut.empty())
    {
        cut.pop_back();
        const std::string candidate = cut + "...";
        if (font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, candidate.c_str()).x <= maxWidth)
            return candidate;
    }
    return {};
}

// Desenha a miniatura com o canto superior esquerdo em origin, numa caixa
// de kSignalThumbWidth x kSignalThumbHeight multiplicada por scale.
void drawSignalThumbnail(ImDrawList* drawList, ImVec2 origin, float scale, SignalCardKind kind, ImU32 bodyColor)
{
    const ImU32 edge = ImGui::GetColorU32(ImVec4(0.04f, 0.04f, 0.05f, 0.85f));
    const ImU32 control = ImGui::GetColorU32(kIvory);
    const ImU32 dark = ImGui::GetColorU32(kCharcoal);
    const ImU32 led = ImGui::GetColorU32(kAccentColor);

    auto at = [&](float dx, float dy) { return ImVec2(origin.x + dx * scale, origin.y + dy * scale); };
    const float r = scale;

    if (kind == SignalCardKind::Stomp || kind == SignalCardKind::Module)
    {
        drawList->AddRectFilled(at(5.0f, 0.0f), at(35.0f, 48.0f), bodyColor, 4.0f * r);
        drawList->AddRect(at(5.0f, 0.0f), at(35.0f, 48.0f), edge, 4.0f * r);
        drawList->AddCircleFilled(at(14.0f, 12.0f), 3.0f * r, control);
        drawList->AddCircleFilled(at(26.0f, 12.0f), 3.0f * r, control);
        drawList->AddCircleFilled(at(20.0f, 27.0f), 2.5f * r, led);
        drawList->AddCircle(at(20.0f, 39.0f), 5.0f * r, control, 12, 1.5f * r);
        return;
    }

    if (kind == SignalCardKind::Amp)
    {
        drawList->AddRectFilled(at(0.0f, 7.0f), at(42.0f, 39.0f), dark, 4.0f * r);
        drawList->AddRect(at(0.0f, 7.0f), at(42.0f, 39.0f), edge, 4.0f * r);
        drawList->AddRectFilled(at(4.0f, 12.0f), at(38.0f, 25.0f), bodyColor, 2.0f * r);
        for (int knobIndex = 0; knobIndex < 5; ++knobIndex)
            drawList->AddCircleFilled(at(8.0f + static_cast<float>(knobIndex) * 6.5f, 18.5f), 1.6f * r, dark);
        drawList->AddRectFilled(at(7.0f, 30.0f), at(35.0f, 35.0f), bodyColor, 1.0f * r);
        return;
    }

    if (kind == SignalCardKind::Cabinet)
    {
        drawList->AddRectFilled(at(4.0f, 0.0f), at(38.0f, 48.0f), dark, 3.0f * r);
        drawList->AddRect(at(4.0f, 0.0f), at(38.0f, 48.0f), edge, 3.0f * r);
        drawList->AddCircleFilled(at(21.0f, 16.0f), 9.0f * r, bodyColor);
        drawList->AddCircle(at(21.0f, 16.0f), 4.0f * r, edge, 16, 1.0f * r);
        drawList->AddCircleFilled(at(21.0f, 36.0f), 8.0f * r, bodyColor);
        drawList->AddCircle(at(21.0f, 36.0f), 3.5f * r, edge, 16, 1.0f * r);
        return;
    }

    drawList->AddRectFilled(at(0.0f, 10.0f), at(42.0f, 36.0f), dark, 3.0f * r);
    drawList->AddRect(at(0.0f, 10.0f), at(42.0f, 36.0f), edge, 3.0f * r);
    drawList->AddRectFilled(at(5.0f, 15.0f), at(29.0f, 31.0f), bodyColor, 2.0f * r);
    drawList->AddCircleFilled(at(34.0f, 19.0f), 2.0f * r, led);
    drawList->AddCircleFilled(at(34.0f, 27.0f), 2.0f * r, control);
}

// Um item da signal chain: miniatura em cima, nome embaixo. Desenhado na
// posição atual do cursor, já no tamanho multiplicado por scale.
bool signalCard(const char* id, const std::string& title, SignalCardKind kind, ImU32 bodyColor,
                bool selected, float scale, SDL_Texture* thumbnail = nullptr, float thumbnailAspect = 1.0f)
{
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size(kSignalCardWidth * scale, kSignalCardHeight * scale);
    ImGui::InvisibleButton(id, size);

    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 end(origin.x + size.x, origin.y + size.y);

    const ImVec4 fill = selected ? kSurfaceSelected
                                 : (hovered ? ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered)
                                            : kSurfaceRaised);
    const ImVec4 border = selected ? kAccentColor : ImGui::GetStyleColorVec4(ImGuiCol_Separator);

    drawList->AddRectFilled(origin, end, ImGui::GetColorU32(fill), 6.0f * scale);
    drawList->AddRect(origin, end, ImGui::GetColorU32(border), 6.0f * scale, 0, selected ? 2.0f : 1.0f);

    const ImVec2 thumbOrigin(origin.x + (size.x - kSignalThumbWidth * scale) * 0.5f, origin.y + 10.0f * scale);
    if (thumbnail != nullptr)
    {
        // A imagem do equipamento, na mesma caixa da miniatura vetorial,
        // mantendo a proporção.
        const float boxHeight = kSignalThumbHeight * scale;
        const float imageWidth = std::min(kSignalThumbWidth * scale, boxHeight * thumbnailAspect);
        const float imageHeight = imageWidth / thumbnailAspect;
        const ImVec2 imageMin(origin.x + (size.x - imageWidth) * 0.5f,
                              thumbOrigin.y + (boxHeight - imageHeight) * 0.5f);
        drawList->AddImage(textureRef(thumbnail), imageMin,
                           ImVec2(imageMin.x + imageWidth, imageMin.y + imageHeight));
    }
    else
    {
        drawSignalThumbnail(drawList, thumbOrigin, scale, kind, bodyColor);
    }

    // A fonte encolhe menos que a miniatura, para o nome continuar legível.
    const float fontSize = ImGui::GetFontSize() * std::clamp(scale, 0.75f, 1.0f);
    const std::string visibleTitle = fitLabel(title, fontSize, size.x - 6.0f);
    const ImVec2 titleSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, visibleTitle.c_str());
    drawList->AddText(ImGui::GetFont(), fontSize,
                      ImVec2(origin.x + (size.x - titleSize.x) * 0.5f, end.y - titleSize.y - 6.0f * scale),
                      ImGui::GetColorU32(ImGuiCol_Text), visibleTitle.c_str());

    if (hovered && visibleTitle != title)
        ImGui::SetTooltip("%s", title.c_str());

    return clicked;
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

// Com texture, desenha a imagem do knob girada em vez do knob vetorial. A
// imagem tem o ponteiro às 12 horas, que é o meio do curso.
bool knob(const char* id, float* value, float minValue, float maxValue, float radius,
          SDL_Texture* texture = nullptr)
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

    if (texture != nullptr)
    {
        const float rotation = angle - 3.14159265f * 1.5f;
        const float cosine = std::cos(rotation);
        const float sine = std::sin(rotation);
        auto corner = [&](float dx, float dy) {
            return ImVec2(center.x + (dx * cosine - dy * sine) * radius,
                          center.y + (dx * sine + dy * cosine) * radius);
        };
        drawList->AddImageQuad(textureRef(texture), corner(-1.0f, -1.0f), corner(1.0f, -1.0f),
                               corner(1.0f, 1.0f), corner(-1.0f, 1.0f));
        return changed;
    }

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
        hovered ? ImVec4(0.40f, 0.38f, 0.35f, 1.0f) : ImVec4(0.26f, 0.25f, 0.23f, 1.0f));

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
    else if (parameter.id() == "rate")
        std::snprintf(buffer, sizeof(buffer), "%.2f Hz", static_cast<double>(value));
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

// Tema da identidade visual: fundo Charcoal, texto Ivory, Warm Gray para
// o secundário e Vintage Gold como acento — o dourado de placa de ampli
// vintage. Botões em pílula, como os rótulos da folha da marca. Chamado
// uma vez, logo após CreateContext().
void applyTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 8.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 12.0f;
    style.GrabRounding = 12.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 12.0f;
    style.TabRounding = 12.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    // Espaçamento mais folgado que o padrão do ImGui — sliders e checkboxes
    // colados uns nos outros parecem uma lista crua, não uma pedaleira.
    style.WindowPadding = ImVec2(16.0f, 16.0f);
    style.FramePadding = ImVec2(12.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    const ImVec4 background = kCharcoal;
    const ImVec4 panel(0.153f, 0.153f, 0.149f, 1.00f);
    const ImVec4 panelHovered(0.212f, 0.204f, 0.188f, 1.00f);
    const ImVec4 panelActive(0.267f, 0.251f, 0.224f, 1.00f);
    const ImVec4 accent = kVintageGold;
    const ImVec4 accentHovered(0.851f, 0.769f, 0.631f, 1.00f);
    const ImVec4 accentActive(0.910f, 0.839f, 0.710f, 1.00f);
    const ImVec4 text = kIvory;
    const ImVec4 textDisabled = kWarmGray;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = text;
    colors[ImGuiCol_TextDisabled] = textDisabled;
    colors[ImGuiCol_WindowBg] = background;
    colors[ImGuiCol_ChildBg] = ImVec4(0.090f, 0.098f, 0.102f, 1.00f);
    colors[ImGuiCol_PopupBg] = panel;
    colors[ImGuiCol_Border] = ImVec4(kWarmGray.x, kWarmGray.y, kWarmGray.z, 0.22f);
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
    colors[ImGuiCol_Separator] = ImVec4(kWarmGray.x, kWarmGray.y, kWarmGray.z, 0.30f);
    colors[ImGuiCol_SeparatorHovered] = accent;
    colors[ImGuiCol_SeparatorActive] = accentActive;
    // O medidor sobrescreve esta cor por chamada em drawMeter() (verde,
    // amarelo ou vermelho conforme o nível) — isto aqui é só o padrão.
    colors[ImGuiCol_PlotHistogram] = accent;
    colors[ImGuiCol_PlotHistogramHovered] = accentHovered;
    colors[ImGuiCol_Tab] = panel;
    colors[ImGuiCol_TabHovered] = panelHovered;
    colors[ImGuiCol_TabSelected] = panelActive;
    colors[ImGuiCol_DragDropTarget] = accent;
    colors[ImGuiCol_NavCursor] = accent;
    colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
}

// Texto em caixa alta com espaçamento largo entre as letras — o estilo dos
// rótulos da marca ("AUDIO TOOLS FOR CREATORS"). Avança o cursor como um
// item normal.
void trackedCaps(const char* label, ImVec4 color, ImFont* font, float tracking = 2.0f,
                 float size = kBrandLabelSize)
{
    // Sem a fonte da marca, a padrão no tamanho atual.
    ImGui::PushFont(font, font != nullptr ? size : 0.0f);

    std::string upper(label);
    for (char& c : upper)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 col = ImGui::GetColorU32(color);
    float x = origin.x;
    for (char c : upper)
    {
        const char glyph[2] = {c, '\0'};
        drawList->AddText(ImVec2(x, origin.y), col, glyph);
        x += ImGui::CalcTextSize(glyph).x + tracking;
    }
    ImGui::Dummy(ImVec2(std::max(0.0f, x - origin.x - tracking), ImGui::GetTextLineHeight()));
    ImGui::PopFont();
}

// Os quatro traços inclinados que fecham os detalhes da folha da marca.
void drawBrandStripes(ImDrawList* drawList, ImVec2 origin, float height, ImU32 color)
{
    for (int i = 0; i < 4; ++i)
    {
        const float x = origin.x + static_cast<float>(i) * height * 0.75f;
        drawList->AddQuadFilled(ImVec2(x + height * 0.45f, origin.y), ImVec2(x + height * 0.80f, origin.y),
                                ImVec2(x + height * 0.35f, origin.y + height), ImVec2(x, origin.y + height),
                                color);
    }
}
}

DesktopUI::DesktopUI(LiveEngine& engine, std::string irsDirectory, bool developerMode)
    : m_engine(engine)
    , m_irsDirectory(std::move(irsDirectory))
    , m_developerMode(developerMode)
{
    m_library.scanImpulseResponses(m_irsDirectory);
}

void DesktopUI::adoptRig(const Rig& rig)
{
    Preset current = preset::capture("Rig", m_engine.chain());
    for (const GearModel* model : m_library.rigModels(rig))
        current.chainItems.push_back({model->id, model->modules.size()});

    std::size_t covered = 0;
    for (const Preset::ChainItemState& item : current.chainItems)
        covered += item.moduleCount;
    while (covered < current.modules.size())
    {
        current.chainItems.push_back({"", 1});
        ++covered;
    }

    adoptChainLayout(current);

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

    // Opcional, como o logo: sem o arquivo, os rótulos usam a padrão.
    std::error_code fontError;
    if (std::filesystem::exists(kBrandFontPath, fontError))
        m_brandFont = io.Fonts->AddFontFromFileTTF(kBrandFontPath, kBrandLabelSize);

    applyTheme();

    // O logo é opcional: sem o arquivo (rodando de outra pasta), o
    // cabeçalho cai para o nome em texto. A textura pertence ao renderer,
    // que a libera em SDL_DestroyRenderer().
    m_logoTexture = loadTexture(kLogoPath, m_logoWidth, m_logoHeight);

    // Peças comuns às skins de equipamento. Sem elas, cada skin ainda
    // desenha corpo e knobs; só faltam LED e footswitch.
    const std::string shared = std::string(kGearAssetsDir) + "/_shared/";
    float ignoredWidth = 0.0f, ignoredHeight = 0.0f;
    m_ledOnTexture = loadTexture(shared + "led-on.png", ignoredWidth, ignoredHeight);
    m_ledOffTexture = loadTexture(shared + "led-off.png", ignoredWidth, ignoredHeight);
    m_footswitchTexture = loadTexture(shared + "footswitch.png", m_footswitchWidth, m_footswitchHeight);

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

bool DesktopUI::startAudio()
{
    try
    {
        if (m_engine.start(m_mode, m_sampleRate, m_blockSize))
        {
            m_audioStartError.clear();
            return true;
        }

        m_audioStartError = m_engine.lastError();
    }
    catch (const std::exception& error)
    {
        m_audioStartError = error.what();
    }

    return false;
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

        const float knobTop = cellTop + ImGui::GetTextLineHeight() + 4.0f;
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
    adoptChainLayout(loaded);

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

    if (!startAudio())
    {
        error = "erro ao religar o motor: " + m_audioStartError;
        return false;
    }

    captureTargets();
    return true;
}

void DesktopUI::adoptChainLayout(const Preset& source)
{
    m_chainItems.clear();
    m_expandedSegments.clear();

    auto adoptStates = [&](const std::vector<Preset::ChainItemState>& states) {
        std::size_t firstModule = 0;
        for (const Preset::ChainItemState& state : states)
        {
            if (state.moduleCount == 0 || firstModule + state.moduleCount > source.modules.size())
                return false;
            m_chainItems.push_back({state.gearId, firstModule, state.moduleCount});
            firstModule += state.moduleCount;
        }
        return firstModule == source.modules.size();
    };

    if (!source.chainItems.empty() && adoptStates(source.chainItems))
    {
        m_selectedItem = m_chainItems.empty() ? kNoSelection : 0;
        return;
    }

    m_chainItems.clear();

    // Compatibilidade: presets antigos conheciam apenas o Rig fixo. Ele é
    // convertido uma vez para a sequência livre, sem reaplicar receitas.
    Rig legacyRig;
    bool hasLegacyRig = !source.rig.empty() && m_library.matchesRig(source, source.rig);
    if (hasLegacyRig)
        legacyRig = source.rig;
    else
        hasLegacyRig = m_library.inferRig(source, legacyRig);

    std::size_t firstModule = 0;
    if (hasLegacyRig)
    {
        for (const GearModel* model : m_library.rigModels(legacyRig))
        {
            m_chainItems.push_back({model->id, firstModule, model->modules.size()});
            firstModule += model->modules.size();
        }
    }

    while (firstModule < source.modules.size())
    {
        m_chainItems.push_back({"", firstModule, 1});
        ++firstModule;
    }

    m_selectedItem = m_chainItems.empty() ? kNoSelection : 0;
}

Preset DesktopUI::captureCurrentPreset(const std::string& name) const
{
    Preset captured = preset::capture(name, m_engine.chain());

    std::size_t covered = 0;
    for (const ChainItem& item : m_chainItems)
    {
        if (item.moduleCount == 0 || item.firstModule != covered
            || covered + item.moduleCount > captured.modules.size())
        {
            captured.chainItems.clear();
            break;
        }
        captured.chainItems.push_back({item.gearId, item.moduleCount});
        covered += item.moduleCount;
    }

    if (covered != captured.modules.size())
    {
        captured.chainItems.clear();
        for (std::size_t i = 0; i < captured.modules.size(); ++i)
            captured.chainItems.push_back({"", 1});
    }

    return captured;
}

void DesktopUI::insertGearAt(const std::string& gearId, std::size_t itemIndex)
{
    const GearModel* model = m_library.find(gearId);
    if (model == nullptr || itemIndex > m_chainItems.size())
        return;

    Preset edited = captureCurrentPreset("Cadeia");
    std::size_t moduleIndex = 0;
    for (std::size_t i = 0; i < itemIndex; ++i)
        moduleIndex += edited.chainItems[i].moduleCount;

    edited.modules.insert(edited.modules.begin() + static_cast<std::ptrdiff_t>(moduleIndex),
                          model->modules.begin(), model->modules.end());
    edited.chainItems.insert(edited.chainItems.begin() + static_cast<std::ptrdiff_t>(itemIndex),
                             {model->id, model->modules.size()});
    edited.rig = {};

    std::string error;
    if (!replaceChain(edited, error))
    {
        m_rigMessage = error;
        return;
    }

    adoptChainLayout(edited);
    m_selectedItem = itemIndex;
    m_currentPresetName.clear();
    m_currentPresetPath.clear();
    m_rigMessage = model->name + " inserido na cadeia";
}

void DesktopUI::removeChainItem(std::size_t itemIndex)
{
    if (itemIndex >= m_chainItems.size())
        return;

    Preset edited = captureCurrentPreset("Cadeia");
    std::size_t moduleIndex = 0;
    for (std::size_t i = 0; i < itemIndex; ++i)
        moduleIndex += edited.chainItems[i].moduleCount;
    const std::size_t count = edited.chainItems[itemIndex].moduleCount;

    edited.modules.erase(edited.modules.begin() + static_cast<std::ptrdiff_t>(moduleIndex),
                         edited.modules.begin() + static_cast<std::ptrdiff_t>(moduleIndex + count));
    edited.chainItems.erase(edited.chainItems.begin() + static_cast<std::ptrdiff_t>(itemIndex));
    edited.rig = {};

    std::string error;
    if (!replaceChain(edited, error))
    {
        m_rigMessage = error;
        return;
    }

    adoptChainLayout(edited);
    if (!m_chainItems.empty())
        m_selectedItem = std::min(itemIndex, m_chainItems.size() - 1);
    m_currentPresetName.clear();
    m_currentPresetPath.clear();
    m_rigMessage = "item removido da cadeia";
}

void DesktopUI::moveChainItem(std::size_t from, std::size_t insertionIndex)
{
    if (from >= m_chainItems.size() || insertionIndex > m_chainItems.size()
        || insertionIndex == from || insertionIndex == from + 1)
        return;

    Preset edited = captureCurrentPreset("Cadeia");
    std::size_t moduleFrom = 0;
    for (std::size_t i = 0; i < from; ++i)
        moduleFrom += edited.chainItems[i].moduleCount;

    const Preset::ChainItemState movedItem = edited.chainItems[from];
    std::vector<Preset::ModuleState> movedModules(
        edited.modules.begin() + static_cast<std::ptrdiff_t>(moduleFrom),
        edited.modules.begin() + static_cast<std::ptrdiff_t>(moduleFrom + movedItem.moduleCount));

    edited.modules.erase(edited.modules.begin() + static_cast<std::ptrdiff_t>(moduleFrom),
                         edited.modules.begin() + static_cast<std::ptrdiff_t>(moduleFrom + movedItem.moduleCount));
    edited.chainItems.erase(edited.chainItems.begin() + static_cast<std::ptrdiff_t>(from));

    std::size_t destinationItem = insertionIndex;
    if (from < insertionIndex)
        --destinationItem;

    std::size_t moduleDestination = 0;
    for (std::size_t i = 0; i < destinationItem; ++i)
        moduleDestination += edited.chainItems[i].moduleCount;

    edited.modules.insert(edited.modules.begin() + static_cast<std::ptrdiff_t>(moduleDestination),
                          movedModules.begin(), movedModules.end());
    edited.chainItems.insert(edited.chainItems.begin() + static_cast<std::ptrdiff_t>(destinationItem), movedItem);
    edited.rig = {};

    std::string error;
    if (!replaceChain(edited, error))
    {
        m_rigMessage = error;
        return;
    }

    adoptChainLayout(edited);
    m_selectedItem = destinationItem;
    m_currentPresetName.clear();
    m_currentPresetPath.clear();
    m_rigMessage = "ordem da cadeia atualizada";
}

void DesktopUI::drawGearBrowser()
{
    drawSectionHeader("Biblioteca");

    if (ImGui::CollapsingHeader("Presets"))
    {
        ImGui::Indent();
        drawPresetPanel(false);
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Cadeia atual"))
    {
        ImGui::Indent();
        drawRigStrip(false);
        ImGui::Unindent();
    }

    constexpr GearCategory kCategories[] = {GearCategory::Stomp, GearCategory::Amp,
                                            GearCategory::Cabinet, GearCategory::Rack,
                                            GearCategory::Equalizer};
    constexpr const char* kLabels[] = {"Pedais", "Amp", "Cab", "Rack", "EQ"};
    constexpr float kWidths[] = {62.0f, 46.0f, 46.0f, 50.0f, 40.0f};

    for (int i = 0; i < 5; ++i)
    {
        if (i > 0)
            ImGui::SameLine();
        const bool selected = m_browserCategory == kCategories[i];
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, kSurfaceSelected);
        if (ImGui::Button(kLabels[i], ImVec2(kWidths[i], 0.0f)))
            m_browserCategory = kCategories[i];
        if (selected)
            ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Arraste para um ponto da linha. Duplo clique adiciona no final.");
    ImGui::Separator();
    drawGearCategory(m_browserCategory, gearCategoryName(m_browserCategory), true);

    if (!m_rigMessage.empty())
    {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_rigMessage.c_str());
    }
}

void DesktopUI::drawGearCategory(GearCategory category, const char* label, bool defaultOpen)
{
    ImGui::PushID(label);
    const std::vector<const GearModel*> models = m_library.modelsIn(category);

    if (models.empty() && category == GearCategory::Cabinet)
    {
        ImGui::Spacing();
        ImGui::TextWrapped("Nenhum cabinet encontrado. Clique em \"Importar IR...\" "
                           "(ou arraste um .wav para a janela), ou coloque IRs em "
                           "irs/<gabinete>/<microfone>.wav e clique em \"Reler irs/\".");
    }

    std::vector<std::string> groups;
    for (const GearModel* model : models)
    {
        const std::string group = category == GearCategory::Cabinet ? model->name : model->character;
        if (std::find(groups.begin(), groups.end(), group) == groups.end())
            groups.push_back(group);
    }

    for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        const ImGuiTreeNodeFlags flags = defaultOpen && groupIndex == 0
            ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
        if (!ImGui::CollapsingHeader(groups[groupIndex].c_str(), flags))
            continue;

        ImGui::Indent();
        for (const GearModel* model : models)
        {
            const std::string group = category == GearCategory::Cabinet ? model->name : model->character;
            if (group != groups[groupIndex])
                continue;

            ImGui::PushID(model->id.c_str());
            const ImVec4 swatch = ImGui::ColorConvertU32ToFloat4(
                colorForModuleName(model->name.c_str(), false));
            ImGui::ColorButton("##swatch", swatch, ImGuiColorEditFlags_NoTooltip, ImVec2(16.0f, 16.0f));
            ImGui::SameLine();

            const bool isCabinet = model->category == GearCategory::Cabinet;
            const std::string itemLabel = isCabinet ? model->microphone : model->name;
            const bool inChain = std::any_of(m_chainItems.begin(), m_chainItems.end(),
                [&](const ChainItem& item) { return item.gearId == model->id; });
            const float deleteWidth = isCabinet
                ? ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x : 0.0f;

            ImGui::Selectable(itemLabel.c_str(), inChain, ImGuiSelectableFlags_AllowDoubleClick,
                              ImVec2(ImGui::GetContentRegionAvail().x - deleteWidth, 0.0f));

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                insertGearAt(model->id, m_chainItems.size());

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload(kGearDragPayload, model->id.c_str(), model->id.size() + 1);
                ImGui::TextUnformatted(model->name.c_str());
                ImGui::TextDisabled("solte entre dois itens da cadeia");
                ImGui::EndDragDropSource();
            }

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s\n%s\nArraste para inserir; duplo clique adiciona no final.",
                                  model->character.c_str(), model->description.c_str());

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
        ImGui::Unindent();
    }

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

    if (category == GearCategory::Cabinet)
    {
        if (ImGui::Button("Importar IR..."))
            openImpulseResponseDialog();

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("escolhe um .wav de IR (ou arraste o arquivo para a janela)");

        if (ImGui::Button("Reler irs/"))
        {
            const std::size_t found = m_library.scanImpulseResponses(m_irsDirectory);
            m_rigMessage = std::to_string(found) + " cabinet(s) em " + m_irsDirectory;
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("procura cabinets de novo em irs/");
    }

    ImGui::PopID();
}

void DesktopUI::drawRigStrip(bool showHeader)
{
    if (showHeader)
        drawSectionHeader("Cadeia atual");

    if (m_chainItems.empty())
        ImGui::TextDisabled("cadeia vazia - arraste qualquer equipamento para a linha");

    std::size_t moveFrom = kNoSelection;
    std::size_t moveTo = kNoSelection;
    std::size_t removeAt = kNoSelection;

    for (std::size_t i = 0; i < m_chainItems.size(); ++i)
    {
        const ChainItem& item = m_chainItems[i];
        const GearModel* model = item.gearId.empty() ? nullptr : m_library.find(item.gearId);
        const std::string name = model != nullptr
            ? model->name
            : (item.firstModule < m_engine.chain().size()
                   ? m_engine.chain().moduleAt(item.firstModule).name() : "Modulo");

        ImGui::PushID(static_cast<int>(i));
        ImGui::TextDisabled("%02d", static_cast<int>(i + 1));
        ImGui::SameLine();
        ImGui::TextUnformatted(name.c_str());

        const float controlsWidth = 3.0f * ImGui::GetFrameHeight()
                                  + 2.0f * ImGui::GetStyle().ItemSpacing.x;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - controlsWidth));

        ImGui::BeginDisabled(i == 0);
        if (ImGui::SmallButton("<"))
        {
            moveFrom = i;
            moveTo = i - 1;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        ImGui::BeginDisabled(i + 1 >= m_chainItems.size());
        if (ImGui::SmallButton(">"))
        {
            moveFrom = i;
            moveTo = i + 2;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::SmallButton("x"))
            removeAt = i;
        ImGui::PopID();
    }

    if (removeAt != kNoSelection)
        removeChainItem(removeAt);
    else if (moveFrom != kNoSelection)
        moveChainItem(moveFrom, moveTo);

    if (!m_rigMessage.empty())
    {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_rigMessage.c_str());
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
    const ChainItem& segment = m_chainItems[segmentIndex];
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

    // Os botões (Reset e, no modo desenvolvedor, "+") ficam embaixo do
    // corpo, como na skin com imagens — no pedal não cabem ao lado do
    // footswitch.
    const float buttonsHeight = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("gear", ImVec2(width, height + buttonsHeight), ImGuiChildFlags_None,
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
        drawList->AddRectFilled(origin, bodyMax, ImGui::GetColorU32(kCharcoal), 10.0f);
        drawList->AddRectFilled(plateMin, plateMax,
                                ImGui::GetColorU32(ImVec4(kVintageGold.x * dim, kVintageGold.y * dim,
                                                          kVintageGold.z * dim, 1.0f)), 4.0f);
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
    if (m_brandFont != nullptr)
        ImGui::PushFont(m_brandFont, isAmp ? 22.0f : 15.0f);
    else
        ImGui::SetWindowFontScale(isAmp ? 1.45f : 1.0f);
    const ImVec2 nameSize = ImGui::CalcTextSize(model.name.c_str());
    ImGui::SetCursorScreenPos(ImVec2(origin.x + (width - nameSize.x) * 0.5f,
                                     origin.y + (isAmp ? 14.0f : 10.0f)));
    ImGui::TextUnformatted(model.name.c_str());
    if (m_brandFont != nullptr)
        ImGui::PopFont();
    else
        ImGui::SetWindowFontScale(1.0f);

    if (!model.microphone.empty())
    {
        const ImVec2 micSize = ImGui::CalcTextSize(model.microphone.c_str());
        ImGui::SetCursorScreenPos(ImVec2(origin.x + (width - micSize.x) * 0.5f, origin.y + kPedalHeaderHeight));
        ImGui::TextDisabled("%s", model.microphone.c_str());
    }

    // Reset (valores de fábrica do equipamento) e, só no modo
    // desenvolvedor, o "+" que abre os módulos da receita.
    ImGui::SetCursorScreenPos(ImVec2(origin.x, bodyMax.y + ImGui::GetStyle().ItemSpacing.y));
    if (ImGui::SmallButton("Reset"))
        resetGearToRecipe(segmentIndex, model);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("voltar aos valores do equipamento");

    if (m_developerMode)
    {
        ImGui::SameLine();
        if (ImGui::SmallButton("+"))
            m_expandedSegments.insert(segmentIndex);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("ver os modulos por dentro");
    }

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

        // Um rótulo mais largo que a coluna ("Sensitivity" num pedal de
        // duas colunas) encolhe até caber, em vez de invadir o vizinho.
        const float fullLabelWidth = ImGui::CalcTextSize(control.label.c_str()).x;
        const float labelScale = std::min(1.0f, (cellWidth - 6.0f) / std::max(fullLabelWidth, 1.0f));
        ImGui::SetWindowFontScale(labelScale);
        const ImVec2 labelSize = ImGui::CalcTextSize(control.label.c_str());
        ImGui::SetCursorScreenPos(ImVec2(cellCenterX - labelSize.x * 0.5f, cellTop));
        ImGui::TextUnformatted(control.label.c_str());
        ImGui::SetWindowFontScale(1.0f);

        const float knobTop = cellTop + ImGui::GetTextLineHeight() + 4.0f;
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
        ImGui::PushStyleColor(ImGuiCol_Button, bypassed ? ImVec4(0.26f, 0.25f, 0.23f, 1.0f)
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

    ImGui::EndChild();
    ImGui::PopStyleVar();

    if (toggle)
        toggleGearBypass(segmentIndex);
}

void DesktopUI::resetGearToRecipe(std::size_t segmentIndex, const GearModel& model)
{
    // O reset NÃO é resetModule(): isso devolveria cada módulo ao padrão
    // DELE, e não ao da receita deste equipamento.
    const ChainItem& segment = m_chainItems[segmentIndex];
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

void DesktopUI::toggleGearBypass(std::size_t segmentIndex)
{
    // Todos os módulos da receita juntos, senão o "pedal" ficaria meio
    // ligado.
    const ChainItem& segment = m_chainItems[segmentIndex];
    const bool bypassed = m_engine.chain().isBypassed(segment.firstModule);
    for (std::size_t m = 0; m < segment.moduleCount; ++m)
        m_engine.setBypassed(segment.firstModule + m, !bypassed);
}

SDL_Texture* DesktopUI::loadTexture(const std::string& path, float& width, float& height) const
{
    int imageWidth = 0, imageHeight = 0, channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &imageWidth, &imageHeight, &channels, 4);
    if (pixels == nullptr)
        return nullptr;

    SDL_Texture* texture = nullptr;
    if (SDL_Surface* surface = SDL_CreateSurfaceFrom(imageWidth, imageHeight, SDL_PIXELFORMAT_RGBA32,
                                                     pixels, imageWidth * 4))
    {
        texture = SDL_CreateTextureFromSurface(m_renderer, surface);
        SDL_DestroySurface(surface);
    }
    stbi_image_free(pixels);

    if (texture != nullptr)
    {
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
        width = static_cast<float>(imageWidth);
        height = static_cast<float>(imageHeight);
    }
    return texture;
}

const DesktopUI::GearTextures* DesktopUI::gearTextures(const GearSkin& skin)
{
    // Carrega na primeira vez que o equipamento aparece e guarda inclusive
    // a falta de imagem, para não tentar ler o disco de novo a cada quadro.
    auto found = m_gearTextures.find(skin.gearId);
    if (found == m_gearTextures.end())
    {
        const std::string directory = std::string(kGearAssetsDir) + "/" + skin.gearId + "/";
        GearTextures textures;
        float knobWidth = 0.0f, knobHeight = 0.0f;
        textures.body = loadTexture(directory + "body.png", textures.bodyWidth, textures.bodyHeight);
        textures.knob = loadTexture(directory + "knob.png", knobWidth, knobHeight);
        found = m_gearTextures.emplace(skin.gearId, textures).first;
    }

    const GearTextures& textures = found->second;
    return textures.body != nullptr && textures.knob != nullptr ? &textures : nullptr;
}

void DesktopUI::drawSkinnedGear(std::size_t segmentIndex, const GearModel& model, const GearSkin& skin,
                                const GearTextures& textures)
{
    const ChainItem& segment = m_chainItems[segmentIndex];
    const bool bypassed = m_engine.chain().isBypassed(segment.firstModule);

    const float width = skin.displayWidth;
    const float height = width * textures.bodyHeight / textures.bodyWidth;
    const float buttonsHeight = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("gear", ImVec2(width, height + buttonsHeight), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    auto at = [&](GearSkin::Spot spot) {
        return ImVec2(origin.x + spot.x * width, origin.y + spot.y * height);
    };

    drawList->AddImage(textureRef(textures.body), origin, ImVec2(origin.x + width, origin.y + height));

    const float ledRadius = skin.ledDiameter * width * 0.5f;
    auto drawLed = [&](GearSkin::Spot spot, bool lit) {
        SDL_Texture* texture = lit ? m_ledOnTexture : m_ledOffTexture;
        if (texture == nullptr)
            return;
        const ImVec2 center = at(spot);
        drawList->AddImage(textureRef(texture), ImVec2(center.x - ledRadius, center.y - ledRadius),
                           ImVec2(center.x + ledRadius, center.y + ledRadius));
    };
    drawLed(skin.statusLed, !bypassed);
    for (const GearSkin::Spot& spot : skin.unlitLeds)
        drawLed(spot, false);

    const float knobRadius = skin.knobDiameter * width * 0.5f;
    for (std::size_t c = 0; c < model.controls.size(); ++c)
    {
        const GearControl& control = model.controls[c];
        const GearSkin::Knob* skinKnob = nullptr;
        for (const GearSkin::Knob& candidate : skin.knobs)
        {
            if (control.parameterId == candidate.parameterId)
                skinKnob = &candidate;
        }
        if (skinKnob == nullptr)
            continue;

        const std::size_t moduleIndex = segment.firstModule + control.moduleOffset;
        std::size_t parameterIndex = 0, flatIndex = 0;
        if (!flatIndexFor(moduleIndex, control.parameterId, parameterIndex, flatIndex))
            continue;

        ImGui::PushID(static_cast<int>(c));
        const ImVec2 center = at(skinKnob->center);
        ImGui::SetCursorScreenPos(ImVec2(center.x - knobRadius, center.y - knobRadius));

        float value = std::clamp(m_targets[flatIndex], control.minValue, control.maxValue);
        if (knob("##knob", &value, control.minValue, control.maxValue, knobRadius, textures.knob))
        {
            m_targets[flatIndex] = value;
            m_engine.setParameter(moduleIndex, parameterIndex, value);
        }

        // A serigrafia da imagem só tem MIN/MAX; o valor exato aparece ao
        // passar o mouse, na mesma escala de 0 a 10 do painel vetorial.
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetTooltip("%s  %.1f", control.label.c_str(),
                              static_cast<double>(10.0f * (value - control.minValue)
                                                  / (control.maxValue - control.minValue)));
        ImGui::PopID();
    }

    bool toggle = false;
    if (skin.hasFootswitch && m_footswitchTexture != nullptr)
    {
        const ImVec2 center = at(skin.footswitch);
        const float switchWidth = skin.footswitchWidth * width;
        const float switchHeight = switchWidth * m_footswitchHeight / m_footswitchWidth;

        // Ligado: um anel vermelho aceso em volta do footswitch, no lugar
        // de uma segunda imagem do botão.
        if (!bypassed)
        {
            const float ringRadius = switchWidth * 0.52f;
            drawList->AddCircleFilled(center, ringRadius + 6.0f,
                                      ImGui::GetColorU32(ImVec4(1.0f, 0.22f, 0.12f, 0.12f)), 48);
            drawList->AddCircleFilled(center, ringRadius + 2.0f,
                                      ImGui::GetColorU32(ImVec4(1.0f, 0.22f, 0.12f, 0.22f)), 48);
            drawList->AddCircle(center, ringRadius, ImGui::GetColorU32(ImVec4(1.0f, 0.25f, 0.12f, 0.95f)),
                                48, 2.5f);
        }

        const ImVec2 imageMin(center.x - switchWidth * 0.5f, center.y - switchHeight * kFootswitchNutCenter);
        drawList->AddImage(textureRef(m_footswitchTexture), imageMin,
                           ImVec2(imageMin.x + switchWidth, imageMin.y + switchHeight));

        ImGui::SetCursorScreenPos(ImVec2(center.x - switchWidth * 0.5f, center.y - switchWidth * 0.5f));
        toggle = ImGui::InvisibleButton("##footswitch", ImVec2(switchWidth, switchWidth));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(bypassed ? "ligar" : "desligar (bypass)");
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + height + ImGui::GetStyle().ItemSpacing.y));
    if (ImGui::SmallButton("Reset"))
        resetGearToRecipe(segmentIndex, model);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("voltar aos valores do equipamento");
    if (m_developerMode)
    {
        ImGui::SameLine();
        if (ImGui::SmallButton("+"))
            m_expandedSegments.insert(segmentIndex);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("ver os modulos por dentro");
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    if (toggle)
        toggleGearBypass(segmentIndex);
}

void DesktopUI::savePreset(const std::string& name)
{
    try
    {
        std::filesystem::create_directories(kPresetsDir);

        const std::string path = std::string(kPresetsDir) + "/" + name + ".ibifxpreset";

        Preset captured = captureCurrentPreset(name);

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
    trackedCaps(label, kVintageGold, m_brandFont);

    // Uma linha fina dourada só embaixo do título — marca "isto é um
    // cabeçalho" sem cortar a janela em duas metades.
    const ImVec2 textEnd = ImGui::GetItemRectMax();
    const ImVec2 textStart = ImGui::GetItemRectMin();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(textStart.x, textEnd.y + 4.0f), ImVec2(textStart.x + 120.0f, textEnd.y + 4.0f),
        ImGui::GetColorU32(kAccentColor), 1.5f);

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
}

void DesktopUI::drawPresetPanel(bool showHeader)
{
    if (showHeader)
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
    if (!startAudio())
        m_practiceMessage = "erro ao religar o motor: " + m_audioStartError;
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

        const std::string importedId = imported->id;
        insertGearAt(importedId, m_chainItems.size());
        if (std::any_of(m_chainItems.begin(), m_chainItems.end(),
                        [&](const ChainItem& item) { return item.gearId == importedId; }))
            m_rigMessage = "IR importada e inserida: " + destination.filename().string();
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

    // Remove todas as instâncias antes de apagar o arquivo, para um preset
    // salvo em seguida não guardar um caminho de IR que deixou de existir.
    Preset edited = captureCurrentPreset("Cadeia");
    bool changed = false;
    for (std::size_t i = edited.chainItems.size(); i-- > 0;)
    {
        if (edited.chainItems[i].gearId != cabinetId)
            continue;

        std::size_t moduleIndex = 0;
        for (std::size_t before = 0; before < i; ++before)
            moduleIndex += edited.chainItems[before].moduleCount;
        const std::size_t count = edited.chainItems[i].moduleCount;

        edited.modules.erase(edited.modules.begin() + static_cast<std::ptrdiff_t>(moduleIndex),
                             edited.modules.begin() + static_cast<std::ptrdiff_t>(moduleIndex + count));
        edited.chainItems.erase(edited.chainItems.begin() + static_cast<std::ptrdiff_t>(i));
        changed = true;
    }

    if (changed)
    {
        std::string replaceError;
        if (!replaceChain(edited, replaceError))
        {
            m_rigMessage = replaceError;
            return;
        }
        adoptChainLayout(edited);
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

void DesktopUI::drawWorkspaceHeader()
{
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
    ImGui::BeginChild("##workspaceHeader", ImVec2(0.0f, kWorkspaceHeaderHeight), ImGuiChildFlags_Borders);

    if (ImGui::BeginTable("##headerLayout", 3, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("brand", ImGuiTableColumnFlags_WidthStretch, 0.34f);
        ImGui::TableSetupColumn("tuner", ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableSetupColumn("io", ImGuiTableColumnFlags_WidthStretch, 0.41f);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (m_logoTexture != nullptr)
        {
            const float logoHeight = 52.0f;
            ImGui::Image(textureRef(m_logoTexture),
                         ImVec2(m_logoWidth * logoHeight / m_logoHeight, logoHeight));
        }
        else
        {
            ImGui::SetWindowFontScale(1.35f);
            ImGui::TextColored(kIvory, "ibiFX");
            ImGui::SetWindowFontScale(1.0f);
        }
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        trackedCaps("Audio tools", kWarmGray, m_brandFont, 3.0f, 11.0f);
        trackedCaps("for creators", kWarmGray, m_brandFont, 3.0f, 11.0f);
        ImGui::EndGroup();

        ImGui::PushFont(m_brandFont, m_brandFont != nullptr ? kBrandLabelSize : 0.0f);
        const bool pedalboard = m_workspace == Workspace::Pedalboard;
        if (pedalboard)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, kVintageGold);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kVintageGold);
            ImGui::PushStyleColor(ImGuiCol_Text, kCharcoal);
        }
        if (ImGui::Button("PEDALEIRA"))
            m_workspace = Workspace::Pedalboard;
        if (pedalboard)
            ImGui::PopStyleColor(3);

        ImGui::SameLine();
        const bool practice = m_workspace == Workspace::Practice;
        if (practice)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, kVintageGold);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kVintageGold);
            ImGui::PushStyleColor(ImGuiCol_Text, kCharcoal);
        }
        if (ImGui::Button("PRATICA"))
            m_workspace = Workspace::Practice;
        if (practice)
            ImGui::PopStyleColor(3);
        ImGui::PopFont();

        ImGui::TableSetColumnIndex(1);
        if (ImGui::Checkbox("Afinador", &m_tunerOn))
            m_engine.setTunerEnabled(m_tunerOn);
        ImGui::SameLine();
        if (ImGui::Checkbox("Mudo", &m_tunerMuted))
            m_engine.setTunerMuted(m_tunerMuted);

        const Tuner& tuner = m_engine.tuner();
        const bool valid = m_tunerOn && tuner.isValid();
        const float cents = valid ? tuner.centsOff() : 0.0f;
        const ImVec4 noteColor = valid && std::fabs(cents) <= kInTuneCents ? kHealthyColor
                                                                          : ImGui::GetStyleColorVec4(ImGuiCol_Text);
        if (valid)
        {
            ImGui::TextColored(noteColor, "%s", tuner.noteName().c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%.1f Hz  %+.0f cents", static_cast<double>(tuner.frequencyHz()),
                                static_cast<double>(cents));
        }
        else
        {
            ImGui::TextDisabled(m_tunerOn ? "toque uma corda..." : "afinador desligado");
        }

        ImGui::TableSetColumnIndex(2);
        const bool running = m_engine.isRunning();
        ImGui::TextColored(running ? kHealthyColor : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),
                           "%s", running ? "Audio ON" : "Audio OFF");
        ImGui::SameLine();
        ImGui::TextDisabled("%d Hz", static_cast<int>(m_engine.sampleRate() > 0.0
                                                          ? m_engine.sampleRate()
                                                          : m_sampleRate));

        if (!running)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Tentar ligar") && startAudio())
                captureTargets();
        }

        drawMeter("IN", m_engine.inputPeak());
        drawMeter("OUT", m_engine.outputPeak());

        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

void DesktopUI::drawSignalChain()
{
    const ModuleChain& chain = m_engine.chain();
    if (m_selectedItem >= m_chainItems.size())
        m_selectedItem = m_chainItems.empty() ? kNoSelection : 0;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 10.0f));
    ImGui::BeginChild("##signalChain", ImVec2(0.0f, kSignalChainHeight), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    trackedCaps("Signal chain", kVintageGold, m_brandFont);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_currentPresetName.empty() ? "sem preset" : m_currentPresetName.c_str());

    // Tudo na linha é posicionado à mão: IN, slots de inserção, cards e
    // OUT. A escala sai da largura disponível, para nunca precisar de
    // scroll horizontal; quanto mais itens, menores as miniaturas.
    const float inWidth = ImGui::CalcTextSize("IN").x + 8.0f;
    const float outWidth = ImGui::CalcTextSize("OUT").x + 8.0f;
    const float available = ImGui::GetContentRegionAvail().x - inWidth - outWidth;
    const float itemCount = static_cast<float>(m_chainItems.size());
    const float naturalWidth = itemCount * kSignalCardWidth + (itemCount + 1.0f) * kSignalSlotWidth;
    const float scale = std::clamp(available / naturalWidth, kSignalMinScale, 1.0f);

    const float cardWidth = kSignalCardWidth * scale;
    const float cardHeight = kSignalCardHeight * scale;
    const float slotWidth = kSignalSlotWidth * scale;

    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    const float rowHeight = ImGui::GetContentRegionAvail().y;
    const float cardTop = rowStart.y + std::max(0.0f, (rowHeight - cardHeight) * 0.5f);
    // O cabo passa na altura do meio da miniatura.
    const float wireY = cardTop + (10.0f + kSignalThumbHeight * 0.5f) * scale;
    const float rowEnd = rowStart.x + inWidth + naturalWidth * scale + outWidth;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 wireColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    drawList->AddLine(ImVec2(rowStart.x + inWidth - 4.0f, wireY), ImVec2(rowEnd - outWidth + 4.0f, wireY),
                      wireColor, 1.0f);

    const float textHeight = ImGui::GetTextLineHeight();
    drawList->AddText(ImVec2(rowStart.x, wireY - textHeight * 0.5f), wireColor, "IN");
    drawList->AddText(ImVec2(rowEnd - outWidth + 8.0f, wireY - textHeight * 0.5f), wireColor, "OUT");

    std::string droppedGearId;
    std::size_t dropIndex = kNoSelection;
    std::size_t draggedItem = kNoSelection;
    float x = rowStart.x + inWidth;

    auto drawDropSlot = [&](std::size_t insertionIndex) {
        ImGui::SetCursorScreenPos(ImVec2(x, cardTop));
        ImGui::PushID(static_cast<int>(insertionIndex));

        ImGui::InvisibleButton("##insert", ImVec2(slotWidth, cardHeight));
        const bool hovered = ImGui::IsItemHovered()
                             || (ImGui::GetDragDropPayload() != nullptr
                                 && ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));
        if (hovered)
        {
            const float centerX = x + slotWidth * 0.5f;
            drawList->AddLine(ImVec2(centerX, cardTop + 4.0f), ImVec2(centerX, cardTop + cardHeight - 4.0f),
                              ImGui::GetColorU32(kAccentColor), 2.0f);
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kGearDragPayload))
            {
                droppedGearId = static_cast<const char*>(payload->Data);
                dropIndex = insertionIndex;
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kChainItemDragPayload))
            {
                if (payload->DataSize == static_cast<int>(sizeof(std::size_t)))
                {
                    draggedItem = *static_cast<const std::size_t*>(payload->Data);
                    dropIndex = insertionIndex;
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();
        x += slotWidth;
    };

    drawDropSlot(0);

    for (std::size_t i = 0; i < m_chainItems.size(); ++i)
    {
        const ChainItem& item = m_chainItems[i];
        const GearModel* model = item.gearId.empty() ? nullptr : m_library.find(item.gearId);
        const char* rawName = item.firstModule < chain.size() ? chain.moduleAt(item.firstModule).name() : "Modulo";
        const std::string title = model != nullptr ? model->name : rawName;
        const SignalCardKind kind = model != nullptr ? signalCardKind(model->category)
                                                     : SignalCardKind::Module;

        const GearSkin* skin = model != nullptr ? findGearSkin(model->id) : nullptr;
        const GearTextures* textures = skin != nullptr ? gearTextures(*skin) : nullptr;

        ImGui::SetCursorScreenPos(ImVec2(x, cardTop));
        ImGui::PushID(static_cast<int>(i));
        if (signalCard("##item", title, kind, colorForModuleName(title.c_str(), false),
                       m_selectedItem == i, scale, textures != nullptr ? textures->body : nullptr,
                       textures != nullptr ? textures->bodyWidth / textures->bodyHeight : 1.0f))
            m_selectedItem = i;

        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(kChainItemDragPayload, &i, sizeof(i));
            ImGui::Text("Mover %s", title.c_str());
            ImGui::EndDragDropSource();
        }
        ImGui::PopID();
        x += cardWidth;

        drawDropSlot(i + 1);
    }

    if (m_chainItems.empty())
    {
        const char* hint = "Cadeia vazia: arraste um equipamento da biblioteca para cá.";
        drawList->AddText(ImVec2(rowStart.x + inWidth + slotWidth + 12.0f, wireY - textHeight - 6.0f),
                          wireColor, hint);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    if (!droppedGearId.empty() && dropIndex != kNoSelection)
        insertGearAt(droppedGearId, dropIndex);
    else if (draggedItem != kNoSelection && dropIndex != kNoSelection)
        moveChainItem(draggedItem, dropIndex);
}

void DesktopUI::drawSelectedEquipmentEditor()
{
    const ModuleChain& chain = m_engine.chain();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 16.0f));
    // Deixa embaixo o espaço da signal chain, que vem depois do editor.
    const float editorHeight = std::max(160.0f, ImGui::GetContentRegionAvail().y - kSignalChainHeight
                                                     - ImGui::GetStyle().ItemSpacing.y);
    ImGui::BeginChild("##selectedEditor", ImVec2(0.0f, editorHeight), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);

    trackedCaps("Equipamento", kVintageGold, m_brandFont);
    {
        // Detalhe da marca no canto do editor.
        const ImVec2 windowMin = ImGui::GetWindowPos();
        drawBrandStripes(ImGui::GetWindowDrawList(),
                         ImVec2(windowMin.x + ImGui::GetWindowWidth() - 180.0f, windowMin.y + 18.0f), 12.0f,
                         ImGui::GetColorU32(ImVec4(kVintageGold.x, kVintageGold.y, kVintageGold.z, 0.55f)));
    }
    bool removeSelected = false;
    if (m_selectedItem < m_chainItems.size())
    {
        // Alinhado à direita pela largura real do botão; um valor fixo
        // menor que ele empurrava o conteúdo e abria um scroll horizontal.
        const float removeWidth = ImGui::CalcTextSize("Remover").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - removeWidth);
        if (ImGui::SmallButton("Remover"))
            removeSelected = true;
    }
    ImGui::Separator();
    ImGui::Spacing();

    if (m_selectedItem < m_chainItems.size())
    {
        const ChainItem& segment = m_chainItems[m_selectedItem];
        const GearModel* model = m_library.find(segment.gearId);
        const bool raw = model == nullptr || model->controls.empty()
                         || m_expandedSegments.count(m_selectedItem) != 0;

        if (raw)
        {
            if (model != nullptr && !model->controls.empty())
            {
                if (ImGui::Button("Voltar ao painel"))
                    m_expandedSegments.erase(m_selectedItem);
                ImGui::Spacing();
            }

            for (std::size_t k = 0; k < segment.moduleCount; ++k)
            {
                const std::size_t moduleIndex = segment.firstModule + k;
                if (moduleIndex >= chain.size())
                    break;
                ImGui::PushID(static_cast<int>(moduleIndex));
                drawPedal(moduleIndex, firstFlatIndexForModule(moduleIndex));
                ImGui::PopID();
                if (k + 1 < segment.moduleCount)
                    ImGui::SameLine();
            }
        }
        else
        {
            // Com imagens, a skin; sem, o painel vetorial.
            const GearSkin* skin = findGearSkin(model->id);
            const GearTextures* textures = skin != nullptr ? gearTextures(*skin) : nullptr;

            const float width = textures != nullptr ? skin->displayWidth : gearPanelWidth(*model);
            if (width < ImGui::GetContentRegionAvail().x)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - width) * 0.5f);
            ImGui::PushID(static_cast<int>(m_selectedItem));
            if (textures != nullptr)
                drawSkinnedGear(m_selectedItem, *model, *skin, *textures);
            else
                drawGearPanel(m_selectedItem, *model);
            ImGui::PopID();
        }
    }
    else
    {
        ImGui::TextDisabled("Selecione uma miniatura na cadeia para editar o equipamento.");
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    if (removeSelected)
        removeChainItem(m_selectedItem);
}

void DesktopUI::drawPedalboardWorkspace()
{
    const float mainWidth = std::max(360.0f, ImGui::GetContentRegionAvail().x - kBrowserWidth
                                              - ImGui::GetStyle().ItemSpacing.x);
    ImGui::BeginChild("##pedalboardMain", ImVec2(mainWidth, 0.0f));
    drawSelectedEquipmentEditor();
    drawSignalChain();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
    ImGui::BeginChild("##browser", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    drawGearBrowser();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

void DesktopUI::drawPracticeTimeline()
{
    PracticeSession& session = m_engine.practiceSession();
    const double rate = m_engine.sampleRate() > 0.0 ? m_engine.sampleRate() : m_sampleRate;
    const std::uint64_t length = session.hasBackingTrack() ? session.backingTrackLengthSamples() : 0;
    const std::uint64_t position = session.positionSamples();

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
    ImGui::BeginChild("##practiceTimeline", ImVec2(0.0f, 190.0f), ImGuiChildFlags_Borders);

    ImGui::TextUnformatted("Timeline de pratica");
    ImGui::SameLine();
    ImGui::TextDisabled("backing track, loop A/B e agulha");

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size(ImGui::GetContentRegionAvail().x, 106.0f);
    ImGui::InvisibleButton("##timeline", size);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 end(origin.x + size.x, origin.y + size.y);

    drawList->AddRectFilled(origin, end, ImGui::GetColorU32(kSurfaceRaised), 6.0f);
    drawList->AddRect(origin, end, ImGui::GetColorU32(ImGuiCol_Separator), 6.0f);
    drawList->AddText(ImVec2(origin.x + 12.0f, origin.y + 12.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                      m_backingTrackPath.empty() ? "BACKING TRACK" : compactLabel(std::filesystem::path(m_backingTrackPath).filename().string(), 42).c_str());
    drawList->AddText(ImVec2(origin.x + 12.0f, origin.y + 66.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                      "LOOP A/B");

    const float laneLeft = origin.x + 132.0f;
    const float laneRight = end.x - 16.0f;
    drawList->AddLine(ImVec2(laneLeft, origin.y + 33.0f), ImVec2(laneRight, origin.y + 33.0f),
                      ImGui::GetColorU32(ImGuiCol_Separator), 12.0f);
    drawList->AddLine(ImVec2(laneLeft, origin.y + 82.0f), ImVec2(laneRight, origin.y + 82.0f),
                      ImGui::GetColorU32(ImGuiCol_Separator), 8.0f);

    if (length > 0 && laneRight > laneLeft)
    {
        const float progress = std::clamp(static_cast<float>(position) / static_cast<float>(length), 0.0f, 1.0f);
        const float playheadX = laneLeft + (laneRight - laneLeft) * progress;
        drawList->AddLine(ImVec2(playheadX, origin.y + 18.0f), ImVec2(playheadX, end.y - 10.0f),
                          ImGui::GetColorU32(kAccentColor), 2.0f);

        if (m_loopEnd > m_loopStart)
        {
            const float loopA = std::clamp(static_cast<float>(m_loopStart) / static_cast<float>(length), 0.0f, 1.0f);
            const float loopB = std::clamp(static_cast<float>(m_loopEnd) / static_cast<float>(length), 0.0f, 1.0f);
            drawList->AddRectFilled(ImVec2(laneLeft + (laneRight - laneLeft) * loopA, origin.y + 76.0f),
                                    ImVec2(laneLeft + (laneRight - laneLeft) * loopB, origin.y + 88.0f),
                                    ImGui::GetColorU32(kAccentColor), 3.0f);
        }

        if (ImGui::IsItemClicked())
        {
            const float clicked = std::clamp((ImGui::GetIO().MousePos.x - laneLeft) / (laneRight - laneLeft), 0.0f, 1.0f);
            session.seek(static_cast<std::uint64_t>(clicked * static_cast<float>(length)));
        }
    }
    else
    {
        drawList->AddText(ImVec2(laneLeft, origin.y + 24.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                          "carregue uma faixa para usar a agulha e o scrub");
    }

    const auto seconds = [rate](std::uint64_t samples) { return static_cast<double>(samples) / rate; };
    ImGui::TextDisabled("%s / %s", formatClock(seconds(position)).c_str(),
                        length > 0 ? formatClock(seconds(length)).c_str() : "00:00");

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

void DesktopUI::drawPracticeWorkspace()
{
    drawPracticeTimeline();
    ImGui::Spacing();

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float controlsWidth = ImGui::GetContentRegionAvail().x * 0.62f;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
    ImGui::BeginChild("##practiceControls", ImVec2(controlsWidth, 0.0f), ImGuiChildFlags_Borders);
    drawPracticePanel();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, spacing);
    ImGui::BeginChild("##practiceTools", ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##practiceTuner", ImVec2(0.0f, 255.0f), ImGuiChildFlags_Borders);
    drawTunerPanel();
    ImGui::EndChild();
    ImGui::Spacing();
    ImGui::BeginChild("##practiceRecorder", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    drawRecorderPanel();
    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

void DesktopUI::drawFrame()
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGui::Begin("IbiFX", nullptr,
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                      | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    drawWorkspaceHeader();
    ImGui::Spacing();

    if (m_workspace == Workspace::Pedalboard)
        drawPedalboardWorkspace();
    else
        drawPracticeWorkspace();

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

    if (!startAudio())
        std::cerr << "erro: " << m_audioStartError << "\n";

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
        SDL_SetRenderDrawColor(m_renderer, 17, 19, 21, 255);   // Charcoal
        SDL_RenderClear(m_renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_renderer);
        SDL_RenderPresent(m_renderer);
    }

    // Fechar a janela no meio de uma gravação não pode perder o arquivo.
    stopAllRecordings();

    m_engine.stop();
    shutdownWindow();

    return m_audioStartError.empty() ? 0 : 1;
}
