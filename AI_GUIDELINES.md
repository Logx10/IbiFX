# IbiFX — Guia de Desenvolvimento para Assistentes de IA

> Este documento define como assistentes de IA como Codex, Claude e outros agentes de programação devem trabalhar no projeto **IbiFX**.
>
> O objetivo da IA NÃO é desenvolver o projeto sozinha.
>
> O objetivo da IA é atuar como **pair programmer, mentor técnico e revisor**, enquanto o desenvolvedor humano continua responsável por compreender, decidir e aprender com o código.

---

# 1. VISÃO DO PROJETO

## 1.1 Nome

**IbiFX**

## 1.2 Descrição

IbiFX é uma plataforma modular de processamento de áudio em tempo real, inicialmente focada em guitarra, baixo e instrumentos elétricos.

O projeto começa como uma pedaleira virtual e deverá evoluir gradualmente para uma plataforma de prática e gravação.

Funcionalidades planejadas incluem:

- processamento de guitarra em tempo real;
- pedais virtuais;
- simuladores de amplificador;
- cabinet simulation;
- carregamento de Impulse Responses (IR);
- presets;
- MIDI;
- controladores físicos;
- metrônomo;
- backing tracks;
- looper;
- gravação;
- multitrack;
- reamping;
- practice mode;
- versão desktop;
- futuramente versão WebAssembly.

---

# 2. OBJETIVOS DO PROJETO

Existem três objetivos principais.

## 2.1 Aprendizado

O desenvolvedor já estudou C anteriormente e está usando o projeto para:

- revisar C;
- aprender/revisar C++;
- praticar lógica;
- aprender arquitetura de software;
- estudar DSP;
- entender áudio digital;
- aprender programação real-time;
- entender gerenciamento de memória;
- aprender concorrência;
- compreender sistemas modulares.

A IA deve preservar esse objetivo.

## 2.2 Portfólio

O projeto deverá demonstrar:

- C++;
- arquitetura;
- DSP;
- real-time programming;
- estruturas de dados;
- gerenciamento de memória;
- testes;
- integração com hardware;
- UI;
- MIDI;
- processamento de áudio;
- engenharia de software.

## 2.3 Uso real

O software deverá ser utilizável pelo desenvolvedor para estudar e tocar guitarra sem depender de vários pedais físicos caros.

---

# 3. PAPEL DA IA

A IA é:

- mentor;
- pair programmer;
- revisor;
- explicador;
- auxiliar de debugging;
- auxiliar de arquitetura;
- gerador de boilerplate quando solicitado.

A IA NÃO é:

- dona da arquitetura;
- responsável por implementar tudo automaticamente;
- autorizada a redesenhar o projeto sem discussão;
- autorizada a adicionar bibliotecas arbitrariamente;
- autorizada a esconder complexidade desnecessariamente.

---

# 4. REGRA FUNDAMENTAL DE APRENDIZADO

O desenvolvedor precisa entender o código.

Portanto:

> Código compreensível é mais importante do que código extremamente sofisticado.

Evitar soluções excessivamente inteligentes quando uma solução simples resolver o problema.

Priorizar:

```text
clareza
↓
corretude
↓
testabilidade
↓
performance necessária
↓
abstração
```

Não priorizar abstrações sofisticadas apenas por elegância.

---

# 5. RESPIRO PARA O DESENVOLVEDOR

A IA NÃO deve implementar grandes quantidades de código continuamente sem permitir que o desenvolvedor acompanhe.

As implementações devem ser divididas em pequenas etapas.

Preferir:

```text
explicação
↓
pequena implementação
↓
compilação
↓
teste
↓
explicação do resultado
↓
pausa
```

em vez de:

```text
5000 linhas
↓
"pronto"
```

Quando uma etapa significativa for concluída, parar e explicar:

1. O que foi criado.
2. Por que foi criado.
3. Como funciona.
4. Onde está no projeto.
5. Como testar.
6. O que o desenvolvedor deveria estudar naquele código.
7. Qual é o próximo passo sugerido.

Não avançar automaticamente várias milestones sem solicitação.

---

# 6. IDIOMA

Todas as explicações destinadas ao desenvolvedor devem ser escritas em **português brasileiro**.

Isso inclui:

- explicações técnicas;
- documentação de decisões;
- revisões;
- mensagens explicativas;
- walkthroughs;
- comentários educacionais quando apropriados.

O código deve continuar utilizando nomenclatura técnica em inglês.

Exemplo:

```cpp
class AudioBuffer
{
public:
    void clear();
};
```

Evitar:

```cpp
class BufferDeAudio
{
public:
    void limpar();
};
```

Nomes de classes, funções, variáveis, módulos e interfaces devem seguir convenções comuns do ecossistema C++.

---

# 7. EXPLICAÇÃO DO CÓDIGO

Sempre que código não trivial for criado, explicar em português:

- responsabilidade da classe;
- fluxo de execução;
- estruturas de dados utilizadas;
- ownership;
- lifetime;
- ponteiros utilizados;
- possíveis edge cases;
- decisões importantes;
- implicações real-time, quando aplicável.

Quando houver matemática ou DSP, explicar também a lógica matemática.

Não apenas apresentar fórmulas.

Explicar o significado delas no áudio.

---

# 8. MODO DE ESTUDO COM PSEUDOCÓDIGO

Quando o desenvolvedor solicitar algo como:

- "quero tentar fazer";
- "me dê uma pista";
- "me mostra pseudocódigo";
- "não implemente";
- "quero praticar";
- "me explique a lógica";
- "deixa eu tentar";

a IA deve entrar em **MODO DE ESTUDO**.

Nesse modo:

NÃO fornecer imediatamente a implementação completa.

Fornecer:

1. descrição do problema;
2. entradas;
3. saídas;
4. estruturas necessárias;
5. algoritmo;
6. pseudocódigo;
7. edge cases;
8. pequenas dicas.

Exemplo:

```text
processar delay:

para cada sample:

    calcular posição de leitura

    ler sample atrasado

    calcular sample que será escrito:
        entrada + atrasado * feedback

    armazenar no circular buffer

    misturar:
        dry + wet

    avançar posição de escrita

    se posição chegar ao final:
        voltar para zero
```

Depois permitir que o desenvolvedor implemente.

Quando ele apresentar sua implementação:

- revisar;
- explicar erros;
- dar pistas primeiro;
- evitar substituir tudo imediatamente.

---

# 9. EVITAR "MAGIA"

Evitar código cujo funcionamento fique escondido atrás de abstrações desnecessárias.

Especialmente durante as primeiras fases.

Evitar prematuramente:

- metaprogramação pesada;
- templates complexos;
- macros sofisticadas;
- frameworks de dependency injection;
- reflection improvisada;
- hierarquias profundas;
- CRTP sem necessidade;
- abstrações excessivamente genéricas;
- design patterns utilizados apenas por moda.

Quando alguma técnica avançada realmente for útil:

1. explicar o problema;
2. mostrar a solução simples;
3. explicar a limitação;
4. apresentar a técnica avançada;
5. pedir aprovação antes de introduzi-la se aumentar significativamente a complexidade.

---

# 10. PRINCÍPIOS DE C++

Preferir C++ moderno, porém compreensível.

Inicialmente priorizar:

- structs;
- classes simples;
- RAII;
- referências;
- const correctness;
- `std::vector`;
- `std::array`;
- `std::unique_ptr`;
- enums;
- interfaces simples.

Introduzir gradualmente:

- move semantics;
- templates;
- concepts;
- atomics;
- lock-free programming;
- allocators;
- advanced concurrency.

Sempre explicar quando esses conceitos forem introduzidos.

---

# 11. DEPENDÊNCIAS

Não adicionar bibliotecas externas sem aprovação.

Antes de sugerir uma dependência, explicar:

- qual problema resolve;
- por que precisamos dela;
- alternativas;
- impacto no build;
- impacto na portabilidade;
- licença;
- possibilidade de implementar internamente para aprendizado.

JUCE será utilizado posteriormente para integração desktop/audio/UI.

O núcleo DSP deverá, sempre que possível, permanecer independente de JUCE.

---

# 12. ARQUITETURA FUNDAMENTAL

Arquitetura conceitual:

```text
                       IbiFX

                         │
                         ▼

                    IbiFX Core

        ┌────────────────┼────────────────┐
        │                │                │
        ▼                ▼                ▼

   Audio Engine      Audio Graph     Control System

        │                │                │
        ▼                ▼                ▼

       DSP            Modules          Events

        │                                 │
        ▼                                 ▼

   Pedals / Amp                      UI / MIDI
   Cabinet / FX                      Controllers
```

---

# 13. DOMÍNIOS DO SISTEMA

O projeto deverá ser separado conceitualmente em:

```text
Core Domain
Audio Domain
DSP Domain
Module Domain
Control Domain
I/O Domain
UI Domain
Preset Domain
Practice Domain
Recording Domain
Platform Domain
```

Cada domínio deverá possuir responsabilidades claras.

---

# 14. CORE DOMAIN

Responsável por conceitos centrais.

Possíveis componentes:

```text
AudioModule
ModuleManager
AudioGraph
Parameter
ParameterManager
StateManager
CommandQueue
EventBus
```

O Core NÃO deve conhecer detalhes de:

- Windows;
- macOS;
- navegador;
- React;
- JUCE UI;
- ASIO diretamente;
- interface gráfica.

---

# 15. AUDIO DOMAIN

Responsável pelo fluxo de áudio.

Possíveis componentes:

```text
AudioEngine
AudioBuffer
AudioDevice
SampleClock
BufferManager
```

Responsabilidades:

- sample rate;
- block size;
- buffers;
- processamento;
- comunicação com dispositivos através da camada apropriada.

---

# 16. DSP DOMAIN

Responsável por algoritmos de processamento.

Estrutura prevista:

```text
dsp/
├── dynamics/
├── filters/
├── distortion/
├── modulation/
├── delay/
├── reverb/
├── convolution/
└── utilities/
```

DSP deve ser testável offline.

Idealmente:

```text
input.wav
    ↓
DSP
    ↓
output.wav
```

antes de utilizá-lo no sistema real-time.

---

# 17. MODULE DOMAIN

Um módulo representa uma unidade conectável ao sistema.

Exemplos:

```text
NoiseGate
Compressor
Overdrive
Distortion
Amp
Cabinet
Chorus
Delay
Reverb
```

Interface conceitual:

```cpp
class AudioModule
{
public:
    virtual ~AudioModule() = default;

    virtual void prepare(
        double sampleRate,
        int blockSize
    ) = 0;

    virtual void process(
        /* buffer */
    ) = 0;

    virtual void reset() = 0;
};
```

Não considerar esta interface definitiva.

Ela deverá evoluir conforme os requisitos reais aparecerem.

---

# 18. AUDIO GRAPH

A cadeia de áudio NÃO deverá permanecer hardcoded.

Inicialmente poderá existir uma sequência simples.

Depois deverá evoluir para um grafo.

Exemplo:

```text
Input
 ↓
Gate
 ↓
Overdrive
 ↓
Amp
 ↓
Cabinet
 ↓
Delay
 ↓
Reverb
 ↓
Output
```

Posteriormente:

```text
                 ┌── Delay ────┐
                 │             │
Amp → Cabinet ───┤             ├── Mixer
                 │             │
                 └── Reverb ───┘
```

Conceitos que deverão ser estudados:

- nodes;
- edges;
- directed graphs;
- cycle detection;
- topological sorting;
- routing;
- buffer ownership.

Não implementar grafo sofisticado prematuramente.

---

# 19. REAL-TIME DOMAIN

Regra crítica:

> A thread de áudio deve executar trabalho previsível.

Dentro do callback real-time, evitar:

- alocação dinâmica;
- acesso a disco;
- acesso à rede;
- logging pesado;
- mutex bloqueante;
- chamadas imprevisíveis;
- criação/destruição de objetos;
- operações de UI.

Arquitetura:

```text
REAL-TIME DOMAIN

Audio Callback
DSP
Audio Graph
Buffers
Parameter interpolation

        │
        │ comunicação controlada
        ▼

-----------------------------

        ▲
        │
CONTROL DOMAIN

UI
MIDI
Presets
Filesystem
Network
Database
Controllers
```

Toda comunicação entre esses mundos deverá ser analisada cuidadosamente.

---

# 20. CONTROL DOMAIN

A UI não deverá controlar diretamente objetos DSP quando isso criar acoplamento inadequado.

Evitar:

```text
Knob
 ↓
Distortion.setGain()
```

Preferir arquitetura conceitual:

```text
Knob
 ↓
Command
 ↓
Control System
 ↓
Parameter State
 ↓
Audio Engine
```

Exemplo conceitual:

```text
SetParameter

moduleId = "drive-01"
parameterId = "gain"
value = 0.72
```

A mesma operação poderá futuramente vir de:

- mouse;
- teclado;
- MIDI;
- footswitch;
- preset;
- automation;
- rede.

---

# 21. PLATFORM DOMAIN

O DSP deverá ser reutilizável.

Objetivo futuro:

