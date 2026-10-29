# Interface desktop — proposta para a branch `Interface`

## Referência e objetivo

O AmpliTube 5 reúne cadeia de equipamentos, navegador, presets e ferramentas de
prática no mesmo espaço. Seu fluxo de arrastar equipamentos e sua cadeia de
sinal configurável são boas referências de interação. O objetivo do IbiFX é
dar a essas ações uma hierarquia clara, com identidade visual própria e os
recursos que o motor já oferece.

Referência: [página oficial do AmpliTube 5](https://www.ikmultimedia.com/products/amplitube5/).

## Limite entre interface e áudio

O motor (`src/ModuleChain.*` e `src/platform/`) não depende de SDL3, ImGui ou
`DesktopUI`.
A dependência aponta no sentido contrário: a interface recebe um `LiveEngine&`
e chama sua API de controle. Knobs e bypass enviam comandos para filas; o
callback consome esses comandos e processa a cadeia sem desenhar nada nem
esperar pela janela. Os medidores são publicados pelo motor em valores
atômicos, que a interface apenas lê.

Trocar um equipamento muda a estrutura da cadeia. A interface faz isso no
domínio de controle: para o dispositivo, aplica o rig e religa o áudio. Mudar
o layout ou o gesto de clique para arrastar não deve colocar operações de UI
no callback. A pausa que já ocorre nessa troca é um comportamento a avaliar
se a interação for aprimorada, separadamente do desenho da janela.

## O que já funciona no IbiFX

| Área | Situação atual |
| --- | --- |
| Cadeia | Rig Stomp → Amp → Cab → Rack, com ordem ajustável dentro de Stomp e Rack. |
| Equipamentos | Navegador à direita, por categoria e caráter; clique adiciona ou troca. |
| Controles | Painéis de equipamento, knobs, bypass e acesso aos módulos internos. |
| Presets e IRs | Salvar e carregar presets; importar IR por seletor ou arrastar um WAV. |
| Ferramentas | Afinador, gravação DI/processada e prática com backing track e metrônomo. |
| Áudio | Medidores e nomes dos dispositivos; erro de início visível e tentativa de reconexão. |

## Principal problema da janela atual

Presets, medidores, três ferramentas, rig e pedais ficam numa coluna vertical.
Na janela inicial de 1200 × 900 px, a pessoa precisa rolar para alternar entre
montar o som e tocar. O navegador tem largura fixa de 330 px. A ordem do sinal
existe nos dados, mas não fica sempre visível enquanto se ajustam os pedais.

## Primeira etapa proposta

Reorganizar apenas o layout, sem mudar o motor de áudio:

1. Barra superior compacta com preset, estado do áudio e medidores.
2. Faixa do rig sempre visível, na ordem do sinal.
3. Área central dedicada aos controles do equipamento selecionado.
4. Navegador de equipamentos à direita, usando o catálogo atual.
5. Ferramentas de prática e gravação em abas inferiores, para não ocupar a
   área dos pedais durante a montagem do som.

Essa etapa pode começar com uma aba `Rig` e uma aba `Prática`, reaproveitando os
widgets atuais. Antes de acrescentar gestos novos, conferir se carregar
presets, trocar equipamentos, importar IRs e ajustar parâmetros continua
funcionando com a nova disposição.

## Etapas posteriores, dependentes de decisão

- Busca no navegador e indicação mais clara do equipamento ativo.
- Arrastar e soltar equipamentos e reordenar a cadeia visualmente. Hoje a
  troca é por clique e a reordenação usa botões. O novo gesto pode reutilizar
  `applyRig()`; é preciso deixar clara a posição de destino e avaliar a pausa
  de áudio que já ocorre durante a reconstrução do rig.
- Seleção de entrada e saída de áudio dentro da janela. O dispositivo hoje é
  aberto pelo padrão do sistema.
- Modos de roteamento em paralelo, mixer de microfones, looper e multitrack
  exigem novos recursos de áudio. Se forem feitos, a separação entre UI e
  motor continua: a interface controla esses recursos pela API, sem entrar
  no callback.

O visual deve usar nomes, ilustrações e componentes próprios do IbiFX. A
referência é o fluxo de trabalho, não a reprodução de marcas ou imagens de
equipamentos de terceiros.
