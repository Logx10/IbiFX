# Roadmap — IbiFX

Fases de alto nível. Cada uma será iniciada apenas quando a anterior estiver
compreendida e funcionando.

**Nenhuma fase além da 2 foi iniciada.**

| # | Fase | Objetivo |
|---|------|----------|
| 0 | Ambiente / CMake | Build funcionando: source → CMake → compilador → linker → executável |
| 1 | Fundamentos de áudio digital | Sample, sample rate, amplitude, clipping, buffers, block processing |
| 2 | DSP básico offline | Gain, clipper, distorção simples — testáveis sem real-time |
| 3 | AudioModule | Contrato mínimo comum a todo módulo processador |
| 4 | Module Chain | Cadeia linear dinâmica: adicionar, remover, reordenar, bypass |
| 5 | Parameters | IDs, ranges, normalização, smoothing |
| 6 | Audio Graph | Nodes, conexões, ordem de processamento, topological sorting |
| 7 | Real-Time Audio | Callback de áudio, block size, latência, real-time safety |
| 8 | Pedalboard | Noise gate, compressor, overdrive, distortion, delay, reverb |
| 9 | Amp Simulation | Preamp, tone stack, power amp |
| 10 | Cabinet / IR | Impulse response, convolução, carregamento de `.wav` |
| 11 | Presets | Serialização de módulos, ordem, bypass e parâmetros |
| 12 | UI | Camada gráfica sobre um engine já funcional |
| 13 | MIDI | Controllers, mapping, comandos |
| 14 | Master Transport | Relógio compartilhado: posição, BPM, play/stop/record/loop |
| 15 | Metronome | Sample-accurate, baseado no transport |
| 16 | Backing Tracks | Load, play, seek, loop A/B |
| 17 | Recorder | Gravação de uma track, depois sessão |
| 18 | Multitrack / Reamping | DI + sinal processado em tracks separadas |
| 19 | Practice Mode | Backing track + metrônomo + loop + gravação integrados |
| 20 | WebAssembly | Reuso do DSP C++ via Emscripten e AudioWorklet |

## Status

- **Fase 0** — concluída. Build com CMake funcionando nos três sistemas, com
  biblioteca, executável e teste registrado no CTest.
- **Fase 1** — coberta na prática, sem etapa própria. Sample, amplitude, faixa
  `[-1, +1]` e buffer apareceram naturalmente ao construir o `GainProcessor`.
- **Fase 2** — concluída. `GainProcessor`, `Clipper` (hard clipping) e
  `SoftClipper` (saturação via `tanh`) prontos e testados, com um demo que
  encadeia os três e compara os dois tipos de corte.
- **Fase 3** — concluída. Contrato `AudioModule` extraído das três
  implementações existentes: destrutor virtual, `name()` e `process()`, os
  dois últimos virtuais puros. Os três módulos herdam dele, e o demo já
  processa cadeias num laço que não conhece os módulos.
  Ficaram deliberadamente de fora `prepare(sampleRate, blockSize)` e
  `reset()`: nenhum módulo atual precisa deles, e o Delay é quem deve
  justificá-los.
- **Fase 4** — concluída. `ModuleChain` com adicionar, remover, reordenar,
  bypass e `clear`, sendo dono dos módulos via `std::unique_ptr`. Índice
  inválido lança exceção em vez de falhar em silêncio. Não é seguro para uso
  concorrente — alterar a cadeia durante o `process()` é corrida de dados, e
  isso será resolvido quando o tempo real entrar.
- **Fase 5** — parcialmente concluída. `Parameter` com id estável, faixa,
  clamp e forma normalizada; `AudioModule` passou a guardar e expor a lista.
  Os setters tipados (`setGain`, `setThreshold`, `setDrive`) continuam, agora
  como atalhos sobre o parâmetro — não há estado duplicado.
  **Falta o smoothing**, deixado de fora de propósito: ele depende de saber o
  sample rate e agir amostra a amostra, o que exige um `prepare()` que ainda
  não existe. O §29 pede que o problema seja estudado antes de abstraído, e
  para estudá-lo é preciso primeiro conseguir ouvir o clique.
- **Fase 6** — concluída. `Delay` com buffer circular, tempo em segundos,
  feedback limitado a 0.95 e mix seco/molhado. Primeiro módulo com estado
  entre blocos, e foi ele que justificou `prepare()` e `reset()` no contrato
  — ambos com implementação padrão vazia, para não obrigar os módulos
  stateless a escrever corpos vazios. O `ModuleChain` repassa os dois a todos
  os módulos. Toda alocação acontece no `prepare()`; o `process()` não aloca.
- **Fase 7** — próxima. Parameter smoothing, agora que o sample rate chega
  aos módulos. Depende de conseguir ouvir o clique de uma mudança abrupta —
  o que pede sinal contínuo, não um buffer de 8 amostras.

## Regra

A ordem pode mudar, mas camadas não devem ser puladas: cada uma precisa ser
compreendida antes que outras sejam empilhadas por cima.
