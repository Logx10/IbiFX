# IRT120H: primeiro exercício, entrada e Pre-Boost

Este exercício deriva a rede linear até a grade de V1A a partir da página 2
do manual de serviço IRT120H (2011), placa 9505, revisão 3. Foi preparado
como exercício antes da alteração do DSP. Com a autorização de implementação,
o bloco foi conectado ao amp: veja o [código, validação e limites](ironheart-entrada-implementacao.md).
Os passos abaixo permanecem como material de estudo e referência matemática.

## 1. Hipóteses e conexões conferidas

Nesta primeira aproximação, o TL072 é ideal, os diodos de proteção não
conduzem, o JFET é uma chave ideal e a grade não consome corrente nem
apresenta capacitância. Saturação, corrente de grade e efeito Miller ficam
para a etapa de V1A. O cálculo abaixo é de pequeno sinal, não uma medição
do amplificador real.

| Rede | Componentes |
| --- | --- |
| Entrada até U1B+ | R26 = 10 kΩ e C13 = 220 nF em série; R30 = 1 MΩ e C15 = 47 pF em paralelo para terra. |
| Realimentação U1B | R38 = 100 kΩ em paralelo com VR2 usado como resistência variável de 0 a 100 kΩ e C23 = 220 pF. |
| Ramo ao terra de U1B− | R59 = 4,7 kΩ em série com C27 = 10 µF. |
| Chave de boost | Q2 desvia a rede de realimentação ao desligar o boost; na chave ideal, U1B passa a seguidor de tensão. |
| Saída para a grade | C14 = 10 nF em série e R37 = 1 MΩ para terra. |

R38 e VR2 **estão em paralelo**, e não em série. No extremo de 100 kΩ do
potenciômetro, a resistência equivalente de realimentação é 50 kΩ. Trabalhe
inicialmente com resistência em ohms; a curva logarítmica do knob será uma
etapa posterior. A marcação log não basta para conhecer sua curva exata.

## 2. Transferências analógicas de referência

Use `s = j·2π·f` para avaliar a resposta em uma frequência `f`.

### Entrada

Com `Rs=R26`, `Rp=R30`, `Ci=C13` e `Cp=C15`:

```text
Hin(s) = (Rp·Ci·s) / [1 + (Ci·(Rs+Rp) + Rp·Cp)·s + Rs·Rp·Ci·Cp·s²]
       = 0,22·s / (1 + 0,222247·s + 0,0000001034·s²)
```

É uma rede com bloqueio de DC e filtragem de RF. Seus polos ideais são
aproximadamente 0,716 Hz e 342 kHz. O polo superior é apenas o resultado
dessa rede RC ideal; não descreve a largura de banda real do TL072.
O divisor resistivo dá cerca de −0,086 dB na região intermediária.

### Pre-Boost

Para boost ligado e resistência do potenciômetro `Rpot > 0`:

```text
Rf = 1 / (1/R38 + 1/Rpot)
Zf(s) = Rf / (1 + s·Rf·C23)
Zg(s) = R59 + 1/(s·C27)
Aboost(s) = 1 + Zf(s)/Zg(s)
```

Para `Rpot = 0` ou boost desligado, use ganho unitário nesta aproximação.
Isso evita a divisão por zero no cálculo de Rf.

Defina `tf = Rf·C23` e `tg = R59·C27`. Então:

```text
Aboost(s) = [1 + (tf + tg + Rf·C27)·s + tf·tg·s²]
            / [1 + (tf + tg)·s + tf·tg·s²]
```

No máximo, o ganho na banda intermediária é `1 + 50000/4700 = 11,6383`,
aproximadamente **21,32 dB**. A rede reduz esse reforço nos extremos; ela
não é um ganho constante de 24 dB nem um passa-alta isolado de 720 Hz.

### Acoplamento para V1A

Sem carga dinâmica da grade:

```text
Hgrid(s) = s·R37·C14 / (1 + s·R37·C14)
         = 0,01·s / (1 + 0,01·s)
```

O corte fica em aproximadamente 15,92 Hz. A resposta completa é o produto
`Hin · Aboost · Hgrid`. Esse produto só vale com as hipóteses de isolamento
adotadas aqui; quando V1A for modelada, sua carga precisa entrar no cálculo.

## 3. Alvos para conferir a implementação

Ganhos totais até a grade ideal, em dB. A coluna 10 kΩ significa resistência
do potenciômetro, não uma posição de knob comprovada.

