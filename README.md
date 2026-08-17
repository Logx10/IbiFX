# IbiFX

> Modular real-time guitar effects and practice platform built with C++.

O IbiFX pretende se tornar uma plataforma modular de processamento de áudio em
tempo real, voltada inicialmente para guitarra, baixo e outros instrumentos
elétricos.

## Estado atual

**Estágio inicial.** No momento o repositório contém:

- a configuração de build com CMake, dividida em dez alvos:
  - `ibifx_core` — biblioteca estática com o código de processamento;
  - `ibifx` — executável de demonstração;
  - oito executáveis de teste, registrados no CTest;
- o contrato `AudioModule`, classe base abstrata com `name()`, `process()`,
  `prepare()` e `reset()`, que também guarda e expõe os parâmetros do módulo;
- o `Parameter`, com id estável, faixa, clamp e forma normalizada;
- o `SmoothedValue`, que faz um parâmetro caminhar até o novo valor em vez de
  saltar, eliminando o clique da mudança abrupta;
- leitura e escrita de `.wav` sem dependência externa, e o processamento
  offline de arquivo que permite finalmente **ouvir** o resultado;
- a camada de plataforma (`AudioDevice`, `LiveEngine`) que toca em tempo real
  pela placa de som, isolada do núcleo de DSP;
- o `ModuleChain`, cadeia linear montada em tempo de execução: adicionar,
  remover, reordenar e colocar módulos em bypass;
- o `AudioGraph`, roteamento em grafo com ordenação topológica e rejeição de
  ciclos, para caminhos paralelos que a cadeia linear não consegue expressar;
- três módulos de DSP que implementam o contrato, cada um com sua bateria
  de testes:
  - `GainProcessor` — aplica um ganho (volume) a um buffer de amostras;
  - `Clipper` — hard clipping simétrico: corta as amostras que passam de um
    teto, para cima e para baixo;
  - `SoftClipper` — saturação suave via `tanh`: comprime o sinal contra o
    limite em vez de decepá-lo, e nunca sai da faixa `[-1, +1]`;
  - `Delay` — eco com realimentação sobre um buffer circular, com tempo,
    feedback e mix;
- a documentação inicial de arquitetura e roadmap.

Os três primeiros são **funções de transferência** aplicadas amostra a amostra
— `f(x) = ganho * x`, `f(x) = corta em ±teto` e `f(x) = tanh(drive * x)`.
Trocar o efeito é trocar o formato dessa curva, e é essa ideia que sustenta a
pedaleira inteira que virá depois.

O `Delay` é de outra natureza: ele é o primeiro módulo com **memória**. Para
devolver o que aconteceu meio segundo atrás, precisa guardar meio segundo de
áudio, e esse trecho atravessa a fronteira dos blocos. Daí vêm o `prepare()`
— que recebe o sample rate e aloca o buffer circular longe da thread de áudio
— e o `reset()`, que descarta o eco pendente sem tocar nos parâmetros.

Como todos herdam de `AudioModule`, a cadeia é montada em tempo de execução e
processada sem que ninguém precise saber quem está dentro dela:

```cpp
ModuleChain chain;
chain.add(std::move(gain));       // a cadeia assume a posse
chain.add(std::move(clipper));

chain.setBypassed(1, true);       // desliga o clipper sem removê-lo
chain.move(0, 1);                 // inverte a ordem
chain.process(buffer);
```

A cadeia é **dona** dos módulos, guardados como `std::unique_ptr`. Como esse
tipo não pode ser copiado, é impossível dois lugares acharem que possuem o
mesmo módulo — e quando a cadeia morre, os módulos morrem junto.

Cada módulo expõe seus parâmetros pela mesma interface, o que permite ajustar
qualquer um deles sem conhecer o tipo concreto:

```cpp
if (Parameter* p = module.findParameter("gain"))
    p->setValue(4.0f);            // ou p->setNormalized(0.75f)
```

