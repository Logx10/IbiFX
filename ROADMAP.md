# Roadmap — IbiFX

Fases de alto nível. Cada uma será iniciada apenas quando a anterior estiver
compreendida e funcionando.

**Fases 0 a 7 concluídas.** Tempo real tocando pela placa de som, com
interface de terminal. Fase 8 (Pedalboard) começando.

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
  Acrescentado depois o `HighPassFilter`, um filtro de um polo com corte
  ajustável. Ele não estava na lista original, mas apareceu como necessidade
  real ao buscar um som de crunch apertado: saturar um sinal com muito grave
  produz intermodulação e empasta o resultado, e cortar depois não conserta.
  É também o primeiro filtro do projeto, e a base para tone stack e cabinet.
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

- **Processamento offline de arquivo** (§26 do AI_GUIDELINES, fora da tabela
  acima). Concluída. Leitura de `.wav` em PCM de 16, 24 e 32 bits e float de
  32 bits; escrita em PCM de 16 bits. Parser RIFF escrito à mão, sem
  dependência externa. Um gerador de sinal de teste permite experimentar sem
  arquivo externo.
  Era o passo que faltava para **ouvir** o que foi construído: até aqui tudo
  havia sido verificado lendo números no terminal, e o próprio guia lembra
  que testes não substituem audição.

- **Fase 6 — Audio Graph.** Concluída. Nós com identificador estável,
  conexões, terminais de entrada e saída, e mistura por soma quando um nó
  recebe de vários. A ordem de processamento sai de uma ordenação topológica
  (algoritmo de Kahn), recalculada só quando a estrutura muda — nunca a cada
  bloco.
  Ciclos são recusados no `connect()`, antes de a conexão existir: um ciclo
  não tem ordem válida e em áudio é realimentação sem atraso. Isso não proíbe
  realimentação interna, como a do `Delay`.
  Cada nó tem buffer próprio, alocado no `prepare()`, porque um nó que
  alimenta dois caminhos precisa que sua saída sobreviva ao primeiro leitor.
  Buffers maiores que o bloco preparado são fatiados em vez de realocados.

- **Fase 7 — Real-Time Audio.** Concluída.
  A **parte de concorrência** ficou pronta primeiro, e era o problema adiado
  duas vezes: `Parameter` guarda um `std::atomic<float>`, e a `CommandQueue`
  leva bypass e reset do controle para a thread de áudio sem bloquear.
  Verificado com duas threads de verdade e com o ThreadSanitizer, que não
  acusou nenhuma corrida. Também foi medido que performance não é gargalo: a
  cadeia usa 0,11% do orçamento de um bloco, então não há motivo para
  paralelizar o processamento.
  O **dispositivo de áudio** também ficou pronto, via miniaudio 0.11.25
  vendorizado — um único header, domínio público. A camada fica confinada em
  `src/platform/` e o núcleo não a enxerga: o alvo `ibifx_core` nem tem o
  diretório dela no include, então a separação é verificada pelo compilador.
  O `LiveEngine` converte entre o buffer intercalado do driver e os buffers
  mono dos módulos, com tudo alocado no `start()`.
  Faltava ajustar latência e medir o comportamento sob carga real — o que só
  fazia sentido tocando de verdade, com hardware. Ao testar com a interface
  real (`mvsilicon B1 usb audio`), apareceram três problemas, todos
  corrigidos:
  - `AudioDevice` media duração de callback, blocos acima do orçamento e
    eventos de reroteamento/interrupção do driver — nenhum dos três acusou
    nada. A cadeia nunca esteve perto do orçamento de tempo real.
  - O que parecia "captura instável" era o próprio `--live` alternando o
    bypass do drive a cada 2s para demonstrar comando ao vivo — confundia
    quem só queria ouvir a cadeia tocando. Virou opcional (`--no-toggle`), e
    `deviceName()`/`captureDeviceName()` passaram a mostrar separadamente o
    dispositivo de saída e o de entrada, porque o Windows pode negociar dois
    dispositivos padrão diferentes para cada lado.
  - O que soava como "toum TOUM toum toum" era clipping de verdade: o
    `Delay` soma o ataque de uma nota nova, já quase saturado pelo
    `SoftClipper`, com a cauda que ainda ecoa da nota anterior — sem teto
    embutido nessa soma. Medido com o sinal de teste padrão: 4514, 6497 e
    6694 amostras cortadas reto nas notas 2, 3 e 4. Resolvido com um módulo
    novo, `Limiter`, sempre por último na cadeia — ver o comentário em
    `src/Limiter.h` para o raciocínio completo.

- **Fase 8 — Pedalboard.** Em andamento.
  Da lista do §32 do AI_GUIDELINES (Noise Gate, Compressor, Overdrive,
  Distortion, Delay, Reverb): `Overdrive`/`Distortion` já existiam como
  `SoftClipper`/`Clipper`, e `Delay` já existia.
  `NoiseGate` — filtro de um polo aplicado ao ganho, não ao sinal: ataque
  fixo em 2ms, release ajustável (padrão 150ms). Entra primeiro na cadeia,
  antes do `HighPass`, porque ruído amplificado depois de passar por
  `Gain`/`SoftClipper` pode ultrapassar o threshold sem ser reconhecido como
  ruído.
  `Compressor` — o primeiro módulo do projeto que trabalha em decibéis por
  dentro, porque "ratio 4:1" só significa o que deveria significar em
  domínio logarítmico. Mesmo desenho de detector de nível do `NoiseGate`
  (filtro de um polo com ataque e release), mas aqui os dois tempos são
  ajustáveis. Fica logo após o `NoiseGate`, antes de qualquer distorção —
  ordem clássica de pedaleira. Makeup gain e soft knee ficaram de fora de
  propósito: o primeiro porque o `GainProcessor` já resolve isso encadeado
  depois, o segundo porque a versão mais simples (hard knee) ainda não tinha
  sido compreendida.
  Faltam `Reverb` — e, da lista do §25, uma distorção assimétrica.

## Regra

A ordem pode mudar, mas camadas não devem ser puladas: cada uma precisa ser
compreendida antes que outras sejam empilhadas por cima.
