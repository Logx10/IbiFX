# IRT120H: V1B, V2A, V2B e correção de R187

Registro desta etapa. Posteriormente foram incorporados
[V3 e os tone stacks próprios](ironheart-v3-eq.md); as referências abaixo
a seguidores ideais e ao EQ anterior descrevem o estado histórico.

Esta etapa substitui as três saturações genéricas restantes por V1B,
V2A e V2B com corrente de placa de Koren e suas redes de carga/acoplamento.
O pré resolve os quatro estágios de ganho V1A/V1B/V2A/V2B. Seguidores V3,
EQ, potência e reverb ainda têm aproximações.

## Correção de conectividade

Na página 2 do [manual de serviço de 2011](https://www.musiker-board.de/attachments/laney_irt120h_sm-pdf.969316/),
a inspeção ampliada dos cruzamentos e conectores mostrou que:

- O cursor de CH1 selecionado segue por HD14/HD15 e pela seleção principal
  até R31/C5/R4, na entrada de **V1B**.
- R187 de 22 kΩ liga a grade de **V2B** ao contato de aterramento.
  O outro terminal da comutação está no terra de R34, através de HD106/HD1.
- R187 não transporta áudio de CH1 para somá-lo à saída de V2A. A carga
  anterior de 172 kΩ no cursor CH1 resultava dessa leitura incorreta e foi removida.

| Modo | Cursor que alimenta V1B | R187 na grade de V2B |
| --- | --- | --- |
| Lead | Lead Gain | Aberto |
| Rhythm | Rhythm Gain, depois da rede de CH1 | Aberto |
| Clean | Clean Volume, depois do divisor 220 kΩ/4,7 kΩ e do bright | Fechado para terra |

Clean reduz a excitação pelo divisor e pelo shunt em V2B. Os quatro
estágios de ganho permanecem no caminho modelado. Lead tem prioridade
sobre Clean/Rhythm na interface. Não se simulam bobinas, tempos mecânicos,
chaves JFET e todos os circuitos de mute. A operação dos seletores também
é descrita no [manual Laney](https://www.manualslib.com/manual/523621/Laney-Ironheart-Irt60h.html?page=17);
os componentes e conexões são extraídos do desenho elétrico.

Os testes anteriores validavam as equações da rede implementada, mas não
provavam que a conectividade escolhida reproduzia corretamente o esquema.
Essa distinção motivou corrigir também o oráculo dos testes dos canais.

## Circuitos implementados

| Seção | Componentes |
| --- | --- |
| V1B | 383 V; R13 de 100 kΩ; R41 de 1,5 kΩ; bypass C19 de 22 µF. |
| V1B → V2A | C6 de 10 nF, R22 de 220 kΩ, R32 de 150 kΩ. |
| V2A | 410 V; R14 de 100 kΩ; R42 de 1,5 kΩ sem bypass; C2 de 220 pF em paralelo com R14. |
| V2A → V2B | C7/C3 de 6,8 nF em série (3,4 nF equivalentes), R29 de 150 kΩ e R34 de 150 kΩ. R187 acrescenta 22 kΩ em paralelo com R34 no Clean. |
| V2B | 410 V; R15 de 100 kΩ; R43 de 1,5 kΩ sem bypass. |
| Divisores para V3 | Duas redes iguais: 470 kΩ em série, depois 150 kΩ em série com 470 kΩ em paralelo com 220 nF para terra. |

C2 é um capacitor explícito, não uma emulação do efeito Miller. As fontes
são rígidas, sem sag compartilhado por R11. `IronheartTriodeStage::Circuit`
define os componentes de cada instância sem copiar o modelo de corrente.
Seu padrão continua V1A; capacitância zero representa cátodo sem bypass.

| Repouso calculado | Placa | Cátodo | Corrente de placa |
| --- | --- | --- | --- |
| V1B | 257,13 V | 1,888 V | 1,259 mA |
| V2A | 274,67 V | 2,030 V | 1,353 mA |
| V2B | 245,39 V | 1,794 V | 1,196 mA |

As duas redes de grade de V3 equivalem a 545 kΩ para terra em DC. Essa
carga participa do repouso de V2B mesmo sem corrente de grade. O divisor
resulta em cerca de 139,6 V nas grades de V3; o esquema anota 140 V.
São verificações de coerência com parâmetros genéricos de 12AX7, sem
forçar coincidência com todas as anotações.

## Solução por amostra

As grades são ideais, sem corrente ou capacitância Miller. Nessa hipótese,
a corrente não linear não retorna pela grade ao estágio anterior. Podemos
resolver a sequência sem inserir um atraso de amostra para fechar a interação.
Cada acoplamento devolve uma carga Norton à placa. O triodo é resolvido
junto com essa carga; sua tensão determina a grade seguinte e o novo
histórico do capacitor. V2A também recebe a carga de C2.

```text
V1A + carga dos potenciômetros
 → cursor selecionado → R31/C5/R4 → V1B
 → C6/R22/R32 → V2A
 → C7/C3/R29/(R34 || R187 em Clean) → V2B
 → divisores de V3 → conversão fixa de nível → EQ existente
```

A carga resistiva DC dos divisores de V3 já está no repouso. Durante o
sinal, acrescentamos somente seu desvio dinâmico para não contá-la duas
vezes. As duas redes iguais podem ser reduzidas a um equivalente paralelo;
a tensão de grade é recuperada no ponto entre os resistores de 470/150 kΩ.

C19 guarda a polarização dinâmica de V1B. Os cátodos de V2A/V2B são
resistivos; suas memórias estão nos acoplamentos, em C2 e nos divisores.
R187 recebe uma rampa de condutância de 20 ms. A troca preserva os estados,
mas essa rampa não modela a mecânica do relé. Seleção dos cursores e EQ
ainda podem gerar transientes.

## Integração, memória e limites

`IronheartGainStages` possui três triodos e os estados dos capacitores por
valor, sem ponteiros ou alocações. `prepare()` valida a taxa e calcula o
repouso; `reset()` restaura os desvios AC dos capacitores ao repouso.
O processamento roda no oversampling de 4× existente, com número limitado
de iterações, sem disco, logs ou locks no callback. Os diagnósticos devem
ser lidos na mesma thread ou com áudio parado.

As três curvas `tanh` e seus filtros estimados de 7,5 kHz/35 Hz foram
removidos do pré. A saída em volts é convertida por **0,04 unidade digital
por volt**. Clean usa recuperação final fixa de 6; Lead/Rhythm mantêm seus
volumes de canal. Esses fatores adaptam o nível ao EQ/master existentes;
não são componentes físicos nem normalização por nota ou bloco.

Clean Volume no zero recebe mute final suavizado, evitando amplificar a
regularização de 1 Ω do cursor. Isso pode abreviar a cauda transitória de
C5 nessa posição. Os IDs/faixas dos presets permanecem, mas o som muda.

Pendências: corrente de grade, blocking distortion associada à carga da
grade, Miller, limites do TL072, fonte do pré, seguidores V3 não lineares,
EQ próprio, potência e reverb. Os divisores que alimentam V3 já existem;
os seguidores são ideais. A escala de 1 unidade digital = 1 V na grade
de V1A ainda não foi calibrada contra uma interface e um cabeçote físico.

## Testes e estudo

`test_ironheart_gain_stages.cpp` compara o repouso com bisseção independente
e ganho/fase com linearização por impedâncias e derivadas por diferenças
finitas: 20 Hz–20 kHz, shunt aberto/fechado, taxas de 88,2/176,4/192/384 kHz.
O maior erro complexo relativo no conjunto principal foi cerca de 2,96·10⁻⁷
contra a frequência bilinear equivalente.

Também são verificados KCL nas três placas, integração trapezoidal dos
capacitores, cátodos sem bypass, saturação, comutação, reset e limites.
O teste de canais agora compara a grade compartilhada de V1B nos três
modos. Os testes integrados cobrem controles, presets, silêncio e partição
de blocos durante automação.

Nesta etapa, **56 testes passaram e o desktop compilou**. No sinal de
auditoria de 220 Hz, com boost desligado e entrada de amplitude 0,08,
o modelo completo produziu −17,768 dBFS em Clean, −17,002 dBFS em Rhythm
e −16,846 dBFS em Lead: diferença máxima de 0,923 dB. A variação de saída
para entradas de 0,01 a 0,3 foi de 25,432/16,738/16,270 dB, respectivamente.
Essas medições sintéticas não substituem comparação auditiva com o amp físico.

Em um microbenchmark local compilado com `-O3`, V1A/rede/V1B/V2A/V2B,
incluindo a geração do seno, processaram 10 segundos a 192 kHz internos
em cerca de 1,31 segundo com knobs estáticos e 1,55 segundo com automação
contínua, sem falhas de convergência. O teste não inclui a cadeia completa
nem garante o prazo do callback de um dispositivo real.

```sh
cmake --build build-ironheart -j 6
ctest --test-dir build-ironheart --output-on-failure
cmake --build build-macos26 --target ibifx_desktop -j 6
./build-macos26/ibifx_desktop --amp amp.ironheart-120
```

Para estudar, acompanhe `Coupling::load()`, a chamada do triodo e
`Coupling::finish()`. Por que R187 reduz o ganho? Por que a carga DC de
V3 altera o repouso de V2B? Por que corrente de grade exigirá rever o fluxo
unidirecional? O próximo bloco é V3A/V3B com suas cargas e o EQ próprio.