| Frequência | Boost desligado | Rpot = 10 kΩ | Rpot = 100 kΩ |
| --- | ---: | ---: | ---: |
| 20 Hz | −2,2244 | 7,0173 | 18,9718 |
| 82,41 Hz | −0,2476 | 9,0959 | 21,0629 |
| 220 Hz | −0,1110 | 9,2381 | 21,2050 |
| 1 kHz | −0,0894 | 9,2599 | 21,2080 |
| 5 kHz | −0,0892 | 9,2456 | 20,7426 |
| 10 kHz | −0,0920 | 9,1979 | 19,5454 |
| 20 kHz | −0,1031 | 9,0127 | 16,6357 |

Os números foram calculados a partir das impedâncias complexas. A fórmula
polinomial da entrada foi conferida contra essa conta direta, com diferença
complexa máxima de aproximadamente 3,4e−16 na varredura usada. Isso confere
a álgebra; não é teste do DSP C++ nem validação física.

## 4. Passagem para o digital

A primeira versão pode ter dois biquads e um passa-alta de primeira ordem.
Use coeficientes e estados em `double`. Sua classe pode possuir todos eles
diretamente, sem ponteiros, alocação por amostra ou dependência de UI.

Para `H(s) = (n0+n1·s+n2·s²)/(d0+d1·s+d2·s²)`, substitua
`s = K·(1−z⁻¹)/(1+z⁻¹)`, com `K=2·taxaInterna`:

```text
B0 = n0 + n1·K + n2·K²       A0 = d0 + d1·K + d2·K²
B1 = 2·(n0 − n2·K²)          A1 = 2·(d0 − d2·K²)
B2 = n0 − n1·K + n2·K²       A2 = d0 − d1·K + d2·K²
```

Divida os cinco coeficientes usados no processamento por A0. O denominador
fica `1 + A1·z⁻¹ + A2·z⁻²`. Não esqueça os sinais negativos na recorrência.
Para Hgrid, derive a versão de primeira ordem diretamente: evite criar
um polo e um zero extras em −1 para depois cancelá-los numericamente.

Use a taxa interna de **4× a taxa do motor**. A transformação bilinear
deforma as frequências altas. No cálculo teórico para a rede em boost
máximo, o erro de magnitude entre 20 Hz e 20 kHz chega a 7,24 dB usando
48 kHz diretamente; a 192 kHz, fica em aproximadamente 0,208 dB.
Esse é um motivo concreto para processar o bloco dentro do oversampling
já existente no pré, antes de V1A, sem adicionar outro oversampler em série.

## 5. Sua primeira implementação

Comece com configurações fixas, definidas antes de processar. Deixe
movimentação de knobs e transição de bypass para depois de validar a rede.

```text
prepare(taxaInterna, resistenciaDoPot, boostLigado):
    validar taxa positiva e resistência entre 0 e 100000 ohms
    calcular Hin, Aboost e Hgrid
    converter as transferências para filtros digitais
    normalizar os coeficientes
    limpar os estados

processarAmostra(x):
    x = filtroEntrada(x)
    se boostLigado e resistenciaDoPot > 0:
        x = filtroBoost(x)
    x = filtroGradeIdeal(x)
    devolver x

reset():
    zerar os estados dos filtros sem alterar a configuração
```

O processamento de cada biquad pode usar dois estados `z1` e `z2`:

```text
y = B0·x + z1
z1 = B1·x − A1·y + z2
z2 = B2·x − A2·y
```

Teste silêncio, reset e seno de 1 kHz com amplitude pequena. Compare o ganho
RMS após o transiente com a tabela; para a implementação bilinear a 4×,
uma tolerância de 0,3 dB cobre a deformação calculada na banda de 20 Hz–20 kHz.
Depois confira os extremos, taxas internas de 176,4/192/384 kHz e igualdade
do resultado com blocos de 1 e 128 amostras.

Não limite artificialmente cada saída a ±1: aqui estamos calculando tensões
relativas de uma rede linear. A relação entre unidade digital e volts,
os limites do TL072 e a carga real da válvula ainda precisam ser definidos
na etapa não linear. A integração atual assume o TL072 ideal. Um filtro que amplifica corretamente pode gerar valores
maiores que 1 dentro da cadeia em ponto flutuante.

Quando os testes passarem, a próxima tarefa é comparar a sua implementação
com essas contas e revisar os estados. Só então adicionamos a não linearidade
e o ponto de operação de V1A.

Fonte: [IRT120H Service Information, esquema principal de 2011](https://www.musiker-board.de/attachments/laney_irt120h_sm-pdf.969316/).
