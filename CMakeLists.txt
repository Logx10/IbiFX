# Versão mínima do CMake exigida.
# 3.20 é um piso seguro e moderno: garante suporte estável a C++20
# e ao comportamento de "target-based" que usamos abaixo.
cmake_minimum_required(VERSION 3.20)

# Nome, versão e linguagens do projeto.
# LANGUAGES CXX evita que o CMake procure um compilador C sem necessidade.
project(IbiFX
    VERSION 0.0.1
    LANGUAGES CXX
)

# Padrão da linguagem.
# STANDARD_REQUIRED ON  -> falha o configure se o compilador não suportar C++20
#                          (em vez de silenciosamente cair para C++17).
# EXTENSIONS OFF        -> usa -std=c++20 e não -std=gnu++20, mantendo o
#                          código portável entre compiladores.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Gera compile_commands.json no diretório de build.
# É o arquivo que clangd, VS Code e CLion usam para entender flags de
# compilação e oferecer navegação/diagnóstico corretos.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Geradores single-config (Makefiles, Ninja) não têm build type padrão.
# Sem isto, o build sairia sem otimização e sem símbolos de debug.
# Geradores multi-config (Xcode, Visual Studio) escolhem no momento do build,
# por isso a condição os exclui.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Flags de warning, guardadas numa variável.
#
# Agora existe mais de um alvo (a biblioteca, o executável e o teste), e os
# três devem ser compilados com o mesmo rigor. Repetir a lista em cada um
# convidaria ao esquecimento.
#
# No MSVC (Visual Studio / clang-cl):
#   /W4         nível de warning equivalente a -Wall -Wextra
#   /utf-8      trata os arquivos-fonte como UTF-8. Sem isto o MSVC usa a
#               codepage do sistema (1252 no Windows brasileiro), emite C4819
#               e pode corromper os acentos dos nossos comentários.
#   /permissive- modo de conformidade com o padrão C++. É o equivalente
#               conceitual do CMAKE_CXX_EXTENSIONS OFF definido acima.
# Biblioteca de threads do sistema.
#
# THREADS_PREFER_PTHREAD_FLAG faz o CMake usar -pthread em vez de -lpthread
# no Linux, que e a forma correta: a flag afeta compilacao e link, nao apenas
# o link. No macOS e no Windows o alvo resolve para nada, porque o suporte a
# threads ja vem embutido no compilador.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(MSVC)
    set(IBIFX_WARNINGS /W4 /utf-8 /permissive-)
