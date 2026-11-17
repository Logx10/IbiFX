# IRT120H: V1A com polarização dinâmica

Este documento registra a primeira versão de V1A, validada sem carga externa.
Na etapa seguinte foram integradas suas [saídas passivas e a carga dos canais](ironheart-saidas-v1a.md).
As hipóteses de saída sem carga e pontes descritas abaixo são históricas;
o teste isolado sem carga continua existindo como referência.
Depois também foram incorporados [V1B/V2A/V2B e o shunt do Clean](ironheart-v1b-v2.md).

Esta etapa substitui a primeira saturação genérica do `IronheartPreamp`
por um modelo de V1A em `src/IronheartTriodeStage.h/.cpp`. Naquele momento,
os demais três estágios, EQ e potência ainda eram comportamentais.

## Circuito e hipóteses

A página 2 do esquema principal 9505 rev.3 de 2011 identifica V1A como
ECC83. O modelo usa R18 = 100 kΩ na placa, R40 = 1,5 kΩ no cátodo e
C18 = 1 µF em paralelo com R40. A alimentação é fixa em 383 V, conforme
a anotação do modelo de 120 W depois de R11. Não usamos os 410 V antes
desse resistor como alimentação de V1A.

Esta versão resolve a válvula com **saída sem carga externa**. R18 é a
carga de placa presente; as redes de acoplamento, potenciômetros e cargas
dos canais seguintes ainda não participam da solução elétrica. Os valores
de C11/C1 e os outros caminhos não foram substituídos por um único RC
estimado. Isso delimita o próximo trabalho, que deve reconstruir suas
conexões e a carga refletida na placa.

A corrente de placa usa a equação fenomenológica de Koren com parâmetros
genéricos de 12AX7: µ = 100, Ex = 1,4, Kg1 = 1060, Kp = 600 e Kvb = 300.
Esses parâmetros não foram ajustados a uma ECC83 física do amplificador.
O cálculo de `log(1 + exp(x))` usa uma forma estável para evitar overflow.
As derivadas em relação às tensões de placa e grade são calculadas junto
com a corrente para o método de Newton.

