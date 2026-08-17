# Roadmap — IbiFX

Fases de alto nível. Cada uma será iniciada apenas quando a anterior estiver
compreendida e funcionando.

**Fases 0 a 5 concluídas.** O engine de DSP offline está de pé; nada de
tempo real, arquivo ou interface foi implementado.

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

- **Fase 0 — Ambiente / CMake.** Concluída. Build funcionando nos três
  sistemas, com biblioteca, executável e testes registrados no CTest.

- **Fase 1 — Fundamentos de áudio digital.** Coberta na prática, sem etapa
  própria. Sample, amplitude, faixa `[-1, +1]` e buffer apareceram
  naturalmente ao construir o `GainProcessor`.

- **Fase 2 — DSP básico offline.** Concluída. Quatro módulos:
  `GainProcessor`, `Clipper` (hard clipping), `SoftClipper` (saturação via
  `tanh`) e `Delay` (eco com buffer circular).
  Os três primeiros são funções de transferência sem estado. O `Delay` é o
  primeiro módulo com memória entre blocos, e foi ele que justificou
  `prepare()` e `reset()` no contrato. Toda alocação acontece no `prepare()`;
  o `process()` não aloca.
  Faltam da lista do §25 do AI_GUIDELINES o noise gate e uma distorção
  assimétrica.

- **Fase 3 — AudioModule.** Concluída. Contrato extraído das implementações
  existentes, e não desenhado por antecipação: destrutor virtual, `name()`,
  `process()`, `prepare()` e `reset()`. Os dois últimos têm corpo padrão
  vazio, para não obrigar módulos stateless a escrevê-los.

- **Fase 4 — Module Chain.** Concluída. Cadeia montada em tempo de execução
  com adicionar, remover, reordenar, bypass e `clear`, dona dos módulos via
  `std::unique_ptr`. Índice inválido lança em vez de falhar em silêncio.
  Não é segura para uso concorrente: alterar a cadeia durante o `process()`
  é corrida de dados, e isso só se resolve quando o tempo real entrar.

- **Fase 5 — Parameters.** Concluída. `Parameter` com id estável, faixa,
  clamp e forma normalizada de 0 a 1; a lista vive no `AudioModule`, o que
  permite ajustar qualquer módulo sem conhecer o tipo concreto. Os setters
  tipados continuam como atalhos sobre o parâmetro, sem estado duplicado.
  `SmoothedValue` cobre o smoothing com rampa linear de 20 ms, ligado ao
  ganho, ao teto, ao drive e ao feedback e mix do `Delay`. Sem `prepare()`
  a rampa fica inativa e os valores saltam.
  O **tempo** do `Delay` não é suavizado: mover a posição de leitura
  gradualmente altera a altura do som, e fazê-lo direito exige interpolação
  entre amostras vizinhas — assunto próprio.

- **Fase 6 — Audio Graph.** Não iniciada.

### Fora da tabela, e recomendado antes da Fase 6

O §26 do AI_GUIDELINES prevê uma etapa de **processamento offline de
arquivo** — `input.wav` → DSP → `output.wav` — que a tabela acima não lista.

Ela vale mais agora do que o grafo, por um motivo simples: tudo até aqui foi
verificado lendo números no terminal. Ler `.wav` e escrever `.wav` é o que
permite finalmente **ouvir** o que foi construído — a diferença entre fuzz e
overdrive, o eco do delay, e o clique que o smoothing acabou de eliminar. O
próprio guia lembra que testes não substituem audição.

## Regra

A ordem pode mudar, mas camadas não devem ser puladas: cada uma precisa ser
compreendida antes que outras sejam empilhadas por cima.
