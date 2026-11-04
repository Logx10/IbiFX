# Capítulo 1 — Pedais

> Branch `pedais`. Do compressor de metal ao rig do John Frusciante.

## Como começou

O pedido foi simples: "implementa um compressor de metal". O projeto já
tinha um `Compressor` e um pedal em cima dele (o Orange Squeeze), então a
pergunta virou outra: **o que um compressor de metal tem que o genérico não
tem?**

A resposta é **ataque rápido sem perder a palhetada**. Antes de um ampli
high gain, o compressor precisa deixar os palm mutes e as notas abertas no
mesmo volume. Para isso, o ataque tem que ser de 1 ms. Só que um ataque
desses esmaga justamente o que dá definição ao riff, a primeira fração de
milissegundo de cada palhetada. A saída clássica é a **compressão
paralela**: misturar um pouco do sinal seco de volta.

```
saida = seco + mix * (comprimido - seco)
```

O `mix` entrou no `Compressor` com padrão 1.0, para nada que já existia
mudar de som. O pedal, **Iron Clamp**, é só uma receita no catálogo. É a
regra do `GearLibrary`: um equipamento novo é uma entrada nova, não uma
classe nova.

### A primeira lição: medir antes de confiar no ouvido (ou na intuição)

O Blend começou em 0.8, e "20% de seco" parecia pouco. O teste mediu outra
coisa: a palhetada forte saía **4 dB acima** da entrada, e a nivelação caía
para 7 dB. A conta explica. Com 22 dB de redução numa palhetada forte, o
caminho comprimido fica tão baixo que 20% de seco pesa quase o mesmo. Com
0.9, a palhetada forte sai a +0.4 dB e a fraca sobe 11 dB a mais. O número
certo veio da conta, não do chute.

## O EQ de 7 bandas

O pedido seguinte foi um EQ de pedal no espírito do GE-7. Aqui não dava
para fazer só receita: **o projeto não tinha nenhum filtro peaking**. O
`ToneStack` é um circuito passivo em que os knobs interagem de propósito.
Um EQ gráfico é o contrário, porque cada knob mexe só na sua faixa.

O `GraphicEQ` são sete biquads em série, com as fórmulas do *Audio EQ
Cookbook* (Bristow-Johnson). Duas coisas valem a pena entender:

- **Por que Q = 1.4.** Com as bandas a uma oitava uma da outra, esse Q dá
  uma oitava de largura a cada sino, e os vizinhos se encontram no meio.
- **Por que o expoente é /40 e não /20.** O ganho no centro sai de A²: metade
  vem do numerador, metade do denominador.

O teste confere o que a teoria promete: ±12 dB exatos no centro de cada
uma das 7 bandas (±0,2 dB), e nenhuma mudança com tudo em 0.

## O rig do Frusciante

Daqui em diante, o trabalho passou a ser **pesquisa antes de código**. Para
cada pedal do board dele, procurei medições publicadas do circuito e só
depois escolhi os números.

| Pedal no IbiFX | Original | O número que guiou a receita |
|---|---|---|
| Orange Turbo | Boss DS-2 | Turbo II: +10 dB estreitos em ~900 Hz, -10 dB em ~2,5 kHz |
| Green Siberian | Big Muff "Green Russian" | corte de médios em 1 kHz, raso (~6 dB, contra ~13 dB nos americanos) |
| Bakersfield Fuzz | Mosrite Fuzzrite | dois estágios misturados fora de fase pelo Depth |
| Ensemble Chorus | Boss CE-1 | chorus lento, mistura fixa em 50% |
| Chapel Reverb | EHX Holy Grail | modo hall |

O EQ de 7 bandas, feito pouco antes, virou a peça que faltava. O "Turbo"
do DS-2 é exatamente a banda de 800 Hz de um `GraphicEQ` depois da
clipagem.

### As dificuldades

- **Um pedal mudo.** O Bakersfield saía a -100 dB. O bias do
  `AsymmetricClipper` é medido na escala da *entrada*. Com bias 0,6 e drive
  12, `tanh(12·(x+0,6))` fica preso em +1 para qualquer palhetada, ou seja,
  um sinal constante, que é silêncio. A correção foi copiar a estrutura do
  Round Fuzz: ganho antes, bias menor.
- **Um teste errado, não um pedal errado.** O teste "volume a ±3 dB em
  toda nota" reprovava o Orange Turbo, porque ele sobe +10 dB em 880 Hz.
  Só que esse *é* o som do Turbo. O teste passou a exigir a **média** de
  três notas a ±3 dB. Quando um teste reprova o comportamento que você quer,
  quem precisa mudar é o teste.

## Wah, phaser e flanger

Três pedais do rig precisavam de DSP que não existia: filtro varrido e
modulação com realimentação.

- **Wah (WH-10).** É um passa-banda cujo centro anda com o pé, de 350 Hz a
  2,2 kHz, em escala de oitavas. O filtro é um **SVF na forma TPT**
  (Zavalishin), porque o centro muda *a cada amostra*. Um biquad comum com
  os coeficientes trocados no meio do som pode estalar ou disparar. O Q
  padrão (3) imita o pico largo do WH-10, que é de amplificador operacional,
  mais largo que o do indutor de um Cry Baby.
- **Phaser (Phase 90).** São 4 all-pass e dois vales. O detalhe bonito: a
  ElectroSmash mede os vales em 58 e 340 Hz, e pela conta dos all-pass eles
  ficam em 0,414·fc e 2,414·fc. As duas medições dão **fc ≈ 141 Hz**, que é
  exatamente o limite imposto pelos resistores de 24k do circuito. Quando
  duas fontes independentes fecham na mesma conta, dá para confiar no
  número.
- **Flanger (Electric Mistress).** Uma linha de atraso de 0,8 a 6 ms (medida
  num Deluxe), LFO triangular, e o Color como realimentação. O teste prevê o
  pente à mão: 625 Hz cancela, 1250 Hz passa, e com Color 0,5 o pico sobe
  exatamente 1/(1−0,5).

O Flanger precisava da mesma interpolação de Hermite do Chorus. Ela foi
para `Interpolation.h`, em vez de ser copiada.

Os três passaram em todos os testes **na primeira execução**. Não foi
sorte: cada teste foi escrito a partir da conta, antes de rodar o código.

## O que ficou para depois

- O wah tira bastante o grave (o Mi solto cai ~11 dB com o pedal no meio).
  Isso é normal num wah, mas vale julgar de ouvido.
- O Electric Mistress original tem um modo "Filter Matrix" (LFO parado),
  que não foi feito.
- Os pedais novos usam o painel genérico de knobs; ainda não têm skin.