Fontes: [equação de Norman Koren](https://www.normankoren.com/Audio/Tubemodspice_article.html),
[parâmetros e discussão de polarização dinâmica em Macak/Schimmel, DAFx-2010](https://www.dafx.de/paper-archive/2010/DAFx10/MacakSchimmel_DAFx10_P12.pdf),
[manual de serviço do IRT120H de 2011](https://www.musiker-board.de/attachments/laney_irt120h_sm-pdf.969316/).

Ainda não há corrente de grade, capacitâncias internas/Miller, limitação
do TL072 anterior ou variação da fonte. Em sobrecarga positiva, a ausência
de corrente de grade limita a precisão. Testar estabilidade nessa região
não comprova fidelidade sonora. A corrente é zerada quando a tensão
placa-cátodo deixa de ser positiva.

## Como a polarização é resolvida

No repouso, o capacitor fica aberto e as relações são:

```text
Va = 383 − 100000·I
Vk = 1500·I
I = Ip(Va−Vk, −Vk)
```

O resultado calculado é aproximadamente **Va = 257,13 V, Vk = 1,888 V,
I = 1,259 mA**. O desenho anota 250 V e 1,8 V. A proximidade é uma
verificação de coerência; não forçamos a curva para coincidir com essas
duas anotações, nem tratamos a diferença como erro numérico.

Durante o sinal, C18 armazena tensão. A integração trapezoidal transforma
o capacitor em uma condutância `Gc = 2·fs·C18` e uma corrente de histórico.
A cada amostra, o sistema se reduz a uma única corrente desconhecida:

```text
Va = 383 − R18·I
Vk = (I + histórico) / (1/R40 + Gc)
F(I) = I − Ip(Va−Vk, Vg−Vk) = 0
```

Após resolver a corrente, a tensão de cátodo atualiza o histórico. Essa
memória altera o ganho durante um transiente e permite recuperar a
polarização depois da nota. Em pequeno sinal, o ganho é cerca de −32,44
a 20 Hz e −62,23 a 5 kHz: C18 reduz a realimentação local nos agudos.
O sinal negativo indica a inversão de fase do cátodo comum.

O solver mantém a raiz dentro de `0..383/R18`. São no máximo oito passos
de Newton protegidos e 32 passos de bisseção. Se não convergir, o bloco
conserva a última solução e o histórico, indica a falha no ponto retornado
e tenta novamente na próxima amostra; não imprime no callback.

## Integração e escala de áudio

```text
IronheartInputStage → V1A em volts → ponte fixa de nível
                   → Clean Volume → três saturações aproximadas → EQ
```

A convenção escolhida é **1 unidade digital = 1 V na grade de V1A**,
depois do bloco de entrada. É uma escala interna explícita, ainda sem
calibração contra a entrada física da interface de áudio. O Pre-Boost
atua antes da válvula e pode levá-la à sobrecarga.

`processSample()` retorna a diferença entre a tensão atual de placa e a
tensão de repouso. A ponte divide esse resultado pelo ganho bypassado
fixo, aproximadamente 62,27, e compensa a inversão para preservar a
convenção de polaridade da cadeia existente. Não mede nem normaliza cada
nota ou bloco. Clean Volume passou a atuar depois de V1A; por isso, mesmo
com Clean Volume baixo, a válvula recebe a excitação da entrada/boost.

A primeira posição do laço antigo agora é uma ponte **linear** com o ganho
de adaptação existente. Seus filtros de 7,5 kHz/35 Hz foram mantidos como
parte da ligação comportamental aos próximos blocos; não são atribuídos
aos componentes de V1A. As outras três posições mantêm suas curvas `tanh`.
O drive dependente do canal atua nessa ponte e nos estágios restantes,
sem alterar os componentes ou a corrente de repouso de V1A.

Não há novo controle nem alteração dos IDs dos presets. O timbre e o nível
de presets do Ironheart mudam pela resposta do novo estágio. Os outros
modelos de amplificador não usam esta classe.

## Estado, limites e tempo real

O pré possui `IronheartTriodeStage` diretamente, sem ponteiros. O objeto
mantém o ponto de repouso, o ponto atual e uma corrente de histórico de
capacitor. `prepare()` valida a taxa interna, resolve o repouso e calcula
o ganho de adaptação. `reset()` restaura C18 carregado no repouso, evitando
um transiente artificial de partida. O silêncio após reset é exatamente zero.

O bloco roda no oversampling de 4× já existente. `processSample()` usa
apenas estado local, cálculo limitado e funções matemáticas: sem alocação,
locks, disco ou rede. `OperatingPoint` permite inspeção nos testes; sua
leitura deve ocorrer na mesma thread ou com o áudio parado. Não é um
mecanismo de comunicação direta com a UI.

Entradas não finitas são tratadas como zero; valores fora de ±100 V são
limitados como guarda numérica, não como modelo de condução de grade.
A saída AC pode ultrapassar ±1 porque está em volts. A ponte do pré faz
a conversão fixa de nível antes de continuar na cadeia em ponto flutuante.

## Testes e estudo

`tests/test_ironheart_triode_stage.cpp` verifica:

- repouso por bisseção independente e coerência com as anotações do esquema;
- ganho **e fase** contra a linearização do circuito, com derivadas de
  corrente por diferenças finitas independentes do solver de produção;
- conservação de corrente na placa e integração de C18 em transientes;
- convergência e limites sob sobrecarga nas taxas internas de 88,2/176,4/192/384 kHz;
- assimetria, compressão forte, mudança de polarização e recuperação;
- silêncio inicial, reset, nova preparação, taxas inválidas e entradas extremas.

Os testes integrados do amp verificam canais, presets, volumes, estabilidade
e igualdade entre blocos de 1 e 128 amostras, com e sem boost.

Nesta etapa, **os 54 testes passaram e o desktop compilou**. A auditoria
dos modelos manteve as medições dos outros amplificadores; no Ironheart,
a diferença entre canais no nível de referência ficou em aproximadamente
1,9 dB com boost desligado. O maior erro relativo complexo no teste de
pequeno sinal de V1A foi 2,26·10⁻⁹ contra a linearização bilinear equivalente.
Isso valida a solução das equações adotadas, não uma equivalência medida
com o amplificador físico.

Em um microbenchmark local compilado com `-O3`, somente V1A processou
10 segundos a 192 kHz internos em 0,36–0,47 segundo, com amplitudes de
0,08/2/12 V e nenhuma falha de convergência. O tempo inclui a geração do
seno; não mede a cadeia inteira nem garante o prazo do callback do dispositivo.

```sh
cmake -S . -B build-ironheart -DIBIFX_BUILD_DESKTOP=OFF -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
cmake --build build-ironheart -j 6
ctest --test-dir build-ironheart --output-on-failure
cmake --build build-macos26 --target ibifx_desktop -j 6
./build-macos26/ibifx_desktop --amp amp.ironheart-120
```

Para estudar o código, acompanhe `evaluate()`, `solve()` e a atualização
de `m_history`. Por que C18 precisa começar carregado? Por que o ganho
depende da frequência? Por que o intervalo da corrente limita a tensão
de placa? O próximo passo é reconstruir as saídas de V1A, suas cargas e
os caminhos Clean/Rhythm/Lead antes de substituir outro triodo.
