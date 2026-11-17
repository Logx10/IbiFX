# IRT120H: seguidores V3 e tone stacks próprios

Registro da etapa V3/EQ. A terminação fixa de 1 MΩ, calibração e pendências
de saída descritas abaixo foram substituídas pelo
[fechamento de volumes, buffers e master](ironheart-saida-master.md).

Esta etapa implementa V3A/V3B como seguidores não lineares de cátodo e
substitui o EQ britânico emprestado por duas redes do esquema de 2011.
O pré processa quatro triodos de ganho e dois seguidores; Clean compartilha
o seguidor/EQ Rhythm. As duas seções continuam processando mesmo quando
suas saídas não estão selecionadas, preservando o histórico dos capacitores.

Fontes: página 2 do [manual de serviço de 2011](https://www.musiker-board.de/attachments/laney_irt120h_sm-pdf.969316/)
e [descrição dos controles no manual Laney](https://www.manualslib.com/manual/523621/Laney-Ironheart-Irt60h.html?page=10).

## V3 com a carga do EQ

V3A e V3B usam placa em 410 V e resistor de cátodo de 100 kΩ, R20/R47.
A polarização de grade vem dos divisores já modelados na saída de V2B.
Com grade em 139,6 V, o modelo genérico de 12AX7 calcula cátodo em cerca
de 141,55 V e corrente de 1,416 mA. O desenho anota aproximadamente 145 V
no cátodo; não ajustamos a curva para forçar esse valor.

`IronheartCathodeFollower` reutiliza a mesma lei de corrente de Koren de
`IronheartTriodeStage::evaluate()`. O ponto desconhecido agora é a tensão
de cátodo, com placa fixa. A rede de tone devolve uma carga Norton:

```text
I_EQ = G_EQ · (Vk − Vk_repouso) − H_EQ
Ip(410−Vk, Vg−Vk) = Vk/100000 + I_EQ
```

O solver resolve essa equação na mesma amostra. O ganho sem carga externa
é aproximadamente +0,9845, próximo de um e sem inversão. O seguidor pode
cortar/comprimir sob excitação e carga fortes; não é um ganho unitário fixo.
Não há capacitor de bypass inventado no cátodo. A memória elétrica desta
seção está nos capacitores do EQ.

São no máximo oito passos de Newton protegidos e 48 de bisseção. A margem
extra evita reter amostras quando o intervalo ainda está alguns nanovolts
acima da tolerância. O teste de regressão reproduz uma forma de onda da
cadeia high gain completa que falhava com o limite anterior de 32 bisseções.
Se uma solução não convergir, o objeto conserva o ponto anterior e informa
o estado no diagnóstico, sem imprimir no callback.

## Conexões do tone stack

Os nomes à esquerda são da seção Lead, à direita da seção Rhythm/CH1.

| Rede | Componentes e conexão |
| --- | --- |
| Entrada | R39/R75 de 47 kΩ entre cátodo e entrada do EQ. |
| Shunt de entrada | R48/R80 de 47 kΩ em série com C24/C42 de 10 nF para terra. |
| Slope | R49/R24 de 47 kΩ. |
| Treble | C16/C34 de 470 pF e potenciômetro de 250 kΩ linear. |
| Bass | C22/C35 de 22 nF e potenciômetro de 250 kΩ log, usado como reostato com cursor unido ao topo. |
| Middle | C25/C37 de 22 nF e potenciômetro de 25 kΩ linear com os **três terminais**: topo no ramo Bass, cursor no capacitor de Mid e base no terra. |
| Treble Shift | C9/C33 de 470 pF adicionais em paralelo com o ramo de treble, através da chave. |
| Deep | C20/C36 de 100 nF adicionais em paralelo com C22/C35, através da chave. |
| Mid Shift | C30/C38 de 10 nF adicionais em paralelo com C25/C37, através da chave. |

Cada capacitor adicional tem resistor de 10 MΩ em paralelo com o contato.
Portanto, desligar o pull não apaga o ramo nem sua memória. O contato
fechado usa 1 Ω de regularização numérica. Aproximadamente, os valores
com pull ligado passam a 940 pF em Treble, 122 nF em Bass e 32 nF em Mid;
o código mantém os capacitores separados para preservar seus estados.

A primeira tradução do Deep colocou o capacitor no nó errado. O teste
de direção do controle falhou; a ampliação confirmou sua ligação ao
mesmo nó de C22. A conexão e o oráculo foram corrigidos antes da conclusão.
Comparar duas implementações da mesma rede, sozinho, não detectaria esse
erro de leitura do esquema.

O taper de Bass continua estimado: 10% de resistência no meio do curso.
Mid e Treble são lineares. Posições recebem rampas de 20 ms; os resistores
nos extremos são limitados a 1 Ω. Os pulls mudam o contato no início do
bloco, conservando os estados; isso não simula bounce mecânico nem garante
ausência de transiente de comutação.

## Rede passiva e integração

`IronheartToneNetwork` resolve oito nós internos, usando companheiros
trapezoidais dos sete capacitores. Cholesky elimina os nós para obter
condutância e fonte de histórico vistas pelo cátodo. Depois de resolver
V3, a tensão de cátodo determina a saída e os novos estados dos capacitores:

```text
load = tone.beginSample()
cathodeAC = follower.processSample(gridAC, load)
output = tone.finishSample(cathodeAC)
```

As duas seções possuem seus próprios objetos e controles. A seleção entre
suas saídas usa uma rampa de 20 ms; Clean usa a saída Rhythm. O antigo
`ToneStack` genérico deixa de participar do Ironheart dedicado.

O circuito é resolvido no oversampling de 4× existente, antes da decimação.
Não há segundo oversampler ou novo buffer de latência. Estados e matrizes
usam `double` e tamanho fixo em `std::array`; a cadeia externa continua em
`float`. Não há alocação, locks, logs ou disco no callback. A fatoração é
recalculada somente durante movimento de controles ou troca de pulls;
com controles estáticos, os coeficientes e a resposta ao nó de entrada
ficam em cache. Somente a thread de áudio acessa os estados mutáveis.

`prepare()` calcula o repouso e prepara redes/rampas. `reset()` restaura
os desvios AC ao repouso e assume os alvos atuais. A rede é aberta em DC
vista do cátodo, pois seus caminhos para terra são acoplados por capacitores.
Os estados representam tensões relativas à polarização, não capacitores
físicos descarregados ao ligar a alimentação. Silêncio após reset é exato.

A inserção passiva no padrão é cerca de −14,89 dB a 220 Hz. A calibração
fixa final passou de 1,8 para 6,3 para adaptar o nível ao motor e ao master
existentes. O fator de conversão de 0,04 unidade por volt e os volumes
permanecem. Isso não normaliza notas, blocos ou posições dos controles.

## Fronteiras e próxima etapa

A saída do EQ está terminada provisoriamente por **1 MΩ fixo**. Ainda não
resolvemos a carga variável dos volumes, o resistor de 680 kΩ para o
somador, o bypass do volume em Clean, os JFETs e os buffers TL072 de saída.
Esses elementos são a próxima etapa; o volume atual continua uma adaptação
posterior ao circuito. Não se afirma que a saída completa do pré esteja pronta.

Continuam pendentes corrente de grade, blocking distortion associada a ela,
Miller, fonte dinâmica, limites do boost/TL072, potência, Dynamics/Tone/Watts
físicos e reverb específico. Com grade ideal, a carga de cada seguidor não
retorna ao divisor anterior; incluir corrente de grade exigirá rever essa
fronteira. O modelo de válvula é genérico e não foi ajustado a medições do
cabeçote. Não houve comparação auditiva com o amplificador real nesta etapa.

## Testes e estudo

`test_ironheart_tone_section.cpp` usa um oráculo de impedâncias complexas
resolvido por Gauss-Jordan com pivotamento, independente de Cholesky e dos
estados digitais. Compara ganho, fase e carga, as oito combinações dos
pulls, extremos, outras posições e taxas internas de 88,2/176,4/192/384 kHz.
Também verifica o ponto de repouso por bisseção independente, o conjunto
V3/EQ, KCL sob corte/saturação e automação, reset, limites e a regressão
de convergência com a cadeia high gain completa.

O teste de direção confirma Bass/Mid/Treble e Deep, além de localizar a
região de atenuação de médios: no padrão, o Mid Shift desloca o mínimo de
cerca de 689 para 601 Hz. São resultados do modelo com a terminação adotada,
não uma medição do aparelho. O teste integrado cobre automação de ganho,
EQ, pulls e canais com partições de 1/128 amostras.

Resultado desta etapa: **57 testes passaram e o desktop compilou**. O maior
erro relativo no conjunto principal de tone/carga foi 2,42·10⁻⁶ contra
a frequência bilinear equivalente. A regressão da cadeia high gain passa
com o novo limite de bisseções e falha com o limite anterior.

Na auditoria de 220 Hz, boost desligado e entrada de amplitude 0,08,
o modelo completo produziu −17,523 dBFS em Clean, −16,784 dBFS em Rhythm
e −16,630 dBFS em Lead: diferença máxima de 0,893 dB. As medições dos
outros amplificadores coincidiram com a etapa anterior.

Em microbenchmark local com `-O3`, os seis triodos e suas redes processaram
10 segundos a 192 kHz internos em cerca de 2,43 segundos com controles
estáticos e 3,22 segundos com automação contínua, sem falhas de convergência.
O tempo inclui geração do seno e ambas as seções de tone; não inclui
oversampler, gabinete/IR, master ou I/O, nem garante prazo do callback real.

```sh
cmake --build build-ironheart -j 6
ctest --test-dir build-ironheart --output-on-failure
cmake --build build-macos26 --target ibifx_desktop -j 6
./build-macos26/ibifx_desktop --amp amp.ironheart-120
```

Para estudar, acompanhe o nó do cátodo e `beginSample()/finishSample()`.
Por que um seguidor de ganho próximo de um ainda precisa ser não linear?
Por que a ligação do capacitor Deep precisa ser conferida além das contas?
Como uma mudança do EQ altera a corrente exigida de V3?