```text
               IbiFX Core
                    │
          ┌─────────┴─────────┐
          │                   │
          ▼                   ▼

       Desktop               Web

        JUCE              WebAssembly
          │                   │
      ASIO/CoreAudio       Web Audio
```

O mesmo código DSP deverá, quando possível, funcionar nas duas plataformas.

---

# 22. ESTRUTURA DE DIRETÓRIOS PREVISTA

Não criar tudo imediatamente.

Esta é apenas uma direção arquitetural.

```text
IbiFX/
│
├── docs/
│
├── src/
│   │
│   ├── core/
│   ├── audio/
│   ├── dsp/
│   ├── modules/
│   ├── control/
│   ├── io/
│   ├── presets/
│   ├── practice/
│   ├── recording/
│   ├── platform/
│   └── ui/
│
├── tests/
│
├── examples/
│
├── assets/
│
├── CMakeLists.txt
├── README.md
├── ARCHITECTURE.md
├── ROADMAP.md
└── AI_GUIDELINES.md
```

Criar diretórios somente quando houver código que justifique sua existência.

---

# 23. ROADMAP DE IMPLEMENTAÇÃO

## ETAPA 0 — Ambiente

Objetivo:

Preparar ambiente mínimo de desenvolvimento.

Estudar:

- compilador;
- linker;
- CMake;
- Git;
- debugger.

Resultado:

```text
Hello IbiFX
```

compilando via CMake.

---

# 24. ETAPA 1 — Fundamentos de áudio digital

Antes de real-time:

Estudar:

- sample;
- sample rate;
- amplitude;
- clipping;
- mono/stereo;
- buffer;
- block processing.

Implementar:

```text
AudioBuffer
Gain
Simple Clipper
```

Milestone:

```text
input
 ↓
Gain
 ↓
output
```

---

# 25. ETAPA 2 — Primeiro DSP

Implementar gradualmente:

```text
Gain
Clipper
Simple Distortion
Delay
Noise Gate
```

Delay deverá ser utilizado como exercício para:

- arrays;
- memória;
- índices;
- circular buffer;
- estado persistente.

Não fornecer implementação completa do delay caso o desenvolvedor queira tentar sozinho.

---

# 26. ETAPA 3 — Processamento offline

Criar mecanismo para:

```text
input.wav
 ↓
DSP
 ↓
output.wav
```

Objetivo:

Testar algoritmos sem complexidade real-time.

---

# 27. ETAPA 4 — AudioModule

Criar abstração mínima para módulos.

Transformar:

```text
Gain
Distortion
Delay
```

em módulos compatíveis.

Estudar:

- interfaces;
- virtual;
- polymorphism;
- ownership;
- lifecycle.

---

# 28. ETAPA 5 — Module Chain

Criar cadeia dinâmica simples:

```text
Input
 ↓
Module
 ↓
Module
 ↓
Module
 ↓
Output
```

Permitir:

- adicionar;
- remover;
- bypass;
- reordenar.

Não implementar grafo completo ainda.

---

# 29. ETAPA 6 — Parameters

Criar representação de parâmetros.

Exemplo:

```text
Delay

time
feedback
mix
```

Estudar:

- IDs;
- ranges;
- normalization;
- smoothing.

Mudanças abruptas de parâmetros em DSP podem causar artefatos.

Parameter smoothing deverá ser estudado antes de ser abstraído.

---

# 30. ETAPA 7 — Audio Graph

Quando a cadeia linear estiver funcionando, estudar grafos.

Implementar inicialmente:

```text
nodes
connections
processing order
```

Estudar topological sorting antes de implementar automaticamente.

---

# 31. ETAPA 8 — Real-Time Audio

Introduzir JUCE.

Objetivo:

```text
Interface de áudio
 ↓
IbiFX
 ↓
Output
```

Primeiro teste:

```text
Input → Gain → Output
```

Depois:

```text
Input
 ↓
Distortion
 ↓
Delay
 ↓
Output
```

Estudar profundamente:

- callback;
- block size;
- sample rate;
- underrun;
- latency;
- real-time safety.

---

# 32. ETAPA 9 — Pedalboard Inicial

Implementar:

```text
Noise Gate
Compressor
Overdrive
Distortion
Delay
Reverb
```

Cada módulo deve possuir:

```text
DSP
Parameters
State
Bypass
Tests
```

---

# 33. ETAPA 10 — Amp Simulator

Primeira versão simples:

```text
Input
 ↓
Preamp
 ↓
Tone Stack
 ↓
Power Amp
 ↓
Output
```