O `Parameter` carrega um **id estável** (é ele que vai gravado no preset do
usuário), uma **faixa** e a conversão para a forma **normalizada** de 0 a 1 —
a moeda comum de MIDI CC, knobs de tela e automação, que falam em posição
relativa e não em unidades de cada efeito.

Valor fora da faixa é ajustado para a borda, como o batente de um knob físico.
Isso resolve na origem um risco antes apenas documentado: o teto do `Clipper`
não pode mais ser negativo, porque a faixa começa em zero.

Mudar um parâmetro de uma vez cria um degrau na forma de onda, e uma
descontinuidade é um estalo de banda larga — o ouvido escuta um clique. Girando
um controle continuamente, os cliques viram o chiado conhecido como *zipper
noise*.

O `SmoothedValue` resolve isso trocando o salto por uma caminhada: o valor
pedido vira um **alvo**, e a cada amostra o valor atual se aproxima um pouco
dele. A rampa padrão é de 20 ms — longa o bastante para eliminar o clique,
curta o bastante para o controle continuar parecendo instantâneo.

Ganho, teto do `Clipper`, drive do `SoftClipper` e o feedback e o mix do
`Delay` são suavizados. O **tempo** do `Delay` não é: movê-lo gradualmente
alteraria a taxa de leitura das amostras antigas, o que é literalmente uma
mudança de altura do som. O efeito é real e desejado em delays analógicos, mas
fazê-lo direito exige interpolação entre amostras vizinhas — assunto próprio.

Sem `prepare()`, a suavização fica inativa e os valores saltam. É o
comportamento correto para processamento offline, e foi o que permitiu ligar a
suavização sem alterar nenhum dos testes já existentes.

## Processando um arquivo

O executável aceita arquivos `.wav`. Para gerar um sinal de teste e ouvi-lo
passando pela cadeia:

```sh
./build/ibifx --generate entrada.wav
./build/ibifx entrada.wav saida.wav
```

A cadeia padrão é `Gain -> SoftClipper -> Delay`: um pedal de drive seguido de
eco, com a distorção antes do delay para que os ecos repitam o som já
distorcido. Invertida, o delay produziria ecos limpos que depois seriam
distorcidos juntos, e o resultado vira uma pasta.

### Ajustando o som

Os parâmetros da cadeia são opções de linha de comando:

```sh
./build/ibifx audio/guitar.wav audio/limpo.wav  --gain 1 --no-drive --no-delay
./build/ibifx audio/guitar.wav audio/crunch.wav --gain 2 --drive 1.5 --mix 0.15
./build/ibifx audio/guitar.wav audio/fuzz.wav   --gain 8 --drive 40 --feedback 0.6
```

| opção | o que faz | faixa | padrão |
|---|---|---|---|
| `--gain N` | volume antes da distorção | -8 a 8 | 6.0 |
| `--drive N` | quantidade de distorção | 0 a 100 | 4.0 |
| `--time N` | atraso do eco em segundos | 0 a 2 | 0.28 |
| `--feedback N` | quantas repetições | 0 a 0.95 | 0.45 |
| `--mix N` | quanto do eco na saída | 0 a 1 | 0.35 |
| `--no-drive` | tira a distorção da cadeia | | |
| `--no-delay` | tira o eco da cadeia | | |

Valores fora da faixa **param na borda**, como o batente de um knob. O programa
imprime o valor que cada módulo realmente guardou, então pedir `--feedback 5`
e ver `0.95` na tela é o comportamento esperado, não um erro silencioso.

Opção desconhecida, valor faltando ou número inválido interrompem com mensagem
clara em vez de seguir com um parâmetro errado.

O sinal gerado são quatro notas que decaem, com fundamental e dois harmônicos.
Não é uma guitarra, mas tem dinâmica suficiente para a saturação responder ao
volume — que é o comportamento descrito no `SoftClipper`.

A leitura aceita PCM de 16, 24 e 32 bits e float de 32 bits, mono ou estéreo.
A escrita produz PCM de 16 bits, o formato mais universal. Formatos fora dessa
lista geram erro explícito dizendo o que foi encontrado.

