# Roadmap — IbiFX

Fases de alto nível. Cada uma será iniciada apenas quando a anterior estiver
compreendida e funcionando.

**Fases 0 a 12 concluídas.** Tempo real tocando pela placa de som, com
interface de terminal E uma janela gráfica (Dear ImGui + SDL3), pedalboard
completo (gate, compressor, filtro, drive simétrico e assimétrico, delay,
reverb, limiter), simulação de amplificador (tone stack, preamp, power
amp), cabinet via convolução com impulse response e presets (salvar/
carregar o estado inteiro da cadeia em texto, pelo CLI ou pela própria
janela). Um afinador (Tuner, via YIN) também foi construído fora da ordem
do roadmap, a pedido direto. **Fase 13 (MIDI): abstração de controle
pronta, dispositivo de hardware pendente** — ver status abaixo. **Fase 14
(Master Transport) concluída.** **Fase 15 (Metronome) concluída.** **Fase
16 (Backing Tracks) concluída.** **Fase 17 (Recorder) concluída.** **Fase
18 (Multitrack / Reamping) concluída.** **Fase 19 (Practice Mode)
concluída.** **Fase 20 (WebAssembly) em andamento** — ver status abaixo.

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

- **Fase 8 — Pedalboard.** Concluída.
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
  `Reverb` — estrutura clássica de Schroeder (1962): 4 combs em paralelo
  (cada um com um passa-baixas no loop de feedback, pra cauda escurecer com
  o tempo) somados, seguidos de 2 allpass em série (aumentam a densidade sem
  colorir o timbre). Comprimentos dos delays vêm do Freeverb (domínio
  público), escalados pelo sample rate real. Fica depois do `Delay` na
  cadeia: molha o eco discreto numa cauda contínua, não o contrário.
  `AsymmetricClipper` — a mesma curva do `SoftClipper`, deslocada por um
  bias que simula o ponto de polarização de um estágio single-ended: os dois
  semiciclos saturam diferente, acrescentando harmônicos pares (som
  "quente") além dos ímpares que uma curva simétrica produz sozinha. Não
  entra na cadeia padrão — `SoftClipper` já cumpre o papel de distorção — e
  sim troca de lugar com ele via `--asymmetric`, para não empilhar uma
  segunda distorção sem pedido. Um teste pegou a saída passando de ±1 (a
  diferença de duas tanh não tem a mesma garantia de faixa que uma tanh
  sozinha) e outro revelou uma propriedade real do desenho: no lado do
  sinal que compartilha o sinal do bias, mais drive pode ENCOLHER a
  distorção em vez de aumentar — os dois termos convergem pra mesma
  assíntota. Não é bug, é a física de um estágio "faminto" de um lado.
  Com isso, tanto a lista do §32 quanto o pendente do §25 estão completos.
  Fase 8 encerrada.

- **Fase 9 — Amp Simulation.** Concluída.
  Objetivo do §33: `Preamp -> Tone Stack -> Power Amp`. `ToneStack`
  concluído — implementado a partir da fonte primária (Yeh & Smith,
  DAFx-06, CCRMA/Stanford), não de memória: análise nodal simbólica do
  circuito passivo real do Fender '59 Bassman, verificada pelos autores
  contra SPICE, discretizada por transformada bilinear num filtro IIR de
  3ª ordem. Reproduz a interação real entre bass/mid/treble (os três
  controles não são independentes — são nós do mesmo circuito RC) que um
  EQ de filtros separados não reproduziria. Um achado do próprio artigo: só
  bass e mid controlam os polos do sistema, treble só move os zeros.
  Ainda não entra na cadeia padrão — `--tonestack` liga.
  `Preamp` concluído — 3 estágios de `tanh(drive*x)` em cascata, com ganho
  de reexpansão entre eles (2.0x), não um único estágio com drive alto. É
  a diferença real entre a textura de um amplificador high-gain (múltiplos
  estágios moderados) e simplesmente aumentar o drive de um distorcedor só,
  que colapsa num degrau em vez de ganhar densidade harmônica. Só o último
  estágio fica sem reexpansão, o que garante saída sempre em [-1, +1] sem
  precisar de lógica extra. 3 estágios fixo por enquanto, não é parâmetro.
  Troca de lugar com `SoftClipper`/`AsymmetricClipper` via `--preamp`, não
  acrescenta uma segunda distorção à cadeia padrão.
  `PowerAmp` concluído — a diferença pro `Preamp` é ter MEMÓRIA: um
  envelope de nível (mesmo filtro de um polo do `NoiseGate`/`Compressor`,
  agora em `OnePole.h`) acompanha o volume sustentado recente, não a
  amostra instantânea, e aumenta o drive efetivo da saturação conforme esse
  volume sobe — simula o "sag" da fonte de alimentação sob carga pesada.
  Uma seção tocada forte deixa a nota seguinte mais comprimida mesmo que
  ela mesma seja fraca. Tempos de ataque/release bem mais lentos (50ms/
  300ms) que os do gate ou do compressor, de propósito: reage à música, não
  à nota. `onePoleCoefficient` foi extraído pra `OnePole.h` nesta fase —
  era a terceira repetição da fórmula (`NoiseGate`, `Compressor`,
  `PowerAmp`), e o próprio comentário no `Compressor.cpp` já previa que a
  terceira vez seria o sinal pra extrair (regra do §55).
  Não entra na cadeia padrão — `--poweramp` liga, no fim, antes do
  `Limiter`.
  Com isso, `Preamp -> Tone Stack -> Power Amp` (§33) está completo. Fase 9
  encerrada.

