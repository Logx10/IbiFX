# Capítulo 6 — Loop de DI

> Branch `di-loop`. "Quero timbrar a guitarra sem precisar tá tocando ela
> direto."

## A ideia

Timbrar é girar um knob e ouvir o resultado. Com a guitarra no colo, cada
ajuste vira um ciclo: solta o knob, pega a palheta, toca o riff, volta pro
knob. E o riff **nunca sai igual duas vezes**, então fica difícil saber se o
som mudou por causa do knob ou da mão.

Os plugins de amp de ponta resolvem isso gravando a guitarra seca (a DI) uma
vez e tocando-a em loop pela cadeia. Foi isso que entrou no IbiFX: **Gravar
DI**, toca o riff, e o **espaço** toca e para.

## Seco, e antes da cadeia

Gravar o som já processado não serviria. O que se quer é ouvir o **mesmo**
riff passando por pedais e amps **diferentes**. Por isso o take é a guitarra
crua, e na reprodução ele **substitui a entrada**: vai para o lugar onde a
guitarra entraria, antes de qualquer módulo. É o mesmo princípio do reamp
(capítulos anteriores do projeto), só que em loop e sem passar pelo disco.

## Por que na memória

O `ReampRecorder` escreve arquivos numa thread própria, porque uma gravação
longa não cabe na memória. Aqui o take é curto (até 60 s, ~11 MB a 48 kHz) e
precisa ser relido do começo a cada volta do loop. Por isso ele mora num
vetor alocado no `prepare()`, e a thread de áudio só lê e escreve posições.

Quem muda o estado é **só a thread de áudio**. A interface deposita um
*pedido* (gravar, tocar, parar) num atômico, e o `process()` o aplica no
começo do próximo bloco. Existe um único escritor do take, a mesma ideia da
fila de comandos do `ModuleChain`, reduzida a um pedido por vez.

## As dificuldades foram de uso

- **O buraco no loop.** Entre apertar Gravar e a primeira nota sempre há
  silêncio, e outro depois da última. Em loop, isso vira um buraco a cada
  volta. O take é cortado sozinho: começa 20 ms antes da primeira amostra
  acima de -46 dBFS e termina 0,2 s depois da última. A parte esperta é
  *quando* isso é calculado: a primeira e a última amostra são anotadas
  **durante** a gravação (uma comparação por amostra). Varrer 60 s de áudio
  dentro do callback, na hora de parar, seria caro demais para a thread de
  áudio.
- **O estalo na emenda.** Um take que termina em +0,3 e recomeça em -0,2 dá
  um degrau de uma amostra para a outra: um estalo a cada volta. Há um fade
  de 5 ms no começo e no fim, e parar no meio desce em fade em vez de cortar.
- **O fluxo natural.** Gravar o riff e apertar espaço tem que já tocar,
  então o espaço durante a gravação fecha o take e começa a reprodução. Cada
  reprodução recomeça do início, para dois timbres serem comparados sobre o
  mesmo trecho.
- **O espaço não pode roubar a digitação.** Ele só vale na Pedaleira, e nunca
  enquanto um campo de texto está ativo, como o nome do preset.

## Um bug antigo, achado no caminho

A barra nova fica entre o editor e a cadeia, e para isso o editor precisa
saber quanto espaço deixar embaixo. Foi aí que apareceu um bug do capítulo
**UI**: o editor reservava a altura fixa de *uma* faixa para a cadeia, mas
com caminhos paralelos a cadeia cresce, e com 3 amps ela passava do fim da
janela. As duas alturas agora saem da mesma função, `signalChainHeight()`.

## Uma pendência resolvida

O capítulo **Motor** registrava que o teste `live_engine` falhava às vezes,
sem saber se a falha já existia. Agora se sabe: compilado a partir da `main`,
antes de qualquer mudança desta história, ele falhou 2 vezes em 25, sempre na
sincronia das tracks do reamp. A falha é anterior a tudo isto, e continua
aberta.

## O que ficou para depois

- Salvar o take em disco e carregar um `.wav` de DI como take (as DIs que o
  Gravador já salva em `recordings/` seriam ótimas para isso).
- A barra ainda não foi vista rodando quando este capítulo foi escrito: os
  testes cobrem o comportamento do áudio, não o desenho.
