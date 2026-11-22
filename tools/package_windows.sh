#!/usr/bin/env bash
# Monta o pacote de distribuição do IbiFX para Windows: um .zip com o
# executável e só os dados que ele precisa para abrir e soar.
#
# Uso (da raiz do projeto, no Git Bash ou MSYS2):
#     tools/package_windows.sh
#
# Resultado: dist/IbiFX-<versão>-windows-x64/ e o .zip ao lado.
#
# O QUE ENTRA
#   IbiFX.exe           build Release sem janela de console
#                       (IBIFX_DESKTOP_NO_CONSOLE), runtime estático
#   assets/             logo, fonte da marca e skins dos equipamentos
#   presets/factory/    os presets de fábrica
#   irs/Factory/        as IRs sintéticas do IbiFX (licença nossa)
#   audio/, recordings/ vazias, para as faixas de prática e as gravações
#   LICENSE, LEIA-ME.txt, THIRD_PARTY_NOTICES.txt
#
# O QUE NÃO ENTRA (de propósito)
#   - IRs de terceiros (irs/Celestion..., irs/Importados): a licença
#     delas não permite redistribuir; o LEIA-ME explica onde baixar.
#   - presets pessoais da raiz de presets/ e os .wav de teste de audio/.
#
# Build num diretório próprio (build-dist/), para não trocar as opções do
# build-release usado no dia a dia. As fontes de SDL3 e Dear ImGui são
# reaproveitadas de build/_deps quando existem, então não precisa de rede.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

VERSION="$(sed -n 's/^ *VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt | head -n1)"
NAME="IbiFX-${VERSION}-windows-x64"
BUILD_DIR="build-dist"
STAGE="dist/${NAME}"

configure_args=(
    -S . -B "$BUILD_DIR" -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DIBIFX_BUILD_DESKTOP=ON
    -DIBIFX_DESKTOP_NO_CONSOLE=ON
)
# O windres (ícone e versão do exe) tem que ser o da mesma toolchain do
# compilador: o de outra instalação de MinGW no PATH não acha o próprio
# pré-processador e falha com "preprocessing failed".
if [[ -n "${CXX:-}" && -x "$(dirname "$CXX")/windres.exe" ]]; then
    configure_args+=(-DCMAKE_RC_COMPILER="$(dirname "$CXX")/windres.exe")
fi
[[ -d build/_deps/sdl3-src ]] && configure_args+=(-DFETCHCONTENT_SOURCE_DIR_SDL3="$ROOT/build/_deps/sdl3-src")
[[ -d build/_deps/imgui-src ]] && configure_args+=(-DFETCHCONTENT_SOURCE_DIR_IMGUI="$ROOT/build/_deps/imgui-src")

echo "==> configurando $BUILD_DIR"
cmake "${configure_args[@]}" > /dev/null
echo "==> compilando ibifx_desktop (Release, sem console)"
cmake --build "$BUILD_DIR" --target ibifx_desktop

echo "==> montando $STAGE"
rm -rf "$STAGE" "dist/${NAME}.zip"
mkdir -p "$STAGE"/{presets,irs,audio,recordings}

cp "$BUILD_DIR/ibifx_desktop.exe" "$STAGE/IbiFX.exe"
strip "$STAGE/IbiFX.exe" 2> /dev/null || true

mkdir -p "$STAGE/assets"
cp -r assets/brand assets/fonts assets/gear "$STAGE/assets/"
rm -f "$STAGE/assets/brand/ibifx.ico"
cp -r presets/factory "$STAGE/presets/"
cp -r irs/Factory "$STAGE/irs/"
cp LICENSE "$STAGE/LICENSE.txt"
cp tools/dist/LEIA-ME.txt tools/dist/THIRD_PARTY_NOTICES.txt "$STAGE/"
printf 'Coloque aqui faixas .wav para praticar (aba PRATICA).\r\n' > "$STAGE/audio/coloque-suas-faixas-aqui.txt"
printf 'As gravacoes do IbiFX aparecem aqui.\r\n' > "$STAGE/recordings/gravacoes-aparecem-aqui.txt"

# O exe não pode depender de DLL nenhuma além das do próprio Windows.
if command -v objdump > /dev/null; then
    deps="$(objdump -p "$STAGE/IbiFX.exe" | sed -n 's/^\s*DLL Name: //p' | tr 'A-Z' 'a-z' | sort -u)"
    unexpected="$(grep -v -E '^(kernel32|user32|gdi32|shell32|ole32|oleaut32|advapi32|winmm|imm32|version|setupapi|cfgmgr32|msvcrt|ucrtbase|api-ms-win-.*|ws2_32|dwmapi|uxtheme|hid|comdlg32|shlwapi|winspool|rpcrt4|bcrypt|uuid|avrt|dbghelp)\.dll$' <<< "$deps" || true)"
    if [[ -n "$unexpected" ]]; then
        echo "ERRO: o exe depende de DLLs que não vão no pacote:" >&2
        echo "$unexpected" >&2
        exit 1
    fi
fi

echo "==> compactando"
powershell -NoProfile -Command \
    "Compress-Archive -Path '$(cygpath -w "$STAGE")' -DestinationPath '$(cygpath -w "dist/${NAME}.zip")' -Force"

echo
echo "pronto: dist/${NAME}.zip ($(du -h "dist/${NAME}.zip" | cut -f1))"