- **Fase 10 — Cabinet / IR.** Concluída.
  Convolução com impulse response (IR) de verdade, em vez de aproximar o
  timbre de um gabinete/microfone com um EQ ajustado à mão — matematicamente
  reproduz o que aquele sistema específico faria com o sinal. Três peças
  separadas, como o §34 sugere pelo nome: `IRLoader` (lê a IR de um `.wav`,
  reduz a mono pela média dos canais se for estéreo, reaproveitando
  `wav::read` já existente), `ConvolutionEngine` (a matemática pura,
  `y[n] = Σ h[k]·x[n-k]`, com um buffer circular guardando as últimas M
  amostras — mesma ideia do `Delay`, só que lendo TODAS as M posições a
  cada amostra de saída) e `Cabinet` (o `AudioModule` que amarra os dois,
  com `mix` padrão em 1.0 — diferente de `Delay`/`Reverb`, um cabinet sim
  substitui o timbre inteiro).
  Custo O(M) por amostra é um limite conhecido e documentado: convolução
  direta, não FFT — o próprio §34 já antecipa isso como o próximo passo
  quando o custo se mostrar um problema de verdade, não antes.
  Testado ponta a ponta com uma IR sintética (ruído + decaimento): o sinal
  ficou bem mais quente que o esperado, encostando no teto do `Limiter` boa
  parte do arquivo — característica da IR de teste improvisada (ruído
  aleatório não é fisicamente representativo de um gabinete real, que não
  amplifica energia), não bug na convolução, já verificada com valores
  exatos calculados à mão nos testes.
  Não entra na cadeia padrão — `--cabinet arquivo.wav` liga, entre o power
  amp e o limiter.

- **Fase 11 — Presets.** Concluída.
  Três peças separadas, como o §35 do AI_GUIDELINES sugere pelo nome:
  `Preset` (só dado — mesmo desenho do `WavFile`: uma struct com o nome e a
  lista ordenada de módulos, cada um com tipo, bypass e parâmetros; e as
  funções livres `preset::capture`/`preset::apply` que vão e vêm de uma
  `ModuleChain` de verdade), `Serialization` (texto humano e depurável, não
  JSON — mesma decisão do WAV: zero dependência nova, parser escrito à mão,
  orientado a linha, com uma palavra-chave abrindo cada uma: `preset`,
  `module`, `param`, `ir`) e `PresetManager` (só a ida e volta ao disco,
  gravando/lendo o texto de `Serialization`).
  O preset é **plano e genérico de propósito**: não existe um campo especial
  pra "ampli" ou "cabinet" — cada módulo na cadeia entra do mesmo jeito,
  identificado pelo mesmo texto que `name()` já devolve. É quem monta a
  cadeia (`buildChain()` em `main.cpp`) que decide o que cada um significa
  musicalmente, não o preset. A única exceção é o caminho da IR do
  `Cabinet`, que não é um `Parameter` — por isso ganhou um getter
  (`Cabinet::irPath()`) e um campo à parte em `Preset::ModuleState`,
  tratado com `dynamic_cast` em vez de um método virtual novo em
  `AudioModule` só para um caso.
  Testado ponta a ponta pelo CLI (`--save-preset` grava a cadeia que acabou
  de montar, `--preset` reconstrói uma cadeia a partir do arquivo): o
  arquivo de áudio processado a partir do preset salvo saiu byte a byte
  idêntico ao processado com as flags originais.