O processamento é **offline**: não há entrada de áudio em tempo real, nem
interface gráfica. Cada canal passa pela mesma cadeia, com `reset()` entre
eles para que o eco de um não vaze para o outro, e em blocos de 512 amostras
para simular o que um dispositivo real entregaria.

### Cadeia ou grafo

O `ModuleChain` é uma fila: cada módulo recebe a saída do anterior. Cobre a
maior parte de uma pedaleira e é o caminho mais simples.

O `AudioGraph` existe para o que a fila não expressa — roteamento paralelo:

```text
                 ┌── Delay ────┐
                 │             │
Amp → Cabinet ───┤             ├── saída
                 │             │
                 └── Reverb ───┘
```

Aqui o sinal se divide e volta a se somar. Numa fila o reverb receberia a
saída do delay em vez do sinal limpo. É o roteamento de qualquer efeito em
paralelo, de loop de efeitos e de mistura entre seco e processado.

Num grafo a ordem de processamento deixa de ser óbvia: um nó só pode ser
processado depois de todos que o alimentam. O `AudioGraph` resolve isso com
**ordenação topológica** (algoritmo de Kahn) e recalcula a ordem apenas quando
a estrutura muda, nunca a cada bloco.

Ciclos são recusados no `connect()`, antes de a conexão existir. Um ciclo não
tem ordem válida — para processar A é preciso B, e para processar B é preciso
A — e em áudio é realimentação sem atraso, que estoura instantaneamente. Isso
não proíbe realimentação: o `Delay` realimenta o tempo todo, mas dentro de si
e com atraso de várias amostras.

## Tocando ao vivo

```sh
./build/ibifx --live 20      # toca por 20 segundos
./build/ibifx --devices      # testa a camada de audio sem hardware
```

O modo ao vivo abre o dispositivo em duplex — entrada da interface, efeitos,
saída — e alterna o bypass do drive a cada 2 segundos, para demonstrar que a
cadeia muda com o áudio rodando.

> **Microfonia.** Se a entrada for o microfone embutido e a saída o
> alto-falante embutido, o som volta para a entrada e realimenta. Com ganho e
> distorção no caminho isso vira um apito alto em segundos. **Use fones.**

O `--devices` abre o dispositivo pelo backend nulo do miniaudio, que gera os
blocos pelo relógio sem hardware nenhum. Serve para confirmar que a camada de
plataforma funciona em máquina sem placa de som, sem permissão de microfone ou
em servidor — e é o mesmo backend que a suíte de testes usa.

### Duas threads, sem bloqueio

Quem gira um knob e quem processa o áudio são threads diferentes, e isso muda
as regras. A thread de áudio tem prazo: a 48 kHz com blocos de 128 amostras,
são **2,67 ms** para entregar o bloco. Quem chega atrasado produz estalo.

Por isso ela nunca pode **esperar**. Um mutex resolveria a corrida, mas
travar pode bloquear — e o escalonador do sistema pode suspender a thread de
controle justamente enquanto ela segura o cadeado, fazendo a de áudio esperar
uma fatia inteira de escalonamento. É a inversão de prioridade clássica.

A solução tem duas partes:

- **`Parameter` guarda um `std::atomic<float>`.** Mudar um valor de outra
  thread passa a ser seguro, e como o tipo é lock-free a leitura na thread de
  áudio continua sendo uma instrução comum.
- **`CommandQueue`**, uma fila circular sem bloqueio, para o que não cabe num
  número: bypass, reset. O controle deposita com `pushCommand()`; o
  `process()` retira e aplica tudo no início do bloco, antes de tocar em
  qualquer amostra. Fila cheia recusa em vez de esperar.

```cpp
Command command;
command.type = Command::Type::SetBypass;
command.moduleIndex = 1;
command.value = 1.0f;

chain.pushCommand(command);   // thread de controle, nunca bloqueia
```

