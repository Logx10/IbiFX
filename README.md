# IbiFX

> Modular real-time guitar effects and practice platform built with C++.

O IbiFX pretende se tornar uma plataforma modular de processamento de áudio em
tempo real, voltada inicialmente para guitarra, baixo e outros instrumentos
elétricos.

## Estado atual

**Estágio inicial.** No momento o repositório contém apenas:

- a configuração de build com CMake;
- um executável mínimo que imprime `IbiFX starting...`;
- a documentação inicial de arquitetura e roadmap.

Nada de áudio, DSP ou interface gráfica foi implementado ainda.

## Objetivos futuros

As funcionalidades abaixo são **planejadas**, não implementadas:

- processamento de áudio em tempo real;
- arquitetura modular de efeitos;
- pedais virtuais (gate, compressor, overdrive, delay, reverb, ...);
- simulação de amplificador;
- cabinet simulation via Impulse Responses (IR);
- presets;
- controle por MIDI;
- metrônomo;
- backing tracks;
- gravação.

## Requisitos

- CMake 3.20 ou superior;
- um compilador com suporte a C++20 (Clang, GCC ou MSVC recentes).

O projeto não tem dependências externas.

## Como compilar

### macOS / Linux

```sh
cmake -S . -B build
cmake --build build
./build/ibifx
```

### Windows — Visual Studio (MSVC)

```bat
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\ibifx.exe
```

O gerador do Visual Studio é **multi-config**: ele produz um único conjunto de
arquivos de projeto que serve para Debug e Release, e a escolha acontece no
momento do build. Por isso o `--config Debug` é obrigatório aqui e o binário sai
em um subdiretório com o nome da configuração.

### Windows — Ninja ou MinGW

```bat
cmake -S . -B build -G Ninja
cmake --build build
.\build\ibifx.exe
```

Estes geradores são **single-config**: a configuração é escolhida no momento do
`cmake -S . -B build` (o projeto usa `Debug` por padrão) e o executável fica
direto em `build/`.

## Saída esperada

```text
IbiFX starting...
```

## Nota sobre `build/`

O diretório `build/` é descartável e não é versionado. Apagá-lo e reconfigurar é
sempre seguro — e é a primeira coisa a tentar quando o build se comporta de
forma estranha. Não reaproveite um `build/` gerado em outro sistema
operacional: ele contém caminhos absolutos daquela máquina.

## Documentação

- [ARCHITECTURE.md](ARCHITECTURE.md) — visão arquitetural inicial;
- [ROADMAP.md](ROADMAP.md) — fases planejadas de desenvolvimento;
- [AI_GUIDELINES.md](AI_GUIDELINES.md) — regras para assistentes de IA que
  trabalham neste repositório.
