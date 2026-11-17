# IRT120H: implementação da entrada e do Pre-Boost de 2011

O bloco linear estudado no [exercício](ironheart-entrada-exercicio.md) está
implementado em `src/IronheartInputStage.h/.cpp` e conectado ao
`IronheartPreamp`. Substitui o multiplicador de ganho de até 24 dB que
representava o boost. Os IDs e as faixas dos parâmetros continuam iguais;
presets existentes carregam, mas o timbre com boost pode mudar.

## O que acontece com o sinal

```text
entrada → rede R26/C13/R30/C15 → TL072 ideal + feedback
        → acoplamento C14/R37 → ganho do canal e saturações existentes
```

A entrada é um biquad obtido pela transformação bilinear da rede carregada:
não tratamos seus componentes como filtros RC independentes. A saída para
a grade é um passa-alta de primeira ordem. Ambos bloqueiam corrente contínua.

No boost, a relação `1 + Zf/Zg` permite separar o sinal direto de um ramo
adicional. Esse ramo contém um passa-alta R59/C27, o ganho `Rf/R59` e um
passa-baixa Rf/C23. A resistência Rf vem do paralelo R38/VR2. Assim, aumentar
o boost também altera sua resposta em frequência antes de empurrar a
saturação. No máximo, Rf é 50 kΩ, e a rede completa fornece aproximadamente
21,21 dB a 1 kHz. Não há normalização automática nem clipping nesse bloco.

Os filtros de primeira ordem usam integradores trapezoidais com estado
persistente. Alteramos a resistência, preservando o histórico dos capacitores,
em vez de interpolar livremente coeficientes de um biquad variável. O ramo
R59/C27 acompanha o sinal mesmo com boost desligado. Com a realimentação
em curto ideal, o estado de C23 é zerado, evitando memória congelada.

O knob mapeia 0..1 para 0..100 kΩ usando
`100000 * (81^posição - 1) / 80`. É uma curva logarítmica **estimada**, com
10 kΩ no meio do curso; o esquema não fornece o taper medido. Resistência
e chave recebem rampas de 20 ms. A transição da chave é uma aproximação
numérica pela resistência efetiva, sem modelar o transiente elétrico do JFET.

## Integração, memória e tempo real

`IronheartPreamp` possui o objeto diretamente, sem ponteiros nem alocação.
Ele vive tanto quanto o pré. `prepare()` calcula os coeficientes fixos e
prepara as rampas; `reset()` limpa os filtros e assume os alvos atuais.
`configure()` lê os parâmetros do pré e entrega seus alvos ao bloco.

`processSample()` roda dentro do oversampler de 4× já existente, antes das
saturações. Isso reduz a deformação da resposta perto de 20 kHz sem adicionar
um segundo oversampler ou outra latência de FIR. Os estados e cálculos dos
filtros usam `double`; a cadeia de áudio continua usando `float`. A UI acessa
os parâmetros do pré; somente a thread que processa áudio acessa o estado
mutável do novo bloco. Não há acesso a disco, locks ou alocação no processamento.

Resistência fora da faixa é limitada; resistência não finita vira zero.
Taxa inválida é rejeitada em `prepare()`, fora do callback. O pré continua
aplicando sua política existente de usar 44,1 kHz para uma taxa inválida.

## Validação

`tests/test_ironheart_input_stage.cpp` mede senos e compara os resultados
com um oráculo independente que associa as impedâncias do esquema:

- 20 Hz, 82,41 Hz, 220 Hz, 1/5/10/20 kHz;
- resistências de 0, 10 e 100 kΩ;
- taxas internas de 176,4/192/384 kHz;
- tolerância de 0,01 dB para o circuito com frequência bilinear equivalente
  e 0,30 dB para o circuito analógico na frequência original;
- equivalência de boost desligado e resistência zero, limites, rejeição de DC,
  reset, nova preparação e automação rápida durante o sinal;
- religamento após silêncio sem reaparecimento de cauda antiga.

O teste integrado do Ironheart também verifica igualdade entre blocos de
1 e 128 amostras com boost máximo, além dos controles e presets existentes.

```sh
cmake --build build-ironheart --target test_ironheart_input_stage test_ironheart -j 6
ctest --test-dir build-ironheart --output-on-failure -R ironheart
```

Resultado da validação desta etapa: os 53 testes do projeto passaram e o
desktop compilou. No conjunto de frequências testadas, o maior erro contra
o circuito com frequência bilinear equivalente foi 0,00000464 dB; contra
o circuito analógico na frequência original, 0,2481 dB. Esses números
validam a discretização sob as hipóteses adotadas, não a equivalência sonora
com o amplificador físico.

## Limite desta etapa e próximo passo

Trata-se da rede de pequeno sinal com TL072 ideal, diodos sem condução,
JFET ideal em regime estacionário e grade sem corrente ou capacitância
Miller. Ainda não há escala calibrada entre unidade digital e volts,
limitação de alimentação ou slew rate do TL072. Não existe medição contra
um cabeçote físico nem validação auditiva contra vídeos nesta etapa.

Na etapa seguinte, [V1A recebeu polarização dinâmica e corrente de placa](ironheart-v1a.md),
com escala de tensão explícita. Depois foram integradas as
[cargas dos canais](ironheart-saidas-v1a.md) e [V1B/V2A/V2B](ironheart-v1b-v2.md),
incluindo a correção do shunt R187, e os [seguidores/EQs próprios](ironheart-v3-eq.md).
Volumes/buffers de saída, potência e reverb ainda têm aproximações.
Para estudar a implementação atual, acompanhe `filtered`, `high`, `added`
e `boosted`: cada variável corresponde a uma parte da rede derivada.