- **Fase 12 — UI. Concluída.**
  O ADR 0001 (docs/adr/0001-portabilidade-desktop-e-web.md) decidiu a
  biblioteca: Dear ImGui + SDL3, trazidos via CMake FetchContent, atrás de
  `option(IBIFX_BUILD_DESKTOP)` — desligada, o build de sempre (CLI +
  testes) nem toca em SDL3/ImGui.
  Primeira fatia pronta: `DesktopUI` (`src/gui/`) e o executável novo
  `ibifx_desktop` (`apps/desktop/main.cpp`), uma janela com um knob de
  ganho e dois medidores (entrada/saída) ligados ao `LiveEngine` de
  verdade. Segue o mesmo contrato da `PedalboardUI` que já existia no
  terminal — lê a cadeia genericamente por `AudioModule::parameterAt()`,
  nunca por tipo de módulo, e só fala com o áudio pela `CommandQueue` —
  mas o widget em si é novo, porque `PedalboardUI` é termios do início ao
  fim. O padrão do "alvo local" (`m_targets`, documentado lá) foi
  replicado aqui pelo mesmo motivo: o comando é assíncrono, e sem isso
  arrastar um slider perderia passos.
  Em seguida, a cadeia completa: `ChainSettings`/`buildChain()`/
  `buildDefaultChain()` saíram de `src/main.cpp` para `PedalboardChain.h/
  .cpp`, no `ibifx_core` — o CLI e a janela desktop agora montam o mesmo
  pedalboard padrão (`NoiseGate -> Compressor -> HighPass -> Gain ->
  SoftClipper -> Delay -> Reverb -> Limiter`) chamando a MESMA função, em
  vez de duas cópias que arriscariam divergir sem ninguém notar.
  `apps/desktop/main.cpp` chama `buildDefaultChain()` como `--ui`/`--live`
  já faziam. `src/main.cpp` continua funcionando como antes — confirmado
  rodando `--devices` e o round-trip de preset (`--save-preset`/
  `--preset`) de novo depois da extração, byte a byte idêntico ao de
  antes.
  Verificado que a janela abre de verdade (`MainWindowTitle` = "IbiFX"),
  com os 8 módulos do pedalboard padrão visíveis, e que o processo roda
  sem travar com `--null` (sem hardware). Depois confirmado ao vivo, com
  guitarra de verdade e fones: os knobs e os footswitches respondem bem.
  Depois, os pedais viraram visuais — corpo colorido (cor derivada por
  hash do nome do módulo, não do tipo concreto), knob giratório desenhado
  à mão, footswitch redondo com LED no lugar do checkbox de bypass, lado a
  lado como um pedalboard de verdade (quebra de linha automática). Tema
  escuro com acento âmbar no lugar do cinza de fábrica do ImGui.
  Também corrigido: a janela nascia com a barra de título fora da área
  visível da tela (o tamanho pedido não descontava a decoração do SO), e
  um assert do próprio ImGui causado por posicionar sub-widgets à mão
  direto na janela principal — resolvido com uma child window por pedal
  (`BeginChild`/`EndChild`), que isola o sistema de coordenadas de cada
  um.
  Por fim, `--live`, `--ui` e `--ui-demo` passaram a aceitar as mesmas
  opções de cadeia do processamento de arquivo (`--cabinet`, `--tonestack`,
  `--gain`...) — antes elas só funcionavam processando um arquivo, e tocar
  ao vivo sempre caía na cadeia padrão fixa, sem IR nem ampli possível.
  `parseSettings()` foi refeito para receber um `std::vector<std::string>`
  em vez de `argc`/`argv` direto, o que permitiu reaproveitá-lo nos três
  modos sem duplicar a lista de flags. De quebra, a alternância de bypass
  de demonstração do `--live` (`--no-toggle`) deixou de depender de um
  índice fixo (`1`) — ela agora procura o estágio de drive pelo nome
  (`SoftClipper`/`AsymmetricClipper`/`Preamp`), porque um índice fixo
  deixou de fazer sentido assim que a cadeia virou configurável também ao
  vivo.
  Verificado ponta a ponta: `--live` com `--cabinet` mostra `Cabinet` na
  cadeia impressa e roda sem erro; processamento de arquivo com as mesmas
  flags de antes continua produzindo o resultado esperado; 27 testes
  continuam passando.
  Em seguida, ampli e cabinet chegaram na janela desktop também: `CliOptions`/
  `parseSettings()` saíram de `src/main.cpp` para `ChainArgs.h/.cpp`, no
  `ibifx_core` — o mesmo motivo de `PedalboardChain`, só que um andar
  acima (a INTERPRETAÇÃO das flags, não a montagem da cadeia em si).
  `apps/desktop/main.cpp` agora aceita `--cabinet`, `--tonestack`,
  `--preamp`, `--poweramp` e todo o resto, chamando `buildChain()` em vez
  de `buildDefaultChain()` — e `--preset CAMINHO` carrega um preset pronto
  no lugar das flags. Nenhuma linha do `DesktopUI` mudou: como ele já
  desenha qualquer módulo genericamente, ToneStack/Preamp/PowerAmp/Cabinet
  viram pedais na janela assim que entram na cadeia, do mesmo jeito que
  Gain ou Delay — a generalidade construída desde a primeira fatia pagou
  o trabalho aqui.
  Verificado: janela abre com `--tonestack --poweramp --cabinet arquivo`
  (confirmado via `MainWindowTitle`), erro de flag desconhecida ou preset
  inexistente imprime mensagem clara e sai com código 1 em vez de abrir
  janela nenhuma, carregar um preset de verdade funciona ponta a ponta, e
  os 27 testes continuam passando.
  Por fim, um painel de presets dentro da própria janela: campo de nome +
  "Salvar" grava a cadeia atual (`preset::capture`/`preset::save`) em
  `presets/<nome>.ibifxpreset`; a lista abaixo mostra o que existe ali e
  carrega qualquer um com um clique.
  CARREGAR COM O MOTOR RODANDO exigiu parar o motor primeiro: `preset::
  apply()` reconstrói a cadeia inteira via `ModuleChain::clear()`+`add()`,
  e realocar esse vetor com a thread de áudio percorrendo ele é a mesma
  corrida que `ModuleChain.h` já documentava como proibida desde a Fase 4.
  `loadPreset()` usa o par `LiveEngine::stop()`/`start()` já existente —
  mesmo caminho que a `PedalboardUI` já usava pra reabrir o dispositivo —
  ao custo de um corte breve no som durante a troca, aceitável para uma
  ação deliberada como trocar de preset (bem diferente de girar um knob).
  `presets/` segue o mesmo padrão de `audio/`: pasta e README
  versionados, os arquivos em si não.
  Com isso a Fase 12 está completa. Fase 12 encerrada.

