# IRT120H: saídas passivas de V1A e carga dos canais

**Correção de conectividade:** a versão inicial interpretava R187 como
terminação de CH1. Ele é um shunt para terra em V2B; os três cursores
selecionados chegam a V1B. A implementação e este documento foram
corrigidos: veja a [etapa V1B/V2](ironheart-v1b-v2.md).

`IronheartChannelNetwork` reconstrói as redes passivas depois de V1A,
calculando tanto seus sinais de saída quanto a corrente que elas retiram
da placa. O bloco está integrado ao `IronheartPreamp` no oversampling de
4×. Não se limita a colocar filtros depois de uma válvula sem carga.

## Conexões conferidas e fronteiras

Fonte: página 2 do [manual de serviço IRT120H de 2011](https://www.musiker-board.de/attachments/laney_irt120h_sm-pdf.969316/),
placa principal 9505 rev.3 e circuito CH1 9504 desenhado na mesma página.
Referências repetidas em placas diferentes são identificadas abaixo.

| Caminho | Rede incluída |
| --- | --- |
| Lead | C11/C1 de 2,2 nF em série, R2 de 150 kΩ, R21 de 150 kΩ para terra e VR1 de 220 kΩ. |
| Entrada de CH1 | C8 de 22 nF, compartilhado pelos ramos Rhythm e Clean. |
| Rhythm, placa CH1 | C1/C2 de 2,2 nF em série, R1 de 150 kΩ, R2 de 150 kΩ para terra e VR10 de 220 kΩ. |
| Clean, placa CH1 | R3 de 220 kΩ, R4 de **4,7 kΩ**, VR1 de 220 kΩ e C4 de 470 pF entre o topo e o cursor do potenciômetro. |
| Saída selecionada, qualquer modo | Cursor Lead, Rhythm ou Clean ligado a R31 de 1 MΩ para terra e C5 de 100 nF em série com a grade de V1B, com R4 de 1 MΩ para terra. Grade sem corrente/Miller. |

Cada par de 2,2 nF equivale a 1,1 nF porque seu ponto central não tem
derivação no desenho. C8 fica **antes** da separação Clean/Rhythm; não
duplicamos esse capacitor nos dois ramos. C4 faz uma ligação de agudos
que contorna a parte superior do Clean Volume, variando sua ação com a
posição do cursor. R57/C28 pertencem ao controle da chave de boost e
não foram acrescentados por engano ao caminho de áudio de CH1.

O seletor do modelo conecta a carga de entrada de V1B ao cursor Lead
quando Lead está ativo; nos outros modos seleciona o cursor de CH1.
A seleção está representada por contatos ideais, com 1 Ω no contato
fechado para regularização numérica. Não há simulação da bobina,
temporização ou ruído mecânico dos relés.

A terminação incorreta de 172 kΩ foi removida. O bloco termina na grade
ideal de V1B; `IronheartGainStages` agora resolve os próximos estágios e
R187 no nó correto. [Seguidores e EQs próprios](ironheart-v3-eq.md) também
foram incorporados posteriormente, com carga de saída ainda aproximada.

## Como a carga retorna à válvula

Cada capacitor usa integração trapezoidal: uma condutância fixa e uma
fonte de corrente que guarda seu histórico. Quando há um resistor em
série com o capacitor, ambos são reduzidos algebraicamente sem perder
a tensão armazenada no capacitor.

A rede tem o nó de placa e nove nós internos. As leis de corrente formam
uma matriz simétrica positiva. Eliminando os nós internos, obtemos:

```text
I_rede = G_rede · Va_AC − H_rede
Va_AC = Va − Va_repouso
```

`G_rede` é a condutância equivalente naquele passo, e `H_rede` contém a
memória dos capacitores. O solver de V1A usa essa relação **na mesma
amostra**, junto com a corrente não linear de placa e C18. A equação
da placa passa a ser:

```text
(383 − Va)/R18 = I_válvula + I_rede
```

Não há atraso de uma amostra usado para fechar essa interação. Depois da
solução não linear, a tensão de placa determina as tensões internas e a
nova memória dos capacitores.

O fluxo de código é:

```text
load = channels.beginSample()
plateAC = v1a.processSample(gridVolts, load)
outputs = channels.finishSample(plateAC)
```

Os capacitores de acoplamento bloqueiam DC. Por isso, com a rede em
repouso, o ponto DC de V1A continua o mesmo calculado na etapa anterior.
A representação usa tensões AC em torno desse repouso: os capacitores
que ligam a placa começam implicitamente carregados na tensão DC correta.
Não se simula um pico de ligar a alimentação.

## Controles e adaptação ao restante do amp

Lead Gain, Rhythm Gain e Clean Volume agora movem os cursores dos
potenciômetros. As curvas log são estimadas como 10% da resistência no
meio do curso, sem alegar um taper medido. As posições recebem rampas de
20 ms. Um limite de 1 Ω nos extremos evita condutância infinita na matriz;
a saída dos cursores Clean/Rhythm no zero é exatamente aterrada. A grade
de Lead ainda pode ter a cauda de descarga de C5 ao fechar seu ganho.

Os ramos passivos permanecem ligados à placa mesmo quando não selecionados.
Um potenciômetro de ganho inativo sem carga no cursor equivale à mesma
resistência total, salvo a pequena regularização nos batentes. Já o
bright do Clean pode alterar um pouco a impedância desse ramo em outro
modo. Isso permite uma pequena interação passiva, sem aplicar Clean Volume
como multiplicador do sinal Rhythm. No sinal do teste integrado, essa
diferença relativa foi aproximadamente 0,0025% após integrar V1B/V2.

Gain move os potenciômetros da rede passiva. Na etapa seguinte, os drives
comportamentais e pontes por caminho foram removidos em favor de V1B/V2.
Os fatores atuais estão documentados nessa etapa.

A antiga primeira ligação linear com filtros estimados de 7,5 kHz/35 Hz
foi removida: agora os acoplamentos dessa parte são resolvidos pela rede.
Os filtros das três saturações seguintes foram removidos ao modelar V1B/V2. Presets
mantêm seus IDs/faixas, mas a resposta de ganho, o brilho e o nível mudam.

Na troca de canal, os capacitores mantêm seu histórico e a conexão elétrica
é alterada no início do bloco. Posições, volume final e shunt de V2B usam
rampas de 20 ms. Isso não resolve a comutação física dos relés;
mudanças na carga e no EQ ainda podem produzir transientes.

## Memória e tempo real

O pré possui a rede e V1A por valor, sem ponteiros emprestados. Matrizes,
capacitores e vetores têm tamanho fixo em `std::array`. Não há biblioteca
nova, alocação, disco ou locks no processamento. A fatoração de Cholesky
é refeita somente quando uma posição suavizada ou o modo muda; em
parâmetros estáticos, somente os vetores de histórico são resolvidos a
cada amostra. Durante automação, o custo aumenta, mas o número de nós
e de operações permanece limitado.

`prepare()` define os componentes digitais e as rampas; `reset()` zera
os desvios AC e mantém os alvos. O silêncio após reset é exato. Antes de
`prepare()`, a rede devolve carga zero e silêncio. A UI continua escrevendo
os parâmetros do pré; somente a thread de áudio toca nos estados DSP.

## Verificação e estudo

`tests/test_ironheart_channel_network.cpp` compara as três saídas e a
admitância de placa com um oráculo independente por divisores e
impedâncias complexas, sem repetir a matriz ou o algoritmo de Cholesky.
As comparações cobrem ganho e fase, 20 Hz a 20 kHz, os três modos,
posições diferentes dos knobs e taxas internas de 88,2/176,4/192/384 kHz.
O maior erro relativo no conjunto principal foi cerca de 1,33·10⁻⁸
contra a frequência bilinear equivalente, abaixo da tolerância de 10⁻⁵.

Também são verificados a ação do bright, carga de um ramo inativo,
rejeição de DC, limites, silêncio/reset e conservação de corrente no
conjunto V1A/rede sob sobrecarga e automação. O teste integrado verifica
igualdade com blocos de 1/128 amostras inclusive ao trocar canais e knobs
nos mesmos instantes.

Registro anterior à integração de V1B/V2: **55 testes passaram e o desktop compilou**.
A auditoria manteve as medições dos demais amplificadores. No Ironheart
com boost desligado e parâmetros padrão, o seno de referência produziu:

| Modo | RMS na entrada de 0,08 | Variação de saída entre entrada 0,01 e 0,3 |
| --- | --- | --- |
| Clean | −17,206 dBFS | 24,973 dB |
| Rhythm | −17,629 dBFS | 14,249 dB |
| Lead | −16,669 dBFS | 9,103 dB |

São medições sintéticas do modelo completo, sem gabinete, feitas pelo
`ibifx_amp_audit`. Validam níveis e resposta à amplitude, não identidade
sonora com um cabeçote físico. Não houve nova comparação auditiva nesta etapa.

Um microbenchmark local com `-O3`, apenas V1A/rede e geração de seno,
processou 10 segundos a 192 kHz internos em cerca de 0,56 segundo com
knobs estáticos e 0,83 segundo com automação contínua, sem falhas de
convergência. Não é uma garantia de prazo do callback da cadeia completa.

```sh
cmake --build build-ironheart -j 6
ctest --test-dir build-ironheart --output-on-failure
cmake --build build-macos26 --target ibifx_desktop -j 6
./build-macos26/ibifx_desktop --amp amp.ironheart-120
```

Para estudar, siga o par `beginSample()/finishSample()`: por que a rede
precisa devolver uma carga antes de conhecer a saída de V1A? Por que
filtrar a tensão de uma válvula sem carga não dá o mesmo resultado?
Como C4 muda o divisor do Clean Volume?

Os estágios seguintes e o shunt de V2B foram implementados na
[etapa V1B/V2](ironheart-v1b-v2.md). [V3 e os EQs](ironheart-v3-eq.md)
também foram implementados; volumes/buffers são a próxima etapa.
