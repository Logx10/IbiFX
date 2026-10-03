# ADR 0001 — Portabilidade: app desktop e WebAssembly

- **Status:** aceita — biblioteca de UI decidida em 2026-10-03 (ver abaixo)
- **Data:** 2026-10-02

## Contexto

O IbiFX hoje roda como CLI e TUI no terminal, com áudio em tempo real via
miniaudio. O objetivo é que o mesmo engine C++ vire:

1. um **app desktop** (macOS e Windows) com janela e interface gráfica;
2. uma **versão web** compilada para WebAssembly (§46 do AI_GUIDELINES).

### O que já favorece a portabilidade

- O `ibifx_core` usa apenas a biblioteca padrão. Não conhece sistema
  operacional, terminal nem miniaudio, e o CMake garante isso com alvos
  separados (`ibifx_core`, `ibifx_platform`, `ibifx_tui`).
- O miniaudio tem backend para Web Audio (inclusive AudioWorklet) quando
  compilado com Emscripten. `AudioDevice` e `LiveEngine` devem funcionar no
  navegador com poucos ajustes, sem um backend novo.
- A comunicação entre UI e áudio já segue o padrão "a UI envia comandos e a
  thread de áudio aplica" (`CommandQueue`, `Parameter` atômico, picos
  atômicos). Qualquer interface nova entra por esse mesmo caminho.

### O que bloqueia ou exige trabalho

| Ponto | Onde | Por que importa |
|---|---|---|
| Executáveis misturados | `src/main.cpp` | Demo, CLI e TUI estão no mesmo `main`. Cada frontend precisa do seu próprio. |
| Terminal POSIX | `src/platform/Terminal.cpp` | Usa termios, que não existe no navegador. Precisa ficar fora do build web. |
| WAV só por caminho | `src/WavFile.cpp` | Usa `std::ifstream`. Na web o arquivo chega como bytes (upload ou fetch). |
| Exceções | AudioGraph, Parameter, WavFile… | Por padrão o Emscripten não consegue capturá-las. Exige `-fwasm-exceptions`. |
| Fila de um produtor só | `CommandQueue` | GUI e MIDI, cada um na sua thread, seriam dois produtores. Não é urgente. |

### Particularidades da web

- O `AudioContext` só começa depois de um gesto do usuário (clique).
- No `getUserMedia` é **obrigatório** desligar `echoCancellation`,
  `noiseSuppression` e `autoGainControl`. Caso contrário o navegador trata a
  guitarra como voz e degrada o sinal.
- A latência no navegador é bem maior que no nativo (algo como 10 a 30 ms ou
  mais). Para tocar ao vivo, o desktop continua sendo a versão principal.
- Compartilhar memória entre a thread principal e o AudioWorklet (o modelo
  atual com `atomic`) exige SharedArrayBuffer, e portanto os headers COOP/COEP
  no servidor. A alternativa é rodar o engine inteiro dentro do worklet e
  controlar tudo por `postMessage`.

### Particularidades do desktop

- **macOS:** é preciso empacotar como `.app` e incluir
  `NSMicrophoneUsageDescription` no Info.plist. Sem isso a entrada de áudio é
  bloqueada sem nenhum aviso. Para distribuir, ainda há assinatura e
  notarização.
- **Windows:** o miniaudio não tem ASIO, só WASAPI. Para baixa latência com
  interfaces de áudio, ASIO faz diferença.

## Decisão

### Decidido

1. **Um core, vários frontends.** O `ibifx_core` continua sem nenhuma
   dependência de plataforma ou UI. Os frontends viram executáveis separados
   (`apps/cli`, `apps/desktop`, `apps/web`), escolhidos por opções do CMake.
2. **Manter o miniaudio** como camada de áudio tanto no desktop quanto na web,
   enquanto não houver necessidade concreta de ASIO ou de plugin.
3. **Validar a portabilidade antes de qualquer UI:** compilar o core e os
   testes com Emscripten e rodá-los no Node pelo CTest.

### Decidido em 2026-10-03: biblioteca de interface

**Dear ImGui + SDL3.** As duas perguntas em aberto foram respondidas
diretamente: a interface é um app desktop com janela própria, sem
pretensão de visual nativo do SO, e não haverá plugin VST3/AU por
enquanto — o que descarta o JUCE. ImGui+SDL3 continua sendo a única opção
em que o mesmo código C++ de interface roda numa janela no desktop e
também no navegador, é MIT, pequena e explícita, e mantém o miniaudio.

Trazido via **CMake FetchContent** (baixa o código-fonte no configure,
compila junto), não vendorizado como o miniaudio: SDL3 é uma biblioteca
grande, com backend por sistema operacional, e vendorizá-la infla o
repositório em dezenas de MB. Ver `option(IBIFX_BUILD_DESKTOP)` no
`CMakeLists.txt` raiz — desligada, o configure não toca em SDL3/ImGui.