- **Fase 13 — MIDI. Abstração de controle concluída; dispositivo de
  hardware pendente.**
  O §37 do AI_GUIDELINES pede o pipeline `Controller -> Mapping ->
  Command -> IbiFX`, e é exatamente isso que existe agora: `MidiMessage`
  (um evento já decodificado — tipo, canal, data1, data2 — sem byte de
  protocolo nenhum, mesma separação que `AudioDevice` já faz pro áudio) e
  `MidiMapping` (a camada "Mapping": liga um número de Control Change a um
  parâmetro ou a um bypass, e traduz qualquer `MidiMessage` no `Command`
  correspondente — o MESMO tipo que a `CommandQueue` já usa pra UI desde a
  Fase 7, não um caminho novo até o áudio).
  Um Control Change virou um `Command::SetParameterNormalized` NOVO, não o
  `SetParameter` que já existia — porque CC entrega 0 a 127, e mapear
  isso pra faixa de cada parâmetro (`Parameter::setNormalized()`) é
  exatamente o que a Fase 5 já previa no comentário de `Parameter.h`:
  "MIDI CC entrega 0 a 127... a forma normalizada é a moeda comum". O
  mapeamento nem guarda min/max — só o id do parâmetro.
  Mapear pra bypass segue a convenção comum de pedaleira MIDI: valor >= 64
  liga, abaixo desliga (meio curso de um pedal de expressão ou switch).
  **Duas filas, não uma.** `CommandQueue` é de mão única por desenho — um
  produtor, um consumidor, e é essa restrição que permite os dois índices
  atômicos não disputarem entre si. UI e MIDI são DUAS threads de controle
  diferentes; dividir a mesma fila recriaria exatamente a corrida que
  `CommandQueue` foi desenhada pra não ter. A solução, já prevista no ADR
  0001 ("GUI e MIDI, cada um na sua thread, seriam dois produtores. Não é
  urgente"): `ModuleChain` ganhou uma segunda `CommandQueue` própria
  (`pushMidiCommand()`), e `process()` esvazia as duas no começo do
  bloco — cada fila continua com um único escritor, só que agora são duas
  filas, uma por produtor.
  **Program Change fica pra depois**, de propósito: o próprio §37 já lista
  isso em "suportar futuramente", separado do CC. Trocar de preset é
  mudança ESTRUTURAL da cadeia (`ModuleChain::clear()`+`add()`), que não
  cabe num `Command` atravessando a fila de tempo real — precisaria do
  mesmo caminho de parar/trocar/religar que `DesktopUI::loadPreset()` já
  usa, fora da thread de áudio.
  **O que falta**: um `MidiDevice` de verdade (camada de plataforma,
  análoga ao `AudioDevice`) que leia uma porta MIDI de hardware e alimente
  `pushMidiCommand()` — hoje `MidiMapping::translate()` só foi exercitado
  com `MidiMessage` escritas à mão em teste, nunca com um controlador
  físico. Isso exigiria uma biblioteca nova (RtMidi é a candidata natural,
  pelo mesmo motivo do miniaudio: pequena, focada, cross-platform) e fica
  para quando houver hardware MIDI de verdade pra testar contra — mesma
  régua que a Fase 7 aplicou ao áudio.
  Testado: 28 testes no total (2 novos — `test_midi_mapping.cpp` cobre a
  tradução CC->Command ponta a ponta; `test_command_queue.cpp` ganhou um
  teste confirmando que as duas filas são independentes e que
  `SetParameterNormalized` aplica pela faixa normalizada).

- **Fase 14 — Master Transport. Concluída.**
  `MasterTransport`: posição (em amostras), BPM, play/stop, um
  sinalizador de recording e uma região de loop — exatamente a lista do
  §14 do roadmap, e nada além dela. Não é um `AudioModule` e não processa
  amostra nenhuma: só CONTA, e quem lê a contagem decide o que fazer com
  ela — o metrônomo (Fase 15) decide quando soar, o player de backing
  track (Fase 16) decide qual frame tocar. Mantém os consumidores
  desacoplados uns dos outros.
  `advance(frameCount)` roda na thread de áudio, uma vez por bloco — é o
  que faz o tempo passar em AMOSTRAS, não num timer de parede que o
  sistema operacional poderia atrasar ou adiantar.
  O mesmo problema de concorrência do `CommandQueue` apareceu de novo,
  numa forma menor: `advance()` faz um ler-somar-gravar na posição a cada
  bloco, e se `seek()` (domínio de controle) gravasse ali direto, um
  pedido chegando entre o ler e o gravar de `advance()` seria apagado sem
  ninguém perceber. Resolvido com a mesma ideia da fila de comandos, só
  que para um valor só: `seek()` deposita um alvo e ergue uma bandeira;
  `advance()` confere a bandeira ANTES de somar o bloco. A posição
  continua tendo um único escritor de verdade — a própria `advance()`.
  O loop também tem seu detalhe: ultrapassar o fim não "pula pro início"
  — isso perderia a fração do bloco que já tinha avançado depois da
  borda, e o loop encolheria um pouco a cada volta. `advance()` preserva
  esse excesso por módulo, então o comprimento do loop nunca deriva.
  Testado: 9 testes novos (`test_master_transport.cpp`) — parado não
  avança, tocando avança, `stop()` congela no lugar (não volta pro
  início), `seek()` funciona mesmo parado, conversão segundos/batidas a
  partir do sample rate e do BPM, o loop preserva o excesso ao dar a
  volta, loop desligado ou invertido não interfere, e `setRecording()` é
  só um sinalizador independente do play. 29 testes no total.

- **Fase 15 — Metronome. Concluída.**
  `Metronome` não é um `AudioModule`: ele GERA um clique num instante
  absoluto do relógio compartilhado, em vez de transformar um sinal que
  chega — por isso `process()` recebe explicitamente a posição do
  primeiro frame do bloco (o `AudioModule::process()` comum só recebe o
  buffer, sem saber onde ele começa no tempo da música).
  "Sample-accurate" (a palavra do próprio §15) significa isto: cada
  amostra do bloco é conferida contra a posição exata da próxima batida
  (derivada do BPM do `MasterTransport`), e o clique começa ali, não "em
  algum momento perto" — a diferença entre as duas é de até um bloco
  inteiro de incerteza (2,7 ms a 48 kHz/128 amostras), audível para um
  ouvido treinado.
  A posição da batida é sempre recalculada a partir do índice absoluto
  (`beatIndex * samplesPerBeat`), nunca acumulada bloco a bloco — é o que
  evita o clique derivar quando `samplesPerBeat` não é um número inteiro
  de amostras (BPMs não redondos).
  `process()` SOMA o clique ao buffer, não substitui: convive com a
  guitarra ou a backing track (Fase 16), não as processa. Um clique que
  não cabe inteiro no bloco em que nasceu continua no próximo `process()`
  — mesmo espírito da cauda do `Delay` atravessando blocos.
  Testado: 6 testes novos (`test_metronome.cpp`) — parado fica em
  silêncio, o clique começa exatamente na amostra certa (nem uma antes),
  atravessa a fronteira entre blocos sem redisparar, `reset()` descarta o
  clique em andamento, o volume escala a amplitude proporcionalmente, e
  longe de uma batida o sinal original passa intocado. 30 testes no total.

- **Fase 16 — Backing Tracks. Concluída.**
  "Load, play, seek, loop A/B" — e play/seek/loop A/B já estavam prontos
  desde a Fase 14, não por acaso: o `MasterTransport` foi construído antes
  exatamente para que isto acontecesse. `BackingTrackPlayer` nem guarda
  referência a um transport — ele só sabe fazer duas coisas que o
  transport não pode fazer sozinho: carregar um arquivo e, dada uma
  posição (que quem chama já leu de algum transport), dizer qual amostra
  dele toca ali. Se esse transport estiver com loop ligado entre A e B, a
  posição que ele relata já volta sozinha para A ao passar de B — o "loop
  A/B" do título da fase não pediu nenhum código novo aqui, só composição.
  Mono (a cadeia inteira do `LiveEngine` é mono) e sem reamostragem — o
  arquivo precisa estar no mesmo sample rate do motor, e `load()` lança se
  não estiver, em vez de tocar silenciosamente em pitch errado. Nenhuma
  outra parte do projeto reamostra hoje (nem o processamento de arquivo,
  nem o `Cabinet`); seria inconsistente a backing track ser a exceção.
  A redução estéreo-para-mono repete o código de `IRLoader.cpp` em vez de
  reaproveitá-lo por um nome emprestado — são só duas ocorrências até
  agora, e a regra do projeto (AI_GUIDELINES §55) é esperar a terceira
  antes de extrair um nome genérico para as duas.
  Testado: 9 testes novos (`test_backing_track_player.cpp`) — sem arquivo
  carregado fica em silêncio, tamanho relatado correto, `process()` soma
  (não substitui) a amostra certa de cada posição, começa numa posição
  arbitrária, fica em silêncio depois do fim do arquivo, estéreo vira mono
  pela média, volume escala a amplitude, sample rate incompatível lança, e
  arquivo inexistente lança. 31 testes no total.

- **Fase 17 — Recorder. Concluída (uma track; sessão fica para a Fase 18).**
  "Gravação de uma track" — a parte de sessão (várias tracks juntas) é
  naturalmente o território da Fase 18 (Multitrack), que vem a seguir.
  O desenho já estava previsto no ADR 0001, na tabela de threads: disco
  pode bloquear por milissegundos (antivírus examinando o arquivo, SO
  ocupado com outra coisa), e a thread de áudio não tem esses milissegundos
  de sobra. `Recorder` separa os dois lados com um buffer circular —
  `thread de áudio → buffer → thread de disco própria → arquivo .wav` —
  usando O MESMO algoritmo da `CommandQueue` (um produtor, um consumidor,
  dois índices atômicos, uma posição sempre vazia), só carregando amostras
  em vez de comandos. Não foi extraído num tipo compartilhado: é a segunda
  vez que esse algoritmo aparece, e a regra do projeto (§55) é esperar a
  terceira antes de abstrair.
  `pushSamples()` (thread de áudio) nunca bloqueia e nunca aloca; se a
  fila estiver cheia — a thread de disco caiu pra trás —, as amostras mais
  novas são descartadas em vez de esperar, porque travar o áudio inteiro
  seria pior que perder um trecho de gravação.
  O arquivo só é escrito de verdade em `stop()`: `wav::write()` não foi
  desenhado para escrita incremental (o cabeçalho RIFF precisa do tamanho
  final), então a thread de disco só ACUMULA em memória enquanto grava, e
  `stop()` — chamado do domínio de controle, nunca do de áudio — bloqueia
  até o arquivo sair.
  **Um bug pego pelo próprio teste de concorrência, não por inspeção**: a
  primeira versão deixava a thread de disco dormir 5ms quando ociosa: um
  teste empurrando 20000 amostras rapidamente contra um buffer pequeno de
  propósito disparou o descarte por fila cheia (comportamento correto!), e
  o PRÓPRIO TESTE, que não esperava perda nenhuma, indexou o arquivo
  resultante além do que foi de fato gravado e crashou. A correção foi em
  dois lugares: o sono ocioso caiu para 1ms (menos tempo cego entre
  rajadas, sem custo de CPU relevante) e o teste passou a usar um buffer
  maior que o total de amostras — o objetivo dele é provar concorrência
  correta, não estressar o limite de overflow, que é outro comportamento,
  já coberto pelo próprio desenho do algoritmo.
  Testado: 6 testes novos (`test_recorder.cpp`) — `isRecording()` reflete
  start/stop, o arquivo gravado bate exatamente com as amostras
  empurradas, `start()` duas vezes seguidas lança, `pushSamples()` antes
  de `start()` não faz nada, o destrutor grava mesmo sem `stop()`
  explícito, e um produtor numa THREAD DE VERDADE empurrando 20000
  amostras preserva ordem e contagem sob concorrência real — não só numa
  simulação de thread única. 32 testes no total.

- **Fase 18 — Multitrack / Reamping. Concluída.**
  "DI + sinal processado em tracks separadas": o sentido de gravar o sinal
  SECO (DI, antes de qualquer efeito) é poder tocar essa gravação de volta
  mais tarde por uma cadeia DIFERENTE (outro ampli, outro drive, outro
  cabinet) sem precisar tocar a música de novo — "reamp". Isso só funciona
  se o seco foi preservado; o processado sozinho já perdeu informação que
  nenhum processamento reverte.
  `ReampRecorder` é só uma composição de dois `Recorder` (Fase 17) — não
  duplica buffer circular nem thread de disco, só os aciona em par. A
  sincronia das duas tracks vem de `pushBlock()` receber os dois buffers
  numa chamada só, nunca duas separadas.
  `LiveEngine` precisou de um ajuste mínimo para alimentar isso: `m_chain.
  process()` modifica o buffer mono NO LUGAR, então o sinal seco só existe
  até o instante exato antes dessa chamada — `processBlock()` agora copia
  pra um buffer próprio (`m_dryBuffer`, reservado no `start()`, nunca
  realocado dentro do callback) só quando alguém está de fato gravando
  (`m_reampRecorder.isRecording()`, uma checagem atômica barata o
  bastante para não custar nada no caso comum de não estar gravando).
  **Um bug de concorrência real, pego pelo próprio teste de integração do
  `LiveEngine`, não por inspeção**: a primeira versão de `ReampRecorder::
  stop()` chamava `m_dryRecorder.stop()` (que bloqueia num `join()`) e só
  depois `m_processedRecorder.stop()`. Nesse intervalo — que pode durar o
  tempo inteiro da thread de disco do primeiro escrever o arquivo —, um
  `pushBlock()` em andamento podia ver o primeiro já parado e o segundo
  ainda gravando, empurrando só pra um dos dois; as tracks saíam de
  tamanhos diferentes, de forma intermitente. A correção: `Recorder::
  stop()` virou duas fases (`requestStop()`, que só sinaliza sem
  bloquear, e `finishStop()`, que junta a thread), e `ReampRecorder::
  stop()` chama `requestStop()` nos dois ANTES de `finishStop()` em
  qualquer um — encolhendo a janela de corrida de "o tempo de um join
  inteiro" para "duas instruções atômicas consecutivas". Um sinalizador
  único (`m_active`) também passou a ser a fonte de verdade que
  `pushBlock()` consulta uma vez, em vez de perguntar a cada `Recorder`
  interno.
  Testado: 5 testes novos (`test_reamp_recorder.cpp`) cobrindo a API em
  isolamento, mais 1 teste de integração em `test_live_engine.cpp` rodando
  o `ReampRecorder` com o `LiveEngine` de verdade (backend Nulo) — que foi
  exatamente quem pegou o bug de concorrência acima, rodando repetidas
  vezes até reproduzi-lo. 33 testes no total.

- **Fase 19 — Practice Mode. Concluída.**
  "Backing track + metrônomo + loop + gravação integrados" — e o ponto
  desta fase é exatamente que ela não precisou inventar nenhum conceito
  novo de DSP. `PracticeSession` só ORQUESTRA o que as Fases 14 a 17 já
  construíram: um `MasterTransport` próprio (a sessão é autocontida),
  somando `BackingTrackPlayer` e `Metronome` por cima do sinal da guitarra
  já processado pelo pedalboard, e gravando o resultado com um `Recorder`.
  Diferente do `ReampRecorder` (Fase 18), que grava o seco separado do
  processado para reamplificar depois, aqui a gravação captura o MIX
  final — guitarra, backing track e clique juntos —, porque o ponto do
  modo prática é revisar a sessão depois, não reprocessar a guitarra.
  `setMetronomeEnabled(false)` também descarta qualquer clique em
  andamento (`Metronome::reset()`), para desligar no meio de uma batida
  não deixar esse clique tocando sozinho até o fim por conta do estado
  interno continuar ativo.
  Ligada ao `LiveEngine`: `processBlock()` agora chama
  `m_practiceSession.process()` DEPOIS do `ReampRecorder` ter capturado o
  sinal processado puro — a ordem importa, porque senão a track
  "processada" do reamp sairia contaminada com a faixa de apoio e o
  clique, que não fazem parte do que alguém quer reamplificar depois.
  Testado: 7 testes novos (`test_practice_session.cpp`) — estado inicial,
  `process()` só avança a posição enquanto `play()` está ativo, a backing
  track soma corretamente ao sinal, o metrônomo pode ser desligado (e
  fica desligado mesmo numa batida exata), o loop funciona através da
  sessão (preserva o excesso, como o `MasterTransport` já garantia),
  gravar captura o MIX e não só a guitarra, e BPM reflete o transport
  interno. 34 testes no total.

- **Fase 20 — WebAssembly. Em andamento.**
  O plano é o do ADR 0001 (`docs/adr/0001-portabilidade-desktop-e-web.md`),
  em passos pequenos:
  1. Reorganizar o CMake por frontend (`apps/cli`, `apps/desktop`,
     `apps/web`) — parcial: `apps/desktop` já existe desde a Fase 12;
     `src/main.cpp` ainda não foi movido para `apps/cli`, porque isso só
     passa a importar de verdade quando o `apps/web` entrar em cena.
  2. **Concluído nesta sessão**: `WavFile` ganhou `readFromMemory()`/
     `writeToMemory()` — o parser/codificador de verdade, operando só
     sobre `std::vector<unsigned char>`, sem tocar em disco. `read()`/
     `write()` viraram atalhos finos por cima. O motivo é concreto, não
     teórico: o navegador entrega um upload como bytes (um
     `ArrayBuffer`), nunca como um caminho — sem isso, nenhuma linha do
     `ibifx_core` que lê ou grava `.wav` funcionaria rodando dentro de uma
     aba.
  3. Instalar o emsdk e compilar `ibifx_core`/os testes em WASM,
     rodando-os no Node — **bloqueado nesta máquina**: o disco tem só
     ~11 GB livres (98% ocupado) no momento desta sessão, e o emsdk
     (toolchain LLVM/Binaryen completo) não cabe com segurança nessa
     folga. Node.js (v22) e Python já estão instalados, então a única
     barreira real é espaço em disco — assim que houver folga, este passo
     é o próximo.
  4. Escolher a biblioteca de UI e montar uma janela mínima — **já
     concluído**, fora de ordem: é a Fase 12 inteira (Dear ImGui + SDL3).
  5. Rodar essa mesma janela no navegador — depende do passo 3.
  Nada aqui tocou em `ibifx_core`/`ibifx_platform` além do `WavFile`: o
  resto do plano é puramente sobre o AMBIENTE de build, não sobre o DSP.
  34 testes continuam passando nativamente; a compilação em WASM é o que
  falta verificar.

- **Biblioteca de equipamentos (GearLibrary) — fora da ordem, a pedido
  direto. Passo 1 de 3 concluído (núcleo).**
  Um catálogo no espírito do navegador de gear do AmpliTube: Stomp, Amp,
  Cabinet (+ microfone) e Rack, montados nessa ordem num rig. Nenhum
  módulo de DSP novo: um equipamento é uma RECEITA de módulos que já
  existem (o "Brit 800" é HighPass -> Gain -> Preamp -> ToneStack -> Gain
  -> PowerAmp -> Gain com valores próprios), guardada como
  `Preset::ModuleState`. A biblioteca só monta um `Preset`; quem aplica
  é o `preset::apply()` de sempre. Cabinets vêm do disco (`irs/<gabinete>/
  <microfone>.wav`), não do código: IR é gravação, não receita.
  Os amplis foram calibrados para sair todos perto de -18 dB RMS com
  `audio/guitar.wav`. Sem um volume de saída depois do PowerAmp, o limpo
  saía a -45 dB (o ToneStack passivo atenua muito) e os saturados a -4 dB,
  colados no Limiter.
  Testado: 8 testes novos (`test_gear_library.cpp`). O principal confere
  que todo parâmetro de toda receita existe no módulo e cabe na faixa,
  porque `preset::apply()` ignora um id errado sem avisar. 35 testes no
  total.
  **Passo 2 concluído: navegador de equipamentos na janela desktop.**
  Painel à direita com abas Stomp / Amp / Cab / Rack e filtro por
  character. Clicar num item muda o rig, e o rig inteiro vira um Preset
  novo, aplicado pelo mesmo caminho de parar/aplicar/religar do
  carregamento de preset (`replaceChain()`). Uma troca de equipamento
  tira e põe módulos na cadeia, e isso não cabe na fila de comandos.
  Antes de parar o motor, o preset é ensaiado numa cadeia de rascunho:
  uma IR que não carrega dá erro com a cadeia antiga ainda tocando
  inteira, em vez de deixá-la pela metade. O pedalboard passa a agrupar os
  pedais por equipamento, e para isso a `GearLibrary` ganhou `rigModels()`,
  em vez de a UI repetir a ordem stomp -> amp -> cab -> rack.
  Limitação conhecida: cada troca de equipamento devolve os knobs aos
  valores da receita.
  **Painel único por equipamento.** `GearModel` ganhou `controls`: cada
  knob do painel diz qual parâmetro de qual módulo da receita ele gira, e
  em que faixa. A faixa é a do knob, não a do Parameter: o Master gira o
  ganho de 0 a um teto pequeno, nunca até -8, que inverteria a fase. Os
  amplis têm o mesmo painel (Gain, Bass, Middle, Treble, Sag, Master), com
  escala de 0 a 10 como num ampli de verdade. Quem tem um módulo só ganha
  um knob por parâmetro automaticamente, lendo as faixas do próprio
  módulo. O pedalboard fica em fileiras (Stomps / Amp + Cab / Rack). O "R"
  de um painel volta aos valores da RECEITA, não ao padrão de cada módulo,
  e o "+" abre o equipamento para ver os módulos de dentro.
  Testado: 1 teste novo confere que todo knob aponta para um módulo e um
  parâmetro reais, com faixa válida e o valor da receita dentro dela.
  **Passo 3 concluído: o rig pela linha de comando.**
  `--stomp ID` (repetível, na ordem dada), `--amp ID`, `--cab ID`,
  `--rack ID` (repetível) e `--irs PASTA`, com ids curtos (`brit-800`) ou
  completos (`amp.brit-800`). `ibifx --list-gear` lista o catálogo. As
  flags valem no processamento de arquivo, no `--live`, no `--ui` e no
  desktop. O desktop abre com o rig já ativo no navegador
  (`DesktopUI::adoptRig()`).
  A montagem da cadeia a partir das opções foi para um lugar só,
  `buildChainFromOptions()`, com a precedência preset > rig > flags
  clássicas. Antes, cada frontend repetia o "if preset... else
  buildChain", e `--live`/`--ui` ignoravam o `--preset` sem avisar. Agora
  ele vale ali também.
  Testado: 7 testes novos (`test_chain_args.cpp`) cobrem ordem das flags,
  valor faltando, ids curtos e completos, id desconhecido e as duas
  precedências. 36 testes no total.

- **Tuner — fora da ordem do roadmap, a pedido direto.**
  Detecção de altura por YIN (De Cheveigné & Kawahara, 2002), não
  autocorrelação simples: testado ao vivo, a autocorrelação confundia a
  fundamental do mi grave (~82 Hz) com harmônicos mais fortes que ela
  (situação comum em captadores de guitarra reais) — o mesmo sinal
  sintético e puro dos primeiros testes não expunha esse problema, por não
  ter harmônico nenhum. YIN mede DIFERENÇA em vez de semelhança, e resolve
  isso de forma muito mais robusta. `--tuner` mostra nota/frequência/cents
  em tempo real, sem alterar o som.

## Regra

A ordem pode mudar, mas camadas não devem ser puladas: cada uma precisa ser
compreendida antes que outras sejam empilhadas por cima.
