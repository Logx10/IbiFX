# Capítulo 5 — UI

> Branch `ui`. Os caminhos paralelos na tela.

## A ideia

Com o motor pronto, faltava montar os 3 amps **arrastando equipamentos**,
vendo os caminhos um embaixo do outro, como no AmpliTube.

A cadeia na GUI era uma linha de cards sobre um "cabo". Agora, um trecho
Split ... Merge é desenhado assim:

```
            ┌─ [Split]   Amp A  Cab A ─┐
  IN ── ... ┼─ [Caminho] Amp B  Cab B ─┼─ [Merge] ── ... ── OUT
            └─ [Caminho] Amp C  Cab C ─┘
```

A faixa cresce com o número de caminhos, e o tamanho dos cards se ajusta
pela largura **e** pela altura disponíveis.

## O que tornou isso simples

Os caminhos são só faixas da lista plana (ver o capítulo **Motor**). Um
ponto de soltar dentro do caminho B insere num **índice global** da lista,
como qualquer outro ponto da linha. Inserir, mover e remover continuaram
sendo exatamente a mesma edição do preset. A GUI só aprendeu a *desenhar* o
trecho empilhado.

## As dificuldades foram de uso, não de código

- **O caminho vazio passava a guitarra crua.** Ao inserir o Split, o
  caminho B nascia vazio, e um caminho vazio passa o sinal seco. Você ouviria
  a DI somada ao amp até colocar outro. A solução: o caminho novo começa
  **mudo** e liga sozinho quando recebe o primeiro equipamento.
- **Três peças para montar à mão.** Inserir o Split já traz o segundo
  caminho e o Merge. "Caminho" acrescenta o terceiro.
- **Remover um marcador ligaria amps em série.** Por isso, remover o Split
  tira o trecho inteiro, remover um Caminho tira o caminho dele, e o Merge
  não sai sozinho.
- **Mover só o Merge desmontaria o trecho.** Os marcadores não podem ser
  arrastados, não dá para abrir Split dentro de Split, e o limite é de 3
  caminhos. O motor aceitaria mais; o limite é de interface e de CPU.

A Fase de cada caminho é um botão **Normal/Invertida**, não um knob, e a
biblioteca ganhou a aba **Rota**.

## Uma pendência honesta

Quando este branch foi feito, o layout dos caminhos empilhados **ainda não
tinha sido visto rodando**. Os testes automáticos cobrem o motor (o
roteamento e a soma), não o desenho. A primeira pessoa a abrir a tela vai
ser quem confirma se ficou bom.

## O que ficou para depois

- **Estéreo.** O Frusciante tocava um amp de cada lado. Os marcadores já são
  o lugar certo para um Pan; o que falta é o motor deixar de ser mono.
- **Efeitos só num caminho.** Já funciona (é só arrastar um pedal para
  dentro de um caminho), mas não foi testado de ouvido.
