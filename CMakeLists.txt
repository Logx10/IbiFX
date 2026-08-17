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
if(MSVC)
    set(IBIFX_WARNINGS /W4 /utf-8 /permissive-)
else()
    set(IBIFX_WARNINGS -Wall -Wextra -Wpedantic)
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
    src/ModuleChain.cpp
    src/AudioModule.cpp
    src/Parameter.cpp
    src/Delay.cpp
    src/SmoothedValue.cpp
    src/WavFile.cpp
    src/offline.cpp
)

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

# O executável: apenas o ponto de entrada, mais a biblioteca.
add_executable(ibifx
    src/main.cpp
)

target_compile_options(ibifx PRIVATE ${IBIFX_WARNINGS})
target_link_libraries(ibifx PRIVATE ibifx_core)

# Liga o CTest, o executor de testes que vem junto com o CMake.
# Precisa vir ANTES do add_subdirectory abaixo, senão os add_test()
# de lá dentro não são registrados.
enable_testing()

# Lê o CMakeLists.txt da pasta tests/.
add_subdirectory(tests)