O `ModuleChain` é deliberadamente **não-copiável e não-movível**: mover a
cadeia mudaria o endereço dos índices atômicos que a thread de áudio pode
estar lendo naquele instante.

Mudanças estruturais — `add`, `remove`, `move` — continuam restritas ao
domínio de controle e não podem ser feitas com o áudio rodando: elas realocam
o vetor de módulos. Trocar a montagem durante o som exigirá construir a cadeia
nova fora e trocá-la por ponteiro atômico.

O `AudioGraph` ainda não tem fila de comandos.

### Sobre performance e threads

Medido com a cadeia `Gain -> SoftClipper -> Delay`, blocos de 128 amostras a
48 kHz, compilado com `-O2`:

```text
orcamento por bloco : 2666.67 us
custo da cadeia     :    3.026 us
uso da CPU          :     0.11 %
```

Caberiam cerca de 880 cadeias dessas em um núcleo. **Performance não é o
gargalo**, e paralelizar o processamento seria otimizar algo que já sobra
quase mil vezes.

Threads extras dentro do callback tendem a piorar, não melhorar: sincronizar
custa dezenas de microssegundos e adiciona variação imprevisível. Trocar um
trabalho estável por um mais rápido na média que ocasionalmente estoura o
prazo é um mau negócio quando cada estouro é audível. Aqui previsibilidade
vale mais que vazão — é o que o §69 do AI_GUIDELINES já orientava.

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

### Dependências

Uma só, e ela já está no repositório:

**[miniaudio](https://miniaud.io) 0.11.25** — acesso ao dispositivo de áudio
do sistema. Domínio público (ou MIT-0, à escolha), um único header em
`third_party/miniaudio/`. Não precisa ser instalada nem baixada no build.

Foi escolhida por três motivos: é um arquivo, não impõe modelo de projeto, e
fica confinada a `src/platform/`. O núcleo de DSP não a enxerga — o alvo
`ibifx_core` sequer tem o diretório dela no caminho de include, então incluí-la
por engano dentro do core quebra o build. Isso torna o princípio 1 do
ARCHITECTURE verificável pelo compilador, e não apenas uma intenção.

Trocar por JUCE ou RtAudio depois significa reescrever
`src/platform/AudioDevice.cpp`, sem tocar em uma linha de DSP.

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

## Como rodar os testes

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

O `--output-on-failure` faz o CTest mostrar a saída do teste apenas quando
ele falha. Sem essa opção você recebe só "Passed" ou "Failed", sem saber
qual verificação divergiu.

Para rodar o executável de teste direto, sem o CTest — útil para ver todas
as verificações, inclusive as que passaram:

```sh
./build/tests/test_gain_processor
./build/tests/test_clipper
./build/tests/test_soft_clipper
./build/tests/test_audio_module
./build/tests/test_module_chain
./build/tests/test_parameter
./build/tests/test_delay
./build/tests/test_smoothed_value
./build/tests/test_wav_file
./build/tests/test_offline
./build/tests/test_audio_graph
./build/tests/test_command_queue
./build/tests/test_live_engine
```

Cada módulo tem seu próprio executável de teste, e não um binário único com
todos. Assim o CTest reporta `gain_processor` e `clipper` separadamente, e uma
falha já aponta o módulo culpado. Vale lembrar que cada arquivo de teste tem
seu próprio `main()` — dois deles não caberiam no mesmo executável.

O `test_audio_module` é diferente dos que testam um módulo só: em vez de
conferir o que um módulo calcula, ele testa o mecanismo que os une — se a
chamada virtual chega na implementação certa, se uma cadeia de tipos diferentes
processa na ordem correta, se destruir por ponteiro para a base é seguro e se
o ajuste genérico de parâmetros funciona sem conhecer o tipo concreto.

## Saída esperada

```text
IbiFX starting...

1. PARAMETROS DA CADEIA — descobertos, nao codificados

MODULO        ID          VALOR     FAIXA           NORMALIZADO
Gain          gain        1.00      -8 .. 8         0.56
Clipper       threshold   1.00      0 .. 2          0.50

2. AJUSTE GENERICO — setParameter("Gain", "gain", 4.0)

cadeia: Gain -> Clipper

entrada:                    0.00    0.10    0.20    0.25    0.30    0.50   -0.40   -0.90
saida:                      0.00    0.40    0.80    1.00    1.00    1.00   -1.00   -1.00

3. FORA DA FAIXA — o teto do Clipper vai ate 2.0

pedimos 99.0 e o parametro guardou 2.00

4. BYPASS — o clipper continua na cadeia, mas e pulado

cadeia: Gain -> Clipper (bypass)

entrada:                    0.00    0.10    0.20    0.25    0.30    0.50   -0.40   -0.90
saida:                      0.00    0.40    0.80    1.00    1.20    2.00   -1.60   -3.60

5. CORTE TROCADO — o clipper sai, o softclipper entra

cadeia: Gain -> SoftClipper

entrada:                    0.00    0.10    0.20    0.25    0.30    0.50   -0.40   -0.90
saida:                      0.00    0.38    0.66    0.76    0.83    0.96   -0.92   -1.00

no hard clipping, 1.20 / 2.00 / -3.60 viraram todos o mesmo valor.
no soft, eles continuam distinguiveis entre si.


6. DELAY — um impulso e seus ecos (10 Hz, 0.3 s, feedback 0.5)

impulso:                    1.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00
ecos:                       0.00    0.00    0.00    1.00    0.00    0.00    0.50    0.00    0.00    0.25    0.00    0.00    0.12

cada eco vale metade do anterior: 1.00, 0.50, 0.25, 0.12...
e por isso que feedback >= 1.0 nunca pararia de crescer.

7. O ESTADO ATRAVESSA OS BLOCOS — agora entra so silencio

silencio:                   0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00
ecos:                       0.00    0.00    0.06    0.00    0.00    0.03    0.00    0.00    0.02    0.00    0.00    0.01    0.00

nada entrou, e mesmo assim saiu som: e a memoria do delay.

8. RESET — descarta o eco pendente

apos reset:                 0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00

silencio absoluto: o reset esvaziou o buffer circular.


9. SMOOTHING — ganho indo de 1.0 para 0.0

sem prepare:                0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00
com rampa:                  0.90    0.80    0.70    0.60    0.50    0.40    0.30    0.20    0.10    0.00    0.00    0.00

sem rampa o valor cai de 1.00 para 0.00 entre duas amostras vizinhas.
esse degrau nao estava no sinal: o ouvido escuta um clique.
com rampa a queda leva 10 amostras e a onda continua continua.


10. GRAFO — dois caminhos paralelos somados

entrada:                    1.00    0.50   -0.25    0.10
A(x2) + B(x3):              5.00    2.50   -1.25    0.50

cada caminho recebeu o sinal ORIGINAL, nao a saida do outro.
1.00 virou 2.00 + 3.00 = 5.00, e nao 1.00 x 2 x 3 = 6.00.

11. CICLO — o grafo recusa realimentacao sem atraso

conectar C -> A fecharia um ciclo? sim
recusada: AudioGraph::connect: a conexao fecharia um ciclo

ordem de processamento: Gain(1) Gain(2) Gain(3) 


uso:
  ./build/ibifx                          demonstracao no terminal
  ./build/ibifx --generate saida.wav     gera um sinal de teste
  ./build/ibifx entrada.wav saida.wav    processa um arquivo
```

O demo monta **uma única cadeia** e a manipula cinco vezes, sem recompilar.

**Etapa 1** lista os parâmetros que existem na cadeia. Nada ali está escrito no
código do demo: ele pergunta a cada módulo quantos parâmetros tem e quais são.
Um módulo novo apareceria nessa tabela sozinho.

**Etapa 2** ajusta o ganho por `setParameter("Gain", "gain", 4.0)` — dois textos
e um número, sem mencionar o tipo `GainProcessor`. O resultado é um pedal de
drive: o ganho empurra o sinal contra o teto e o clipper corta o excedente. É
por isso que o botão "gain" de um pedal de distorção não é um controle de
volume; ele controla quantas amostras batem no limite, ou seja, quanta
distorção existe.

**Etapa 3** pede um teto de `99.0` para o `Clipper` e recebe `2.00` de volta,
porque é onde a faixa termina. O parâmetro protege o módulo.

**Etapa 4** põe o clipper em bypass. Ele continua na cadeia, apenas é pulado, e
o sinal sai estourado (`2.00`, `-3.60`) porque ninguém o segura.

**Etapa 5** troca o tipo de corte. Onde a entrada valia `1.20`, `2.00` e
`-3.60`, o hard clipping devolvia `1.00`, `1.00` e `-1.00`: três valores
distintos viraram o mesmo, e a informação se perdeu. O soft clipping devolve
`0.83`, `0.96` e `-1.00`, mantendo a hierarquia entre eles — é essa preservação
da diferença que o ouvido lê como definição, e é por isso que overdrive soa
encorpado onde fuzz soa chapado.

**Etapas 6 a 8** mostram o `Delay`, e usam um sample rate de 10 Hz — absurdo
para áudio, ideal para ver o mecanismo: com `time = 0.3 s` o atraso dá
exatamente 3 amostras, e os ecos cabem numa linha de terminal.

A entrada é um **impulso**, um único `1.00` seguido de silêncio. Onde ele
reaparecer é, literalmente, o atraso do módulo — e ele reaparece na posição 3,
depois 0.50 na 6, 0.25 na 9, 0.12 na 12. Cada eco vale metade do anterior,
porque `feedback = 0.5` multiplica o sinal a cada volta. É uma progressão
geométrica, e é exatamente por isso que `feedback >= 1.0` nunca pararia de
crescer — daí a faixa do parâmetro terminar em `0.95`.

A etapa 7 é a mais reveladora: entra **só silêncio** e mesmo assim sai som. Os
ecos que continuavam guardados no buffer circular seguem saindo, agora bem mais
baixos (`0.06`, `0.03`, `0.02`, `0.01`). É a memória do módulo, atravessando a
fronteira do bloco. Nenhum dos três módulos anteriores conseguiria fazer isso.

A etapa 8 chama `reset()` e o silêncio volta a ser absoluto — o buffer circular
foi zerado, sem que nenhum parâmetro fosse alterado.

**A etapa 9** mostra o smoothing. A entrada é um sinal constante de `1.00`, o
mais simples possível de ler: qualquer coisa que apareça na saída veio do
parâmetro, não do sinal. O ganho vai de `1.0` para `0.0` nos dois casos.

Sem `prepare()`, a queda acontece **entre duas amostras vizinhas** — a saída já
começa em `0.00`. Esse degrau não estava no sinal, e é ele que o ouvido escuta
como clique. Com a rampa, a mesma queda leva 10 amostras (`0.90`, `0.80`,
`0.70`...) e a onda permanece contínua. O valor final é idêntico nos dois
casos: o erro nunca esteve em *para onde* o parâmetro foi, e sim em *quão
rápido* chegou lá.

**As etapas 10 e 11** mostram o grafo. Dois ganhos recebem o mesmo sinal de
entrada e suas saídas são somadas: `1.00` vira `2.00 + 3.00 = 5.00`. Numa
cadeia linear o resultado seria `1.00 × 2 × 3 = 6.00`, porque o segundo módulo
receberia a saída do primeiro — e é exatamente essa diferença que justifica o
grafo existir.

A etapa 11 tenta fechar um ciclo e é recusada, com a mensagem dizendo o
motivo. Em seguida o grafo imprime sua ordem de processamento, calculada por
ordenação topológica.

Note também a amostra `0.25`: com ganho 4.0 ela cai exatamente em `1.0`, o
limite. Ela **não** é cortada — valores no teto são válidos, e há um teste
dedicado a essa fronteira.

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
