# IRT120H original: estudo do circuito e plano de modelagem

## Objetivo

Trazer características do IRT120H original para o IbiFX por meio de sua
topologia, componentes e resposta dos controles. O modelo comportamental
inicial serve como referência de comparação. O estudo começou sem alteração
do DSP. Após autorização de implementação, a entrada e o Pre-Boost foram
integrados e testados: [resultado da primeira etapa](ironheart-entrada-implementacao.md).
V1A também foi integrado com polarização dinâmica: [implementação inicial](ironheart-v1a.md).
As [saídas passivas e suas cargas](ironheart-saidas-v1a.md) participam
da solução de V1A. [V1B/V2A/V2B e a correção do shunt R187](ironheart-v1b-v2.md)
foram integrados. [V3A/V3B e os tone stacks próprios](ironheart-v3-eq.md)
também foram incorporados; volumes/buffers e redes posteriores permanecem pendentes.

O usuário não tem acesso ao amplificador físico. Vídeos podem orientar a
escuta, mas não permitem atribuir toda diferença de áudio ao cabeçote:
guitarra, execução, gabinete, microfone e processamento também interferem.

## Referência encontrada e inspecionada

Cópia pública de **IRT120H Service Information**, identificada como projeto
de 2011, com quatro páginas. Inspeção por texto e imagens renderizadas:

- Página 1: identificação do documento.
- Página 2: esquema principal 9505, revisão 3, datado de 10/06/2011,
  incluindo placas de canal, potência e alimentação.
- Página 3: FX/reverb 9506, revisão 6, de 07/07/2011.
- Página 4: lista de peças do IRT120H (2011).

