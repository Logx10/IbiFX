# Capítulo 2 — Amps

> Branch `amps`. Os dois Marshall do John Frusciante.

## Como começou

Depois dos pedais, o pedido foi "os pre amps dele, o Jubilee e outro que não
lembro". A pesquisa resolveu o "outro". Segundo a Guitar Player de 2006 e
outras fontes, de *Californication* a *Stadium Arcadium* ele dividia a
guitarra entre dois amps ao mesmo tempo: um **Marshall Major 200 W** e um
**Silver Jubilee 2555**, cada um no seu par de 4x12.

## O que cada um é, em termos de circuito

- **Major (Brit 200).** O circuito de um Plexi, com muito mais headroom:
  válvulas KT88 e um transformador enorme. Na receita isso vira pouco ganho
  de preamp, power amp folgado e quase sem sag. É um limpo alto e firme, que
  só abre quando é empurrado.
- **Silver Jubilee (Brit Silver).** O que define o Jubilee é uma rede de
  **LEDs vermelhos e diodos de silício** entre os estágios do preamp,
  montada para clipar *assimetricamente*, gerando harmônicos pares como num
  overdrive. Várias fontes que trabalham pelo esquema dizem que, sem esses
  diodos, o Jubilee seria praticamente um amp limpo.

## A primeira dificuldade: um painel que dependia de posição

Todos os amps do catálogo tinham **exatamente 7 módulos**, e o painel
apontava os knobs por posição: Gain no módulo 2, o Master no `modules[6]`.
O Jubilee tem um módulo a mais (a clipagem). Com índices fixos, o Master
dele apontaria para o PowerAmp, e **nada avisaria**: o knob só mexeria na
coisa errada.

Por isso o primeiro commit do branch é um refactor. O painel passa a achar
cada módulo pelo **tipo**. O Presence também deixou de ser uma lista de ids
e aparece em todo amp cuja receita define presence. É o tipo de mudança que
vale fazer *antes* de precisar, num commit separado, para ficar claro que o
comportamento dos amps antigos não mudou.

## A segunda dificuldade: calibrar volume

Um amp novo que sai 15 dB mais baixo que os outros parece "fraco", mesmo
que o timbre esteja certo. Por isso medi o nível de **todos** os amps
(palhetada fraca, média e forte):

| Amp | Média, antes | Média, depois |
|---|---|---|
| American Clean (referência de limpo) | -11,4 dBFS | — |
| Brit Plexi / 800 (referência de ganho) | ~-14,5 dBFS | — |
| Brit 200 | **-26,4 dBFS** | -11,6 dBFS (master 0,2 → 1,1) |
| Brit Silver | -16,9 dBFS | -14,6 dBFS (master 0,2 → 0,26) |

## Uma lição que só apareceu depois

Na hora de separar esta história em commits, a verificação automática pegou
um bug: o **Brit 800 também tinha ido para 0,26**. Para calibrar o Silver,
eu tinha trocado o texto "linha do PowerAmp + master 0,2" pelo master novo.
Só que o Brit 800 tem **exatamente a mesma linha** de PowerAmp, e a troca
acertou os dois. A medição não pegou porque, naquela hora, eu só imprimi os
amps novos.

A correção já está aplicada nos commits deste branch: o Brit 800 continua
com 0,2. As duas lições:

1. Uma troca de texto que espera um lugar só precisa *conferir* que achou
   um lugar só.
2. Depois de mexer no catálogo, meça o catálogo inteiro, não só o que
   mudou.

## O que ficou para depois

O Frusciante tocava os dois amps **ao mesmo tempo**. Os amps deste branch
ainda entram um por vez no rig. Tocar os dois juntos depende dos caminhos
paralelos, que vêm nos capítulos **Motor** e **UI**.