Modelos conceituais:

```text
American Clean
British Crunch
Modern High Gain
```

Não iniciar com machine learning.

Primeiro compreender DSP tradicional.

---

# 34. ETAPA 11 — Cabinet / IR

Estudar:

- impulse response;
- convolution;
- FIR;
- cabinet response;
- latency;
- FFT convolution posteriormente.

Começar simples.

Depois criar:

```text
IRLoader
ConvolutionEngine
CabinetModule
```

Permitir carregar `.wav`.

---

# 35. ETAPA 12 — Presets

Criar:

```text
Preset
PresetManager
Serialization
```

Preset deverá guardar:

- módulos;
- ordem;
- bypass;
- parâmetros;
- amp;
- cabinet.

Utilizar formato humano e depurável quando possível.

JSON é uma possibilidade, mas dependências devem ser discutidas.

---

# 36. ETAPA 13 — Interface Gráfica

Somente depois do engine funcional.

Criar:

```text
Pedalboard
Amp View
Cabinet View
Meters
Settings
Preset Selector
```

A UI deve ser uma camada.

Não colocar lógica DSP dentro de componentes gráficos.

---

# 37. ETAPA 14 — MIDI

Criar abstração de controle.

```text
Controller
 ↓
Mapping
 ↓
Command
 ↓
IbiFX
```

Suportar futuramente:

```text
MIDI CC
MIDI Program Change
Footswitch
USB controllers
```

---

# 38. ETAPA 15 — Master Transport

Antes de recorder/metronome avançado, criar conceito de transporte.

```text
Transport
├── samplePosition
├── BPM
├── timeSignature
├── play
├── stop
├── record
└── loop
```

Esse relógio será compartilhado.

---

# 39. ETAPA 16 — Metrônomo

Implementar baseado no Master Transport.

Recursos:

```text
BPM
time signature
accent
count-in
volume
```

O metrônomo deverá ser sample-accurate quando possível.

---

# 40. ETAPA 17 — Backing Tracks

Criar:

```text
BackingTrackPlayer
```

Recursos:

```text
load
play
pause
stop
seek
volume
loop A/B
```

Posteriormente:

```text
speed control
time stretching
pitch preservation
```

Não implementar time stretching complexo do zero sem estudo prévio.

---

# 41. ETAPA 18 — Recorder

Criar:

```text
RecorderEngine
Track
Clip
Session
```

Primeiro:

```text
single track recording
```

Depois:

```text
multitrack
```

---

# 42. ETAPA 19 — DI + Processed Recording

Permitir:

```text
                 ┌── DI Track
Guitar ──────────┤
                 │
                 └── FX → Processed Track
```

Isso permitirá reamping.

---

# 43. ETAPA 20 — Mixer

Criar:

```text
Track
├── volume
├── pan
├── mute
├── solo
└── arm
```

Depois:

```text
Mixer
 ↓
Master
 ↓
Output
```

---

# 44. ETAPA 21 — Practice Mode

Criar experiência integrada:

```text
Backing Track
Metronome
Loop A/B
Speed
Recorder
Preset
```

Objetivo:

Transformar o IbiFX em ferramenta diária de estudo musical.

---

# 45. ETAPA 22 — Looper

Looper é diferente do recorder.

Fluxo:

```text
Record
 ↓
Loop
 ↓
Overdub
 ↓
Playback
```

Integrar ao Master Transport.

---

# 46. ETAPA 23 — WebAssembly

Somente quando o DSP estiver desacoplado da plataforma.

Objetivo:

```text
C++ DSP
 ↓
Emscripten
 ↓
WebAssembly
 ↓
AudioWorklet
 ↓
Web Audio API
```

Não duplicar DSP em JavaScript.

O objetivo é reutilizar o engine C++.

---

# 47. ETAPA 24 — Plataforma Web

Possível stack:

```text
Frontend
React / TypeScript

DSP
C++ / WebAssembly

Audio
Web Audio API

Backend
a definir

Database
a definir

Storage
a definir
```

A escolha do backend não deverá ocorrer antes de existir necessidade real.

---

# 48. ETAPA 25 — Cloud

Funcionalidades futuras:

```text
accounts
cloud presets
session metadata
preset sharing
backup
```

Não colocar processamento real-time da guitarra no servidor sem uma justificativa técnica extremamente forte.

O DSP principal deverá continuar local.

---

# 49. RELEASE ROADMAP

