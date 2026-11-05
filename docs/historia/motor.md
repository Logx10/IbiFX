# Capítulo 4 — Motor

> Branch `motor`. "Quero usar até 3 amps + caixas diferentes, igual no
> AmpliTube."

## O problema era de arquitetura

Até aqui, a cadeia do IbiFX era uma **fila**: cada módulo recebe a saída do
anterior. Três amps tocando juntos exigem que o sinal **se divida e volte a
se juntar**, e isso uma fila não faz.

O projeto até já tinha a peça certa, o `AudioGraph` (nós, conexões,
ordenação topológica). Só uma demo o usava. Todo o resto (o motor ao vivo,
os presets, o MIDI, a GUI e a inferência de rig) endereça a cadeia como uma
**lista plana de módulos, por índice**.

Como o `AI_GUIDELINES` pede para discutir antes de redesenhar, as opções
foram apresentadas antes de qualquer código:

1. **Migrar o motor para o `AudioGraph`.** É mais geral (qualquer
   roteamento), mas reescreve motor, presets, GUI e MIDI.
2. **Marcadores dentro da própria fila.** É o caminho de menor mudança.

A escolha foi a 2, com saída **mono** e Level e Fase por caminho.

## A ideia dos marcadores

A lista continua plana. O trecho paralelo é só uma *faixa* dela:

```
stomps  Split  AmpA CabA  Branch  AmpB CabB  Branch  AmpC CabC  Merge  rack
        |---- caminho A ---|----- caminho B ---|----- caminho C ---|
```

Os marcadores (`ParallelMarker`) não processam nada. Quem roteia é o
`ModuleChain`:

1. No **Split**, guarda uma cópia do sinal.
2. Cada caminho processa essa cópia.
3. Em cada **Branch** e no **Merge**, o caminho que terminou é somado num
   acumulador, com o volume e a fase do marcador que o *abriu*.
4. Depois do Merge, a fila segue com a soma.

Como a lista não mudou de forma, **presets, MIDI e índices funcionaram sem
nenhuma alteração**. Esse foi o argumento decisivo.

## As decisões que importam

- **Fase por caminho.** Duas caixas com microfones diferentes podem chegar
  com fases opostas, e somá-las cancela o grave. Num rig real, o técnico de
  som inverte um dos dois; aqui é um botão.
- **Caminho mudo não gasta CPU.** Bypass no Split ou num Branch silencia o
  caminho, e os módulos dele nem são processados.
- **Estrutura malformada não quebra o áudio.** Merge ou Branch soltos são
  ignorados, e um trecho sem Merge fecha no fim. A thread de áudio não pode
  lançar exceção, então a regra é degradar com calma.
- **Nada aloca na thread de áudio.** Os buffers do roteamento são
  reservados no `prepare()`, com folga para drivers que mandam blocos
  maiores.

## O catálogo

Os marcadores entraram no catálogo como a categoria **Routing** (Split,
Caminho, Merge), para a GUI oferecê-los como qualquer equipamento. O Merge é
o único equipamento sem painel: o volume de cada caminho mora no marcador
que o abre.

## Uma pendência honesta

Durante este trabalho, o teste `live_engine` falhou de vez em quando (~1 em
3), na verificação de que as duas tracks da gravação de reamp terminam do
mesmo tamanho. Parece uma corrida entre threads sem relação com o
roteamento: sem marcadores, o laço do `ModuleChain` faz exatamente o que
fazia antes. Mas **não foi confirmado** que a falha já existia. Fica
registrado para investigar.

**Atualização (branch `di-loop`):** confirmado. O mesmo teste, compilado a
partir da `main` (antes de qualquer mudança desta história), falhou 2 vezes
em 25, sempre nessa mesma verificação — contra 3 em 25 com tudo aplicado. A
falha é anterior ao roteamento e ao loop de DI: é a corrida que o próprio
`ReampRecorder.h` já descreve ("não zero matematicamente"). Continua aberta.
