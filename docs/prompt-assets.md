# Como gerar os assets de equipamento no ChatGPT

Fluxo para o chat do ChatGPT. O chat não controla bem tamanho, transparência
ou centralização, e não precisa: isso é corrigido depois, no projeto. Ele só
precisa acertar o que não tem como consertar depois (lista abaixo).

## O que o chat precisa acertar

1. **Knob visto exatamente de cima**, como um círculo, sem a lateral
   aparecendo. O app gira a imagem; com perspectiva, o knob fica torto.
2. **Corpo sem knobs**: só o buraco vazio onde o knob entra.
3. **LEDs apagados no corpo**, ou só o soquete vazio.
4. **Texto certo**: nome, rótulos dos knobs e "ibiFX". Nenhuma marca de
   fabricante.

## O que é corrigido no projeto

Tamanho, recorte, fundo transparente, centralização do knob, LED aceso e
apagado do mesmo tamanho, nomes dos arquivos. Pode mandar a imagem como o
chat entregar, com fundo branco ou cinza.

## Passo 1 — abrir uma conversa por equipamento

Cole isto no começo da conversa, trocando os campos entre `{chaves}` pelos
dados de `assets-equipamentos.md`:

```text
Vamos criar as peças de um pedal de guitarra para um app chamado ibiFX. Vou pedir uma peça por vez.

O equipamento:
- Tipo: {pedal | cabeçote de amplificador | equipamento de rack}
- Nome: {Noise Gate}
- Inspirado em: {Boss NS-2} — pode lembrar o formato e as cores do original, mas sem nenhuma logo ou nome do fabricante. A única marca é "ibiFX".
- Cor: {corpo branco fosco com painel superior preto}
- Knobs, da esquerda para a direita: {THRESHOLD, RELEASE}

Regras para todas as imagens:
- Vista de frente, reta, sem perspectiva e sem inclinação.
- Fundo liso, sem mesa, chão ou cenário.
- Só os textos que eu pedir, escritos exatamente assim.
- Estilo foto realista, iluminação de estúdio suave.

Responda só "ok" e espere o primeiro pedido.
```

## Passo 2 — pedir o corpo

```text
Gere o corpo do equipamento.
- Onde ficam os knobs, desenhe só um buraco redondo vazio, escuro, SEM knob dentro.
- Em volta de cada buraco, marquinhas de escala em arco, como num relógio: começando perto das 7h30, passando pelas 12h no meio e terminando perto das 4h30.
- Embaixo de cada buraco, escreva "MIN" à esquerda e "MAX" à direita.
- Acima de cada buraco, o nome do knob.
- O LED aparece APAGADO, só o soquete escuro.
- {Só pedal:} No lugar do footswitch, só um furo redondo vazio com arruela, sem botão.
- Escreva o nome "{Noise Gate}" grande e "ibiFX" pequeno.
```

Se algum knob tiver escala conhecida (ver tabela no fim), troque "MIN"/"MAX"
pelos números daquele knob.

## Passo 3 — pedir o knob

```text
Agora gere UM knob sozinho, visto exatamente de cima, como se a câmera estivesse em cima dele olhando para baixo.
- Deve parecer um círculo perfeito. Não pode aparecer a lateral do knob.
- Estilo: {tampa branca com saia preta}.
- Uma linha indicadora reta apontando exatamente para cima (12h).
- Sem sombra para nenhum lado.
```

Se o resultado vier em perspectiva, responda:

```text
Ainda está em perspectiva, dá para ver a lateral. Quero vista de cima pura, igual a um ícone circular plano visto de frente.
```

## Peças compartilhadas (uma vez só, numa conversa separada)

LED e footswitch servem para todos os pedais e já existem em
`assets/gear/_shared/`. Só gere de novo se quiser trocar o visual.

```text
Gere um LED redondo de painel de pedal de guitarra, visto exatamente de cima, com aro de metal escuro. Fundo liso.
Primeiro a versão ACESA, vermelha brilhante. Depois, a mesma imagem com o LED APAGADO, mesmo enquadramento.
```

```text
Gere um footswitch cromado de pedal de guitarra, visto exatamente de cima, com porca sextavada em volta. Fundo liso.
```

Basta o footswitch normal. Ligado, o app desenha um anel vermelho aceso em
volta dele; não há imagem de botão pressionado.

## Onde colocar os arquivos

Coloque as imagens originais numa pasta e me passe o caminho. Elas são
processadas com `tools/gear_assets.py` (fundo, recorte, tamanho, posição
dos knobs) e o resultado vai para `assets/gear/<id>/`, com o id do
catálogo (ex.: `assets/gear/stomp.gate/`). O nome dos arquivos originais
não importa; eles são
renomeados no projeto.

## Escalas que podem ter números

Um número errado no desenho engana quem usa o app; MIN/MAX nunca está errado.

| Knob | Escala |
|---|---|
| Release do Noise Gate | 10 a 1000 ms |
| Time dos delays | MIN / MAX (cada delay tem uma faixa) |
| Threshold do Noise Gate | MIN / MAX (o motor trabalha em amplitude linear, não em dB) |
| Amps | 0 a 10 |
| Demais | MIN / MAX |
