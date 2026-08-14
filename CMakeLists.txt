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

# O executável desta etapa.
add_executable(ibifx
    src/main.cpp
)

# Warnings e conformidade.
# PRIVATE = as flags valem para compilar este alvo, e não se propagam
# para quem futuramente vier a depender dele.
#
# No MSVC (Visual Studio / clang-cl):
#   /W4         nível de warning equivalente a -Wall -Wextra
#   /utf-8      trata os arquivos-fonte como UTF-8. Sem isto o MSVC usa a
#               codepage do sistema (1252 no Windows brasileiro), emite C4819
#               e pode corromper os acentos dos nossos comentários.
#   /permissive- modo de conformidade com o padrão C++. É o equivalente
#               conceitual do CMAKE_CXX_EXTENSIONS OFF definido acima.
if(MSVC)
    target_compile_options(ibifx PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(ibifx PRIVATE -Wall -Wextra -Wpedantic)
endif()