[PDF de serviço](https://www.musiker-board.de/attachments/laney_irt120h_sm-pdf.969316/).
O documento foi baixado para inspeção local, sem adicioná-lo ao Git.

O esquema dá uma referência concreta, mas não fornece todas as curvas dos
componentes, características magnéticas do transformador ou o programa do
reverb. Também será necessário conferir a conectividade dos relés/JFETs
para cada modo antes de afirmar quantos estágios ficam ativos em cada um.

## Diferenças concretas em relação ao modelo atual

| Bloco | Observação no documento | Consequência para o DSP |
| --- | --- | --- |
| Entrada/boost | TL072 e rede com potenciômetro de 100 kΩ | Rede linear derivada e implementada; TL072/JFET ideais e curva do knob estimada. |
| Pré | V1/V2 e seguidores de cátodo V3A/V3B | Um seguidor tem comportamento e carga diferentes de outro estágio de ganho; quatro curvas iguais não representam a rede. |
| EQ | Slope de 47 kΩ; treble de 470 pF; capacitores de 22 nF e componentes chaveados | Substituir o voicing britânico emprestado por uma rede derivada desse esquema. |
| Potenciômetros | Bass 250 kΩ log, Mid 25 kΩ linear, Treble 250 kΩ linear | Reproduzir faixas e curvas, além dos valores no meio do curso. |
| Master | Tone e Watts duplos de 220 kΩ; Dynamics em rede de retorno | Modelar os dois ramos e sua interação com a inversora/potência. |
| FX/reverb | Loop com três modos; processador FV-1 com memória externa | Reproduzir roteamento e níveis; o algoritmo do reverb exige outra referência. |

Não usar os valores acima para trocar constantes isoladas sem reconstruir
a conectividade: dois capacitores em série, por exemplo, não equivalem a
um único capacitor com o mesmo valor marcado em cada peça.

O texto técnico compartilhado anteriormente é uma lista de hipóteses de
modelagem, não um substituto do esquema. Contar triodos como estágios de
ganho ou assumir um passa-alta de 720 Hz no boost pode produzir outro som.

## Mapa de trabalho

```text
rede de entrada / Pre-Boost
  → V1 e roteamento de ganho/seleção
  → estágios V1/V2 e caminhos selecionados
  → seguidores V3 e EQs próprios
  → seleção de saída / volumes / recuperação de nível
  → FX Send/Return e reverb
  → inversora V4 / redes Tone e Watts
  → potência e realimentação com Dynamics
  → transformador / carga / gabinete ou rota DI
```

Esse mapa organiza o estudo; não é uma netlist validada de cada modo.
Uma netlist descreve quais terminais compartilham o mesmo nó elétrico.

## Primeiro exercício: rede linear de entrada e boost

O [exercício detalhado da entrada](ironheart-entrada-exercicio.md) traz as
conexões conferidas, transferências, valores de referência e pseudocódigo.

Antes da válvula, começar com os resistores, capacitores e o amplificador
operacional da entrada. Não introduzir ainda o modelo completo de válvulas.

**Entrada:** amostras mono, sample rate, posição de boost e estado da chave.
**Saída:** sinal que alimenta V1A, em uma escala interna documentada.
**Estado:** memória dos capacitores e valores suavizados dos controles.

1. Identificar todos os nós da entrada até a grade de V1A.
2. Para boost ligado/desligado, substituir a chave pelo caminho correspondente.
3. Derivar a transferência da rede, incluindo a posição do potenciômetro.
4. Calcular os polos/zeros e verificar os ganhos em DC e na banda da guitarra.
5. Discretizar a rede e comparar a resposta digital com a análise analógica.

Pseudocódigo que orientou a primeira implementação:

```text
prepare(taxa):
    preparar estados dos capacitores
    definir limites e suavização dos controles
    calcular coeficientes iniciais da rede

processar(bloco):
    ler os alvos dos controles
    para cada amostra:
        avançar a posição suavizada do boost
        atualizar a rede quando necessário
        processar acoplamento de entrada
        processar a realimentação do estágio de boost
        processar o acoplamento para V1A
        devolver o sinal, preservando os estados para o próximo bloco
```

O ganho do boost altera quanto V1A distorce; normalizar automaticamente cada
bloco destruiria essa relação. O estado dos capacitores precisa persistir
entre callbacks. `prepare()` pode criar memória; `process()` não deve alocar,
ler arquivos, acessar a rede ou usar mutex. Não adicionar bibliotecas nesta etapa.

**Como verificar:** zero na entrada, impulso, varredura de frequência,
ganho com boost desligado e nos extremos, troca da chave e igualdade entre
blocos de 1/128 amostras. Comparar transferência analógica/digital e verificar
estabilidade em 44,1/48/96 kHz. Só depois conectar a um estágio de válvula.

Perguntas para estudo: qual nó é a referência da realimentação? Quais
capacitores estão em série? A chave altera apenas o ganho ou também a
resposta em frequência? O que depende do potenciômetro?

## Etapas posteriores e critérios de conclusão

1. **Triodos e seguidores:** derivar os circuitos de cada estágio, escolher
   modelo de corrente com base em curvas de referência e validar o ponto de
   operação. Separar assimetria, acoplamento, bypass de cátodo e carga.
2. **Canais:** confirmar estados de relés/JFETs e reconstruir Clean/Rhythm/Lead.
   Validar roteamento e interação com os controles antes de ajustar timbres.
3. **EQ e switches:** resolver a rede completa com os componentes do desenho;
   comparar curvas para cada posição e extremos dos controles.
4. **Inversora/potência:** incluir Tone, Watts e Dynamics na topologia que
   eles controlam. Usar solução causal e estável para a realimentação; um
   laço algébrico não pode ser implementado apenas chamando módulos em círculo.
5. **FX e DI:** implementar modos/níveis e pontos de derivação como rotas de
   áudio; não adicionar controles decorativos sem consequência no sinal.
6. **Reverb:** manter claramente identificado como aproximado até obter
   referência suficiente do algoritmo/impulso e decaimento do original.

Não é necessário escolher WDF para o amp inteiro. Podemos começar pela
solução de redes lineares e modelos locais não lineares, avaliando precisão,
estabilidade e custo antes de escolher a técnica de cada bloco. Solvers
iterativos futuros precisam de limites de iteração e tratamento explícito
de não convergência.

## Referência de escuta

[Demonstração do IRT120H original por Andy K, publicada pela Laney em 2011](https://www.youtube.com/watch?v=F48N3HV3iGo).
Localizada como referência; não houve análise auditiva nem comparação
automática de seu áudio nesta etapa. Registrar os ajustes visíveis e a cadeia
de captação antes de usá-la para avaliar o modelo.

Priorizar trechos de guitarra isolada, knobs visíveis e comparação do mesmo
controle na mesma gravação. Evitar inferir características do cabeçote a partir
de mixagens completas. Comparações perceptivas devem alinhar volume e usar
gabinete/IR compatível. A revisão BCC é outra referência, pois a própria Laney
documenta alterações no boost; não misturar automaticamente seus circuitos.

Sem o amp físico, conseguiremos validar circuito, estabilidade e coerência
com referências. A afirmação de equivalência sonora exige evidência adicional.