A primeira fatia (passo 4 do plano abaixo) está implementada: `src/gui/
DesktopUI.{h,cpp}` e `apps/desktop/main.cpp`, uma janela com um knob de
ganho e dois medidores (entrada/saída) ligados ao `LiveEngine`, seguindo o
mesmo contrato da `PedalboardUI` (camada genérica sobre a cadeia, sem
lógica de DSP, comandos só pela `CommandQueue`).

## Alternativas

| Opção | Desktop | Web | Licença | Observação |
|---|---|---|---|---|
| **Dear ImGui + SDL3** | ✅ | ✅ o mesmo código | MIT | UI única. Visual não nativo, bom para knobs e medidores desenhados. |
| **JUCE** | ✅ áudio, UI, VST3/AU, ASIO | ❌ sem suporte oficial | AGPLv3 ou comercial | Prevista no AI_GUIDELINES. Substituiria o miniaudio no desktop. A web precisaria de outra UI. |
| **UI web compartilhada** (React + webview no desktop) | ✅ | ✅ | depende da biblioteca de webview | Uma UI em TypeScript/React (stack do §47). Junta dois ecossistemas. |
| **Qt** | ✅ visual nativo | ⚠️ funciona, mas é pesado | LGPL ou comercial | Robusto, porém grande para o projeto. |

O JUCE não fica descartado. Como o core é independente, ele pode entrar depois
como camada de plataforma (plugins, ASIO) sem afetar o DSP.

## Consequências

### Plano de execução, em passos pequenos

1. Reorganizar o CMake e separar os executáveis por frontend, sem mudar
   comportamento.
2. Fazer o `WavFile` ler de um buffer de bytes em memória (a versão por
   caminho passa a usá-la), com testes.
3. Instalar o emsdk e rodar `ibifx_core` e os testes em WASM no Node.
4. Escolher a biblioteca de UI e fazer uma janela mínima: um knob de ganho e
   um medidor ligados ao `LiveEngine`.
5. Rodar essa mesma janela no navegador.

### Ferramentas necessárias

- **emsdk** (Emscripten), para compilar para WebAssembly;
- **Node.js**, para rodar os testes compilados em WASM;
- **Ninja**, opcional, para builds mais rápidos.

### Dúvida levantada: uma thread para o core e outra para E/S?

Durante o estudo, o desenvolvedor perguntou se faria sentido ter uma thread só
para o core e outra para entrada e saída. A resposta, registrada aqui porque
vai guiar as fases 13, 16 e 17:

**O DSP fica no callback do driver.** Quem cria a thread de áudio é o driver,
que chama o callback a cada bloco. Mover o DSP para uma thread nossa obrigaria
a passar os buffers por ring buffers:

```text
hoje:        driver → callback → cadeia DSP → driver
alternativa: driver → ring buffer → thread DSP → ring buffer → driver
```

Isso custa pelo menos um bloco a mais em cada sentido (2,67 ms com 128 frames
a 48 kHz) e aumenta o risco de estalo, porque uma thread criada por nós não
tem prioridade de tempo real sem configuração específica (no macOS, audio
workgroups).

**A E/S de disco, sim, ganha thread própria.** Gravar, tocar backing track e
carregar IR envolvem disco, que pode bloquear por milissegundos. Isso nunca
pode acontecer na thread de áudio:

```text
gravação:      áudio → ring buffer → thread de disco → arquivo .wav
backing track: arquivo → thread de disco → ring buffer → áudio
```

**"Core" não é uma thread.** O mesmo código roda em threads diferentes
conforme quem chama: `prepare()`, `add()` e carregar preset rodam no controle;
`process()` roda no áudio.

Mapa de threads previsto:

| Thread | Quem cria | O que faz | Como conversa |
|---|---|---|---|
| Áudio | driver | `process()` da cadeia | recebe da `CommandQueue`, publica picos em `atomic` |
| Controle / UI | `main` | janela, knobs, presets | envia para a `CommandQueue` |
| MIDI | driver MIDI | pedais e controladores | envia comandos (fase 13) |
| E/S de disco | o IbiFX | gravar e ler arquivos | ring buffer com o áudio (fases 16 e 17) |

UI e MIDI seriam dois produtores, mas a `CommandQueue` aceita só um. A solução
prevista é **uma fila por produtor**: a thread de áudio esvazia todas no
começo do bloco, e cada fila continua com um produtor e um consumidor.

Também existem threads auxiliares para trabalho pesado (a parte longa de uma
IR comprida, ramos paralelos do grafo em vários núcleos). Ficam para quando
houver necessidade medida.

### Custos aceitos

- A versão web terá latência maior e nunca substituirá o desktop para tocar
  ao vivo.
- Sem JUCE, o Windows fica sem ASIO até que isso vire uma necessidade real.
- Cada frontend novo é mais um alvo para manter compilando nas três
  plataformas.