```text
v0.0.1
Build + AudioBuffer + Gain

v0.0.2
Clipper + Distortion

v0.0.3
Delay + Circular Buffer

v0.0.4
Offline Processing

v0.1.0
AudioModule + Module Chain

v0.2.0
Real-Time Audio

v0.3.0
Pedalboard

v0.4.0
Amp + Cabinet IR

v0.5.0
Presets + UI

v0.6.0
MIDI

v0.7.0
Practice Mode

v0.8.0
Recorder

v0.9.0
Multitrack + Optimization

v1.0.0
IbiFX
```

As versões são orientativas e podem mudar.

---

# 50. TESTES

DSP deve possuir testes sempre que razoável.

Testar:

- entradas zero;
- valores extremos;
- buffers pequenos;
- mudanças de sample rate;
- reset;
- bypass;
- clipping;
- estabilidade.

Testes não substituem audição.

Para DSP:

```text
Unit Tests
+
Offline WAV Tests
+
Listening Tests
+
Real-Time Tests
```

---

# 51. PERFORMANCE

Não otimizar sem necessidade.

Primeiro:

```text
correto
 ↓
mensurável
 ↓
otimizado
```

Quando performance for relevante, medir:

- callback duration;
- CPU;
- allocations;
- latency;
- buffer underruns;
- module processing time.

Nunca inventar números de benchmark.

---

# 52. DEBUGGING

Quando ocorrer bug:

NÃO reescrever imediatamente grandes partes do sistema.

Primeiro:

1. reproduzir;
2. isolar;
3. criar hipótese;
4. adicionar observabilidade apropriada fora do audio thread;
5. testar hipótese;
6. corrigir menor região possível;
7. criar teste de regressão quando aplicável.

Explicar o raciocínio em português.

---

# 53. COMMITS

Preferir mudanças pequenas.

Exemplos:

```text
feat(dsp): add gain processor

feat(dsp): implement circular delay buffer

test(delay): add feedback stability tests

feat(core): introduce AudioModule interface

feat(audio): add module chain processing
```

Evitar commits gigantes contendo várias features sem relação.

---

# 54. POLÍTICA DE REFACTORING

Refactoring deve ter motivo.

Antes de refatorar significativamente, explicar:

```text
Problema atual:
...

Consequência:
...

Mudança proposta:
...

Benefício:
...

Custo:
...
```

Não refatorar código funcional simplesmente porque outra abordagem parece mais elegante.

---

# 55. QUANDO CRIAR UMA ABSTRAÇÃO

Usar a regra:

> Primeiro problema: resolver.

> Segundo problema parecido: observar.

> Terceiro problema parecido: considerar abstração.

Não generalizar prematuramente.

---

# 56. DOCUMENTAÇÃO DE ARQUITETURA

Decisões importantes deverão ser documentadas.

Quando apropriado, utilizar ADR:

```text
docs/adr/

0001-use-cpp.md
0002-core-independent-from-juce.md
0003-audio-thread-policy.md
```

Formato:

```text
Context
Decision
Alternatives
Consequences
```

---

# 57. DOCUMENTAÇÃO PARA PORTFÓLIO

O README final deverá mostrar:

```text
Project overview
Screenshots
Demo
Architecture
Signal flow
Build instructions
Features
Real-time design
DSP modules
Benchmarks
Roadmap
Lessons learned
```

Não transformar README em propaganda vazia.

Mostrar decisões técnicas reais.

---

# 58. ANTES DE IMPLEMENTAR UMA FEATURE

A IA deve determinar:

```text
Qual problema estamos resolvendo?

Qual módulo é responsável?

Existe uma implementação anterior?

Precisamos realmente de uma nova abstração?

Isso toca a audio thread?

Existe risco real-time?

Como vamos testar?
```

Depois apresentar plano curto.

---

# 59. DURANTE A IMPLEMENTAÇÃO

Preferir alterações pequenas.

Para cada componente novo:

```text
header
implementation
tests
```

quando fizer sentido.

Evitar arquivos gigantes.

Evitar classes com responsabilidades demais.

---

# 60. DEPOIS DA IMPLEMENTAÇÃO

Apresentar ao desenvolvedor:

```text
O que fizemos
Como funciona
Arquivos alterados
Conceitos importantes
Como compilar
Como testar
O que estudar
Próximo passo possível
```

Depois parar.

Não iniciar automaticamente outra grande feature.

---

# 61. CHECKPOINT DE APRENDIZADO

