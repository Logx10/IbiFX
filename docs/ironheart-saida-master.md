# IRT120H: fechamento do pré e master reduzido

Esta implementação fecha a saída do pré e substitui os filtros master
estimados. O alvo é o esquema original de 2011, páginas 2 e 3 do
[manual de serviço](https://www.musiker-board.de/attachments/laney_irt120h_sm-pdf.969316/).
O [guia de uso](ironheart-120.md) descreve os 28 controles funcionais e as
limitações. Não há alegação de equivalência medida com um cabeçote físico.

## Volumes e somadores

`IronheartOutputStage` possui rampas de posição dos volumes, seleção Clean
e três estados de filtro. Não empresta memória nem possui ponteiros para
outros módulos. O taper log estimado é `(81^posição − 1)/80`, com 10% no
meio do curso; a curva real dos potenciômetros não foi medida.

VR5/VR8 têm 1 MΩ. R52/R84 de 680 kΩ terminam no terra virtual do somador:
o segmento inferior fica em paralelo com esses resistores. A resistência
vista pelo EQ é `Rsuperior + (Rinferior || 680k)`; o ganho do cursor depende
dessa carga. `beginSample()` fornece a condutância equivalente ao EQ
**antes** de resolver o seguidor V3. Mover Volume também altera o EQ e a
carga do seguidor, em vez de somente multiplicar sua saída.

Em Rhythm, Q3 conduz e Q5 aterra o meio do ramo R5/R22; R5 de 330 kΩ
continua carregando o EQ. Em Clean, Q3/Q5 abrem e Q1 conduz: R5+R22 dão
660 kΩ ao somador, enquanto o cursor Rhythm fica aberto. O potenciômetro
de 1 MΩ continua ligado ao EQ. O ganho fixo do caminho Clean é 330/660=0,5.
JFETs são ideais; a troca interpola condutâncias/transferências por 20 ms,
sem simular os tempos de comutação físicos.

R50/R76 de 330 kΩ em paralelo com C17/C32 de 22 pF formam o feedback de
U2A/U4A. U4B é unitário; U2B soma os canais com R7 de 10 kΩ em paralelo
com C12 de 47 pF. O caminho selecionado contém a transferência:

```text
H(s) = ganho_do_cursor / [(1+s·330k·22p) (1+s·10k·47p)]
```

Os polos são discretizados pela regra trapezoidal. Duas inversões dão
polaridade final positiva. A excursão útil dos buffers é limitada a ±13 V,
uma estimativa para alimentação de ±15 V. Não se modelam slew rate,
recuperação de overload ou perda do terra virtual em saturação.
Os antigos fatores Clean de recuperação e Volume² foram removidos.

A ponte para o motor é **0,6 unidade por volt**, igual para todos os
canais. Os padrões de Volume são Rhythm 0,35 e Lead 0,32; isso equilibra
a receita inicial sem mudar o ganho elétrico por canal. Clean Volume zero
recebe mute digital suavizado para remover o vazamento da regularização
numérica de 1 Ω dos cursores de entrada. Os dois caminhos continuam
processando seus estados quando não estão selecionados.

## Tone e Watts duplos

`IronheartPowerSection` possui duas redes de três nós. Cada uma contém:

- C64/C69 de 100 nF entre a placa equivalente da inversora e a rede;
- C32/C34 de 10 nF em série com a resistência do Tone até terra;
- Watts duplo de 220 kΩ linear, com R23/R24 de 10 kΩ no terminal inferior;
- C33/C35 de 100 nF entre cursor Watts e entrada da potência;
- R114/R118 de 220 kΩ terminando no bias, tratado como terra em AC.

Os cursores do Tone estão aterrados; os segmentos sem sinal não alteram
essa redução AC. Os segmentos ativos variam juntos de 0 a 220 kΩ,
abrindo os agudos ao aumentar Tone. Watts aumenta a excursão dos dois
cursores. No mínimo, os resistores de 10 kΩ ainda dão saída reduzida:
o controle não é um mute. A posição não indica watts físicos calibrados.

A inversora é reduzida a fontes opostas com resistências de saída
estimadas de 35/38 kΩ, conservando uma assimetria aproximada associada às
cargas de placa de 82/100 kΩ. **Não se resolve o par V4 não linear.**
C65/R120 também dão o acoplamento de entrada de 22 nF/1 MΩ.

Capacitores usam companheiros trapezoidais. A matriz simétrica 3×3 só é
invertida enquanto um controle se move. `beginSample()` reduz cada rede
a `saída = ganho·fonte + memória`. A fonte definitiva vem da solução da
malha; `finishSample()` atualiza os capacitores com essa fonte.

## Dynamics na realimentação

O retorno usa R128 de 39 kΩ, VR13 de 220 kΩ, C81 de 6,8 nF e R129 de
4,7 kΩ. Omite-se o carregamento da cauda da inversora, mantendo uma
impedância constante. Por isso esta é uma aproximação da malha completa:

```text
beta(s) = 4k7 / [4k7 + 39k + (Rdyn || 1/sC81)]
```

Quanto maior Rdyn, menor a realimentação dos graves. O capacitor recupera
realimentação em frequências mais altas. Seu estado trapezoidal fornece
`feedback = B·y + memória`. Tone/Watts ficam **dentro** da malha:

```text
fonte_inversora = entrada_AC − feedback
drive = ganho_rede·fonte_inversora + memória_rede
y = teto·tanh(12·drive/teto)
```

A equação escalar monotônica recebe até 16 passos de Newton protegidos
por intervalo. A derivada inclui o ganho instantâneo das redes passivas;
não há uma amostra de atraso artificial no retorno. A saturação é
comportamental, com ganho aberto normalizado em 12 e sag de ataque
50 ms/recuperação 300 ms, reduzindo o teto em até 7,5%. O master usa
uma adaptação fixa final de 0,75 para o motor.
Um acoplamento equivalente de 5 Hz depois da saturação impede DC na saída,
sem pretender reproduzir a resposta magnética do transformador.

Não são modelos individuais das quatro 6L6, do núcleo do transformador,
da alimentação ou de sua interação com o falante. Grades ideais no pré
também excluem corrente de grade, Miller e blocking distortion. Todos
os controles funcionais não significam todos os fenômenos físicos.

## Reverb e memória

O FV-1 original usa um programa em memória externa, ausente do esquema.
Mantemos o Schroeder como aproximação explícita. `Reverb::processWetSample()`
expõe a cauda e permite somá-la ao dry sem baixar a nota seca.
`Reverb::process()` conserva sua mistura anterior para os demais módulos.

Reverb On/Level e Power/Standby têm rampas de 20 ms. Desligá-los não
congela o DSP. Redes/curvas rodam em 4×; estados, matrizes e históricos
são membros dos objetos, sem alocação, locks, I/O ou logs em `process()`.
Delays são alocados em `prepare()`. `reset()` limpa redes, filtros, FIRs,
sag e reverb, mantendo os parâmetros. A cadeia continua sendo dona dos
módulos via `unique_ptr`; não há ponteiros ou buffers compartilhados novos.
Na entrada da potência, valores não finitos viram zero e amplitudes além
de ±100 são limitadas antes do FIR, como proteção numérica fora da faixa musical.

## Evidências e estudo

`test_ironheart_output.cpp` verifica volumes por análise nodal do cursor,
buffers por impedâncias complexas e potência por divisores de impedância
e solução linear da malha. O oráculo não usa matrizes, estados ou inversão
da produção. O FIR de oversampling é medido separadamente.

O maior erro relativo observado no master foi cerca de **2,91·10⁻⁷** em
sinal pequeno. Isso valida o modelo reduzido, sem provar equivalência com
o circuito completo. Testes verificam direção dos controles, mudança de
compressão por Watts, saída reduzida no mínimo, limites sob excitação
forte, reset, dry do reverb e igualdade exata entre blocos de 1/128 amostras
com automação. O teste do EQ confere as cargas Lead/Clean/Rhythm.

Resultado final: **58/58 testes passaram**, e o desktop compilou com o
SDK macOS 26.5. O maior erro relativo dos buffers foi 9,47·10⁻¹⁴ no
conjunto de frequências testado, antes dos limites de excursão.

Na auditoria com seno de 220 Hz, 48 kHz, entrada de amplitude 0,08,
sem gabinete/limiter e com os padrões do catálogo:

| Modo | RMS do amp | Variação de saída para entrada 0,01 → 0,3 |
| --- | ---: | ---: |
| Clean | −18,959 dBFS | 28,37 dB |
| Rhythm | −19,539 dBFS | 16,56 dB |
| Lead | −17,889 dBFS | 16,03 dB |

A diferença máxima entre canais nesse estímulo é 1,65 dB. Isso não é
normalização de notas nem uma medição do Laney. O CSV dos demais amps
coincidiu exatamente com a auditoria anterior, incluindo o Brit Silver.
Presets mantêm os identificadores e valores gravados, mas o Ironheart
muda de resposta pela nova carga, taper e posição de Watts na potência.

Para estudar, acompanhe a carga de saída até o solver V3; depois compare
os dois `beginSample()` da potência com a solução do retorno. São os pontos
em que uma multiplicação posterior deixaria de representar o circuito.
Veja o [guia de uso](ironheart-120.md) para build/testes e fronteiras do
loop/DI no domínio digital. Ainda falta comparação auditiva com uma
referência de entrada e cadeia conhecidas.