else()
    set(IBIFX_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# Liga o frontend desktop (Dear ImGui + SDL3, ver docs/adr/0001).
#
# Desligado, o configure não baixa nada: nem SDL3 nem Dear ImGui são
# tocados, e ibifx_core/ibifx_platform/ibifx/testes compilam exatamente
# como antes. É a válvula de escape para quem quer só o CLI, ou para
# configurar sem rede.
option(IBIFX_BUILD_DESKTOP "Compilar o frontend desktop (Dear ImGui + SDL3, via FetchContent)" ON)

# miniaudio — a camada de plataforma para falar com o dispositivo de áudio.
#
# Vendorizada como um único header em third_party/, versão 0.11.25, domínio
# público (ou MIT-0, à escolha). Não precisa ser instalada nem baixada no
# build: está no repositório e compila junto.
#
# ALVO SEPARADO, DE PROPÓSITO
# Este é código de terceiros e não segue o nosso padrão. Compilá-lo sob
# -Wall -Wextra -Wpedantic produziria centenas de avisos que não podemos
# corrigir e que esconderiam os nossos. Por isso ele fica num alvo próprio,
# SEM as flags do IbiFX.
#
# O include é PUBLIC para que quem depende deste alvo enxergue o header, e é
# marcado como SYSTEM para que o compilador também silencie os avisos vindos
# de dentro dele.
add_library(ibifx_miniaudio STATIC
    src/platform/miniaudio_impl.cpp
)

target_include_directories(ibifx_miniaudio SYSTEM PUBLIC third_party/miniaudio)

# O miniaudio precisa das bibliotecas de áudio do sistema. No macOS são
# frameworks do CoreAudio; no Linux, dl/pthread/math; no Windows ele resolve
# sozinho pelas bibliotecas padrão.
if(APPLE)
    target_link_libraries(ibifx_miniaudio PUBLIC
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )
elseif(UNIX)
    target_link_libraries(ibifx_miniaudio PUBLIC ${CMAKE_DL_LIBS} m Threads::Threads)
endif()

# A biblioteca com o código de verdade do IbiFX.
#
# POR QUE UMA BIBLIOTECA, E NÃO SÓ O EXECUTÁVEL
# Dois programas precisam do GainProcessor: o `ibifx` e o teste. Sem uma
# biblioteca, cada um teria de listar src/GainProcessor.cpp na sua própria
# lista de fontes — e todo arquivo novo teria de ser lembrado em dois
# lugares. Uma vez esquecido, o erro aparece só no linker, longe da causa.
#
# STATIC = o código é copiado para dentro de quem usa, na hora do link.
# Não gera .dll/.so, não há arquivo extra para distribuir. É o padrão
# sensato enquanto tudo é um programa só.
#
# Cada .cpp listado aqui vira um .o próprio; o linker costura depois.
# Headers (.h) NÃO entram: eles não são compilados sozinhos, são colados
# dentro dos .cpp pelo pré-processador, via #include.
add_library(ibifx_core STATIC
    src/GainProcessor.cpp
    src/Clipper.cpp
    src/SoftClipper.cpp
    src/Limiter.cpp
    src/NoiseGate.cpp
    src/Compressor.cpp
    src/Reverb.cpp
    src/AsymmetricClipper.cpp
    src/ToneStack.cpp
    src/Preamp.cpp
    src/PowerAmp.cpp
    src/Tuner.cpp
    src/IRLoader.cpp
    src/ConvolutionEngine.cpp
    src/Cabinet.cpp
    src/ModuleChain.cpp
    src/AudioModule.cpp
    src/Parameter.cpp
    src/Delay.cpp
    src/SmoothedValue.cpp
    src/WavFile.cpp
    src/offline.cpp
    src/AudioGraph.cpp
    src/CommandQueue.cpp
    src/HighPassFilter.cpp
    src/Preset.cpp
    src/Serialization.cpp
    src/PresetManager.cpp
    src/PedalboardChain.cpp
    src/ChainArgs.cpp
    src/MidiMapping.cpp
    src/MasterTransport.cpp
    src/Metronome.cpp
    src/BackingTrackPlayer.cpp
    src/Recorder.cpp
    src/ReampRecorder.cpp
)

# A camada de plataforma, separada do core.
#
# O princípio 1 do ARCHITECTURE pede que o núcleo não conheça sistema
# operacional nem driver. Manter isto num alvo próprio torna a regra
# verificável: se alguém incluir miniaudio.h dentro do core, o build quebra,
# porque o ibifx_core não enxerga esse diretório.
add_library(ibifx_platform STATIC
    src/platform/AudioDevice.cpp
    src/platform/LiveEngine.cpp
    src/platform/Terminal.cpp
)

target_include_directories(ibifx_platform PUBLIC src/platform)
target_compile_options(ibifx_platform PRIVATE ${IBIFX_WARNINGS})
target_link_libraries(ibifx_platform PUBLIC ibifx_core PRIVATE ibifx_miniaudio)

# A interface de terminal. É uma CAMADA sobre o engine, como pede o princípio
# 2 do ARCHITECTURE: ela lê estado para desenhar e envia comandos para
# alterar, sem calcular nada de áudio. Apagar este alvo não afetaria o DSP.
add_library(ibifx_tui STATIC
    src/tui/PedalboardUI.cpp
)

target_include_directories(ibifx_tui PUBLIC src/tui)
target_compile_options(ibifx_tui PRIVATE ${IBIFX_WARNINGS})
target_link_libraries(ibifx_tui PUBLIC ibifx_platform)

# Onde procurar os headers desta biblioteca.
#
# PUBLIC, e não PRIVATE: quem usar a ibifx_core herda este diretório
# automaticamente. É o que permite ao teste escrever #include "GainProcessor.h"
# sem saber em que pasta o arquivo mora.
target_include_directories(ibifx_core PUBLIC src)

# PRIVATE = as flags valem para compilar este alvo e não se propagam
# para quem depende dele. Warnings são assunto de quem escreve o código,
# não de quem o consome.
target_compile_options(ibifx_core PRIVATE ${IBIFX_WARNINGS})

# O Recorder (Fase 17) sobe sua própria thread de disco — PUBLIC para que
# quem ligar ibifx_core (inclusive os testes) herde o link, em vez de cada
# um redescobrir que precisa de Threads::Threads por conta própria.
target_link_libraries(ibifx_core PUBLIC Threads::Threads)

# O executável: apenas o ponto de entrada, mais a biblioteca.
add_executable(ibifx
    src/main.cpp
)

target_compile_options(ibifx PRIVATE ${IBIFX_WARNINGS})
# ibifx_platform ja arrasta ibifx_core como dependencia PUBLIC.
target_link_libraries(ibifx PRIVATE ibifx_tui)

# O frontend desktop (Fase 12, AI_GUIDELINES §36) — Dear ImGui + SDL3, a
# escolha registrada em docs/adr/0001-portabilidade-desktop-e-web.md: é a
# única dupla em que o mesmo código C++ de interface roda numa janela
# desktop e, mais tarde, no navegador via Emscripten.
#
# POR QUE FetchContent, E NÃO VENDORIZAR COMO O MINIAUDIO
# O miniaudio é um header único; SDL3 é uma biblioteca grande, com backend
# por sistema operacional. Vendorizá-la infla o repositório em dezenas de
# MB. FetchContent baixa o código-fonte na hora do configure (cacheado
# depois em build/_deps) e compila junto — o repositório continua leve, e
# o build continua em um comando só, ao custo de precisar de rede na
# primeira configuração.
if(IBIFX_BUILD_DESKTOP)
    include(FetchContent)

    # Builds estático, sem os exemplos/testes do próprio SDL3 — nada disso
    # nos interessa, e ligar isso tudo deixaria o configure bem mais lento.
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.16
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)

    # Dear ImGui não publica CMakeLists.txt próprio — FetchContent só
    # baixa o fonte, e a lib é montada à mão aqui, no mesmo espírito do
    # ibifx_miniaudio: alvo próprio, sem os warnings do IbiFX aplicados a
    # código de terceiros, includes marcadas SYSTEM para o compilador
    # silenciar os avisos de dentro dele também.
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.7
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
    )

    target_include_directories(imgui SYSTEM PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )

    target_link_libraries(imgui PUBLIC SDL3::SDL3)

    # A camada de UI desktop, espelhando como ibifx_tui embrulha a
    # PedalboardUI: um alvo próprio, com o nosso rigor de warnings, que
    # depende de ibifx_platform (LiveEngine/AudioDevice) e do imgui.
    add_library(ibifx_gui STATIC
        src/gui/DesktopUI.cpp
    )

    target_include_directories(ibifx_gui PUBLIC src/gui)
    target_compile_options(ibifx_gui PRIVATE ${IBIFX_WARNINGS})
    target_link_libraries(ibifx_gui PUBLIC ibifx_platform imgui)

    add_executable(ibifx_desktop
        apps/desktop/main.cpp
    )

    target_compile_options(ibifx_desktop PRIVATE ${IBIFX_WARNINGS})
    target_link_libraries(ibifx_desktop PRIVATE ibifx_gui)
endif()

# Liga o CTest, o executor de testes que vem junto com o CMake.
# Precisa vir ANTES do add_subdirectory abaixo, senão os add_test()
# de lá dentro não são registrados.
enable_testing()

# Lê o CMakeLists.txt da pasta tests/.
add_subdirectory(tests)