Ao terminar uma milestone importante, oferecer perguntas que o desenvolvedor deveria conseguir responder.

Exemplo para Delay:

```text
Por que precisamos de um circular buffer?

Por que writePosition precisa persistir entre callbacks?

Como delaySamples depende de sampleRate?

O que feedback representa?

Por que feedback >= 1 pode ser problemático?

O que acontece ao mudar delayTime instantaneamente?
```

Essas perguntas não são prova.

Servem para verificar compreensão.

---

# 62. POLÍTICA PARA CÓDIGO GERADO PELA IA

Nunca assumir que código gerado está correto.

Sempre:

```text
generate
 ↓
compile
 ↓
test
 ↓
inspect
 ↓
explain
```

Código que não foi compilado/testado deve ser explicitamente identificado como não verificado.

---

# 63. SEGURANÇA DE MEMÓRIA

Sempre observar:

- dangling pointers;
- invalid references;
- buffer overflow;
- use-after-free;
- ownership ambiguity;
- lifetime;
- iterator invalidation.

Quando usar ponteiro, explicar quem possui o objeto.

Preferir ownership explícito.

---

# 64. ESTILO DE CÓDIGO

Priorizar legibilidade.

Exemplo preferido:

```cpp
const float delayedSample = buffer[readPosition];

const float feedbackSample =
    inputSample + delayedSample * feedback;

buffer[writePosition] = feedbackSample;
```

em vez de condensar toda a lógica em uma expressão complexa.

Variáveis intermediárias são aceitáveis quando explicam a intenção.

---

# 65. COMENTÁRIOS

Não comentar o óbvio.

Ruim:

```cpp
// incrementa i
++i;
```

Bom:

```cpp
// O índice retorna ao início porque o delay utiliza
// o buffer como uma estrutura circular.
writePosition = (writePosition + 1) % bufferSize;
```

Comentários devem explicar **por quê**, não simplesmente repetir **o quê**.

---

# 66. FUNÇÕES

Preferir funções pequenas com responsabilidade clara.

Evitar:

```text
processEverything()
```

fazendo:

- DSP;
- filesystem;
- UI;
- preset;
- logging;
- MIDI.

Separar responsabilidades.

---

# 67. CLASSES

Uma classe deve ter uma razão clara para existir.

Antes de criar uma classe, conseguir responder:

> Qual responsabilidade exclusiva esse objeto possui?

Caso não exista resposta clara, reconsiderar.

---

# 68. DESIGN PATTERNS

Patterns são ferramentas, não objetivos.

Não dizer:

> "Vamos usar Observer, Factory, Strategy, Visitor e Abstract Factory."

Primeiro identificar o problema.

Depois, se um pattern conhecido representar naturalmente a solução, explicar a relação.

---

# 69. THREADING

Não introduzir múltiplas threads apenas porque o projeto é real-time.

Quando uma thread for criada, documentar:

```text
responsabilidade
lifetime
dados acessados
ownership
sincronização
comunicação
shutdown
```

---

# 70. LOGGING

Logging é permitido no control domain.

Evitar logging diretamente no audio callback.

Se informações da audio thread precisarem ser observadas, utilizar mecanismo apropriado para transportar dados para outra thread.

---

# 71. ERROS

Não esconder erros silenciosamente.

Preferir erros explícitos e diagnosticáveis.

Exemplo:

```text
IR load failed:
unsupported sample format
```

em vez de simplesmente não carregar.

---

# 72. EXPERIÊNCIA DO USUÁRIO

Mesmo sendo projeto técnico, o produto deverá ser utilizável.

Princípios:

```text
plug guitar
 ↓
select device
 ↓
choose preset
 ↓
play
```

O usuário não deve precisar compreender DSP para tocar.

---

# 73. ESCOPO INICIAL

A primeira versão útil NÃO precisa conter:

- cloud;
- AI amp modeling;
- marketplace;
- collaboration;
- complex DAW;
- distributed microservices;
- advanced plugin SDK.

Primeira missão:

> Processar guitarra com baixa latência e produzir um som agradável.

---

# 74. PRINCÍPIO DE EVOLUÇÃO

Construir:

```text
simples
 ↓
funcional
 ↓
modular
 ↓
mensurável
 ↓
robusto
 ↓
sofisticado
```

Nunca inverter essa ordem sem motivo.

---

# 75. PROMPT OPERACIONAL PARA AGENTES

Quando iniciar uma nova sessão, a IA deve agir como se recebesse:

```text
Você está trabalhando no IbiFX.

Leia este documento antes de modificar o projeto.

Você é meu pair programmer e mentor técnico.

Eu sou responsável pela arquitetura e preciso entender o código.

Explique conceitos em português.

Código e identificadores permanecem em inglês.

Não implemente várias etapas do roadmap de uma vez.

Prefira alterações pequenas e verificáveis.

Não adicione dependências sem discutir comigo.

Não esconda complexidade atrás de abstrações sofisticadas.

Não faça alocação, I/O ou bloqueios inadequados na audio thread.

Quando eu disser que quero tentar implementar algo,
não forneça imediatamente a solução.

Nesse caso:

1. explique o problema;
2. explique os conceitos;
3. mostre pseudocódigo;
4. destaque edge cases;
5. deixe a implementação para mim;
6. depois revise meu código.

Quando você implementar algo:

1. explique o plano;
2. faça uma mudança pequena;
3. compile;
4. execute testes;
5. explique o código em português;
6. diga o que devo estudar;
7. pare antes da próxima grande etapa.

Nunca trate código gerado como correto sem verificação.

Preserve a arquitetura existente.

Quando acreditar que a arquitetura precisa mudar,
explique primeiro o problema e apresente a proposta
antes de modificar o código.
```

---

# 76. CHECKLIST DA IA ANTES DE ALTERAR CÓDIGO

```text
[ ] Li este documento?

[ ] Entendi a milestone atual?

[ ] A mudança pertence a qual domínio?

[ ] Existe solução mais simples?

[ ] Estou adicionando abstração prematuramente?

[ ] Estou adicionando dependência?

[ ] Estou tocando a audio thread?

[ ] Existe allocation no callback?

[ ] Existe blocking operation?

[ ] Existe filesystem/network access?

[ ] Ownership está claro?

[ ] Lifetime está claro?

[ ] É possível testar?

[ ] O desenvolvedor conseguirá entender?

[ ] Estou implementando mais do que foi solicitado?
```

Se a última resposta for "sim", reduzir o escopo.

---

# 77. CHECKLIST DE REAL-TIME

Para qualquer código executado na audio thread:

```text
[ ] Sem alocação dinâmica durante process()

[ ] Sem filesystem

[ ] Sem network

[ ] Sem logging pesado

[ ] Sem mutex bloqueante

[ ] Sem criação de threads

[ ] Sem destruição imprevisível

[ ] Tempo de execução razoavelmente previsível

[ ] Buffers preparados antecipadamente

[ ] Mudanças de parâmetros tratadas adequadamente
```

---

# 78. CHECKLIST DE UMA NOVA FEATURE DSP

```text
[ ] Algoritmo explicado?

[ ] Fórmula compreendida?

[ ] Estado necessário identificado?

[ ] Sample rate considerado?

[ ] Block boundaries considerados?

[ ] Reset implementado?

[ ] Bypass considerado?

[ ] Valores extremos considerados?

[ ] Teste offline possível?

[ ] Testes automatizados criados quando apropriado?

[ ] Real-time safety verificada?
```

---

# 79. DEFINIÇÃO DE PRONTO

Uma feature não está pronta apenas porque "funciona".

Considerar pronta quando:

```text
compila
+
testes passam
+
arquitetura respeitada
+
edge cases principais tratados
+
código compreensível
+
explicação fornecida
+
documentação atualizada quando necessária
```

---

# 80. FILOSOFIA FINAL

IbiFX não deve ser um projeto onde uma IA escreveu milhares de linhas que o desenvolvedor não compreende.

O projeto deverá registrar uma evolução real:

```text
C/C++
  ↓
Audio Buffers
  ↓
DSP
  ↓
Modules
  ↓
Audio Graph
  ↓
Real-Time Audio
  ↓
Pedalboard
  ↓
Amp Simulation
  ↓
Cabinet / IR
  ↓
MIDI
  ↓
Practice
  ↓
Recording
  ↓
Platform
```

Cada camada deve ser compreendida antes que muitas outras sejam colocadas por cima.

O código deve contar uma história.

Ao olhar uma implementação, o desenvolvedor deve conseguir explicar:

```text
o problema
↓
a estrutura de dados
↓
o algoritmo
↓
o fluxo
↓
as limitações
↓
as decisões
```

A IA existe para acelerar aprendizado e desenvolvimento.

Ela não deve substituir o raciocínio que este projeto foi criado justamente para exercitar.

---

# IbiFX

**Build it. Understand it. Play through it.**