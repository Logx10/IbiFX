# Capítulo 3 — IR (gabinetes)

> Branch `ir`. "Verifique por que a IR Celestion está estourando o áudio."

## Uma pergunta, duas causas

Este capítulo é um bom exemplo de *debugging* de áudio: o sintoma
("estourando") tinha **duas causas independentes**, e só uma delas era de
volume.

## Causa 1: volume

O primeiro passo foi olhar o arquivo, e ele estava perfeito: 24 bits, mono,
pico em 0,98, sem DC. O problema não era o arquivo; era o que a convolução
**faz** com ele.

IRs comerciais vêm normalizadas pelo **pico da amostra**. Só que a
convolução *soma* milhares de amostras. Para entender o efeito, medi a
resposta em frequência de cada IR da pasta:

| IR | Pico da resposta | Na faixa da guitarra |
|---|---|---|
| Factory (as do projeto) | +0,2 dB | -2 a 0 dB |
| Celestion V30 | +20 dB @ 240 Hz | +12 a +15 dB |
| Mesa Rectifier | +20 dB @ 120 Hz | +8 a +19 dB |
| JCM900 sm57 | +18 dB @ 4,9 kHz | vale em 1 kHz |

Os amps foram calibrados com as IRs Factory, perto de 0 dB. A Celestion
somava +15 dB justamente onde a guitarra tem energia.

### Escolher a regra foi a parte difícil

A correção é normalizar cada IR ao carregar. Mas normalizar *por qual
critério*? Simulei três regras nas IRs reais **antes** de escrever código:

| Regra | O que acontece |
|---|---|
| 0 dB em 1 kHz (a do gerador das Factory) | quebra nos JCM900: eles têm um vale em 1 kHz e ficariam com **+23 dB** no agudo |
| pico da resposta em 0 dB | nunca amplifica, mas deixa os JCM900 **20 dB mais baixos** que os outros |
| **energia média de 80 Hz a 5 kHz** | as Factory mudam ~1 dB; as comerciais ficam todas no mesmo volume |

Ficamos com a terceira. A Celestion foi de +20 para +5,6 dB no pico.

## Causa 2: CPU

A Celestion tinha outra particularidade: **9.600 taps** (200 ms) ou
**24.000** (500 ms). As outras IRs tinham entre 800 e 2.300. O
`ConvolutionEngine` fazia a conta direta, multiplicando todos os taps a cada
amostra:

| IR | CPU só do gabinete (Release, 48 kHz) |
|---|---|
| Celestion 200 ms | 33% de um núcleo |
| Celestion 500 ms | **81%**: dropout garantido |

Dropout soa como estalo, e estalo, para quem está tocando, é "estourando".

### A saída fácil, recusada

Cortar a IR em ~100 ms resolveria. Antes de decidir, medi a cauda: esse
"Mix" tem ambiência de sala, com energia em -24 dB depois de 42 ms. Cortar
**mudaria o som**. A decisão foi resolver a conta.

### Convolução por FFT, sem latência

Convolução no tempo é multiplicação na frequência, e com FFT custa N·log N
em vez de N². O problema da FFT é trabalhar em blocos, e esperar um bloco é
**latência**, o que num pedal ao vivo é o que mais se sente. O truque:

- A **cabeça** da IR (128 taps) continua na conta direta e responde na
  hora.
- A **cauda** vai por FFT, em partes de 128. A parte *k* só enxerga entradas
  de pelo menos *k*·128 amostras atrás. Então a contribuição da cauda para o
  próximo bloco pode ser calculada **quando o bloco anterior fecha**, e fica
  pronta exatamente quando é necessária.

O resultado é latência zero. A Celestion de 500 ms caiu de **81% para
2,4%** de um núcleo, com a resposta em frequência idêntica.

### Detalhes que pesaram

- A FFT foi escrita no projeto (`Fft.h`, radix-2), sem biblioteca, como o
  `AI_GUIDELINES` pede para peças que valem a pena aprender.
- O `operator*` de `std::complex`, no GCC, chama uma rotina lenta que trata
  NaN e infinito (`__mulsc3`). Nos laços quentes, a multiplicação complexa é
  escrita à mão.
- O sinal é real, então o espectro é espelhado: guardar metade dos bins
  corta metade da conta.
- Um teste exigia saída **exatamente 0,0** depois do fim da IR. A FFT deixa
  ~1e-8 de arredondamento. O limite virou 1e-6, com o motivo escrito no
  teste.

## O que ficou para depois

A cauda é calculada numa amostra a cada 128. Com um buffer de driver menor
que 128, alguns callbacks pagam a conta e outros não. Em 2,4% de média isso
não deve pesar, mas se aparecer estalo com buffer muito pequeno, é por aí.
