#include "DesktopUI.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>

namespace
{
constexpr int kWindowWidth = 640;
constexpr int kWindowHeight = 720;

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

    m_window = SDL_CreateWindow("IbiFX", kWindowWidth, kWindowHeight, 0);

    if (m_window == nullptr)
    {
        std::cerr << "erro: SDL_CreateWindow falhou: " << SDL_GetError() << "\n";
        return false;
    }

    m_renderer = SDL_CreateRenderer(m_window, nullptr);

    if (m_renderer == nullptr)
    {
        std::cerr << "erro: SDL_CreateRenderer falhou: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_SetRenderVSync(m_renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Sem ibifx.ini: a janela é fixa, nao ha layout de paineis pra lembrar
    // entre execucoes, e gravar um arquivo por engano no diretorio de
    // trabalho seria uma surpresa desagradavel.
    ImGui::GetIO().IniFilename = nullptr;

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

void DesktopUI::drawFrame()
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGui::Begin("IbiFX", nullptr,
                  ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::TextUnformatted(m_engine.isRunning() ? "tocando" : "parado");
    ImGui::Text("%s   %d Hz", m_engine.deviceName().c_str(), static_cast<int>(m_engine.sampleRate()));

    ImGui::Separator();
    drawMeter("entrada", m_engine.inputPeak());
    drawMeter("saida", m_engine.outputPeak());
    ImGui::Separator();

    const ModuleChain& chain = m_engine.chain();
    std::size_t flat = 0;

    for (std::size_t i = 0; i < chain.size(); ++i)
    {
        const AudioModule& module = chain.moduleAt(i);
        const std::size_t firstFlat = flat;
        flat += module.parameterCount();

        // Escopa os ids dos widgets deste modulo (checkbox, botao, cada
        // slider), pra dois modulos do mesmo tipo (dois Gain, por exemplo)
        // nao colidirem so por terem o mesmo name()/label().
        ImGui::PushID(static_cast<int>(i));

        if (ImGui::CollapsingHeader(module.name(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool bypassed = chain.isBypassed(i);

            if (ImGui::Checkbox("bypass", &bypassed))
                m_engine.setBypassed(i, bypassed);

            ImGui::SameLine();

            if (ImGui::Button("reset"))
            {
                m_engine.resetModule(i);
                captureModuleTargets(i, firstFlat);
            }

            for (std::size_t p = 0; p < module.parameterCount(); ++p)
            {
                const Parameter& parameter = module.parameterAt(p);
                const std::size_t index = firstFlat + p;

                float value = m_targets[index];

                // Desenha o ALVO local, nao o parameter.value() lido agora
                // — ver o comentario de m_targets no header.
                if (ImGui::SliderFloat(parameter.label().c_str(), &value,
                                        parameter.minValue(), parameter.maxValue()))
                {
                    m_targets[index] = value;
                    m_engine.setParameter(i, p, value);
                }
            }
        }

        ImGui::PopID();
    }

    ImGui::End();
}

int DesktopUI::run(AudioDevice::Mode mode, double sampleRate, int blockSize)
{
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
