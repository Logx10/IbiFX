# Revisão dos amplificadores — 16/11/2026

Este é o diagnóstico inicial. A [primeira rodada de melhorias e suas medições](melhorias-amplificadores.md)
documenta o estado posterior à implementação.

Foram revisados os nove modelos do catálogo, incluindo os três modos do
Ironheart completo e do pré isolado. O Brit Silver foi tomado como referência
de escuta aprovada pelo usuário. As receitas sonoras foram preservadas nesta
revisão; as alterações são ferramentas de medição, testes e documentação.

## O que funciona e onde evoluir

| Modelo | Implementação atual | Principal ponto para evolução |
| --- | --- | --- |
| American Clean | Pré de três estágios com bias leve, EQ Blackface, potência pouco saturada. Preserva boa parte da variação de nível de entrada. | Estudar posição do EQ e headroom dos estágios com referência real; hoje o EQ inteiro vem depois da distorção. |
| Tweed '59 | EQ Bassman de terceira ordem, bias maior e sag alto. Há compressão progressiva no teste. | Medir e remover DC residual na saída; estudar recuperação da fonte e saturação da inversora. |
| Brit Plexi | Cascata genérica com drive 1,8, EQ britânico, potência com Presence. | Saturação precoce: quase não reage em volume à entrada fraca/forte nos valores iniciais. Redistribuir ganho entre pré e potência. |
| Brit 800 | Mesmo pré do Plexi com mais ganho, filtros e recuperação de nível diferentes. | Aumentar intervalo útil do Gain e diferenciar estágios; no ponto inicial a dinâmica de amplitude já está muito comprimida. |
| Modern Hi-Gain | Drive 4,5 nos três estágios, entrada 4x e grave filtrado. | Prioridade: quase toda a dinâmica de amplitude desaparece. Separar ganho/filtragem por estágio para obter sustain sem saturar tudo desde a entrada fraca. |
| Brit 200 | Pré menos saturado, potência de baixo drive, Presence; EQ cai no circuito padrão Bassman. | Escolher e documentar um voicing próprio. Não assumir que a receita corresponde ao circuito elétrico do Major. |
| Brit Silver | Pré + AsymmetricClipper + EQ padrão Bassman + potência. Tem uma topologia diferente dos outros Brit e dinâmica intermediária nas medições. | Preservar este som; implementar uma futura variante mais fiel com canais e Rhythm Clip, mantendo referência para A/B. |
| Ironheart 120 | Pré próprio com três modos, EQ compartilhado Clean/Rhythm, EQ Lead, switches e master separado. | Equalizar níveis iniciais entre canais; aprofundar os modelos de potência/NFB e os estágios por canal. |
| Ironheart Preamp | Mesmo pré do modelo completo, sem master/potência/reverb. | A diferença de nível entre Clean e canais saturados também nasce aqui; calibrar antes de compensar no master. |

Os nomes e descrições indicam inspiração. Não há evidência de equivalência
ao hardware para nenhum amp. O EQ Bassman tem uma base física publicada,
mas isso não valida o circuito completo de um amplificador.

## Medições reproduzíveis

Configuração: 48 kHz, blocos de 128 amostras, sem gabinete, sem gate e sem
limiter. Seno de 220 Hz, duração de 1 s, medição nos últimos 0,5 s. Picos de
entrada de 0,01, 0,08 e 0,3; extremos separados por 29,54 dB.
Todos os controles ficam nos padrões do catálogo; no Ironheart muda somente
a seleção do canal. O reverb do modelo completo permanece no padrão.

| Modelo/modo | RMS entrada fraca → saída (dBFS) | RMS entrada média → saída (dBFS) | RMS entrada forte → saída (dBFS) | Variação de saída (dB) | THD H2–H10 na entrada média (%) |
| --- | ---: | ---: | ---: | ---: | ---: |
| American Clean | -43,54 | -25,70 | -16,66 | 26,88 | 1,58 |
| Tweed '59 | -30,31 | -14,65 | -13,42 | 16,89 | 14,85 |
| Brit Plexi | -17,64 | -15,31 | -15,18 | 2,46 | 41,79 |
| Brit 800 | -15,53 | -14,85 | -14,80 | 0,73 | 42,32 |
| Modern Hi-Gain | -15,47 | -15,38 | -15,37 | 0,10 | 30,05 |
| Brit 200 | -30,24 | -12,56 | -5,43 | 24,81 | 2,17 |
| Brit Silver | -21,29 | -14,86 | -14,48 | 6,81 | 32,45 |
| Ironheart 120 Clean | -62,09 | -43,94 | -32,34 | 29,76 | 0,08 |
| Ironheart 120 Rhythm | -28,98 | -25,82 | -25,67 | 3,30 | 27,22 |
| Ironheart 120 Lead | -27,16 | -25,72 | -25,65 | 1,52 | 28,93 |
| Ironheart Preamp Clean | -44,99 | -26,94 | -15,52 | 29,48 | 0,06 |
| Ironheart Preamp Rhythm | -12,17 | -8,94 | -8,76 | 3,41 | 26,82 |
| Ironheart Preamp Lead | -10,32 | -8,82 | -8,72 | 1,60 | 28,77 |

“Entrada fraca/média/forte” identifica o seno aplicado, não uma calibração da
guitarra do usuário. THD é a relação dos harmônicos H2–H10 com a fundamental,
não um critério de qualidade nem uma medição de aliasing. O resultado depende
da nota, do ganho e dos demais controles. Pouca variação de volume também
não implica ausência de toda dinâmica tímbrica.

No sinal médio, o Ironheart Clean ficou **18,11 dB abaixo do Rhythm**. Entre
os amps completos, Brit 200 e Ironheart Clean ficaram **31,38 dB** separados.
Essas diferenças tornam a comparação de timbres nos padrões pouco justa.

## Achados comuns

1. **Master é volume de saída nos sete amps tradicionais.** Está depois do
   `PowerAmp`. Girar esse controle não aumenta sua saturação; faltam um
   controle antes da potência e um trim separado de saída. É uma limitação
   funcional, não um defeito no multiplicador de ganho.
2. **A maioria compartilha a mesma topologia de pré.** Três curvas iguais
   com filtros iguais entre estágios; mudam valores, bias e voicing do EQ.
   Criar mais receitas com esse mesmo núcleo não reproduz automaticamente
   outras topologias. O Silver acrescenta uma curva; o Ironheart usa quatro
   estágios próprios, mas ainda com quantidade fixa entre os modos.
3. **Há componente DC residual depois da potência.** No teste médio, a
   média da saída foi 0,005087 no Plexi e 0,004590 no Silver. O EQ bloqueia DC
   antes da potência, mas a não linearidade posterior pode recriá-la quando
   a forma de onda é assimétrica. Falta modelar o acoplamento final; testar
   o efeito nos graves antes de adotá-lo, especialmente no Silver.
4. **Suavização incompleta.** Drive do `Preamp` e ganhos usam rampas, mas
   Drive/Sag/Presence do `PowerAmp` são lidos diretamente por bloco. O EQ
   muda coeficientes sem transição. Girar esses controles pode causar
   descontinuidades. A ausência de alocação não resolve esse problema.
5. **Headroom e sag são aproximações.** Não há modelo explícito de
   inversora, transformador, impedância do falante ou circuito de feedback.
   O sag atual segue um envelope, não a corrente real de uma fonte.
6. **Oversampling já está presente.** Pré, clipagem assimétrica e potência
   usam 4x. No Silver há três passagens de oversampling; isso filtra e
   acrescenta atraso. A revisão não mediu aliasing nem custo de CPU em tempo
   real, então não afirma que 4x é suficiente para todos os ajustes.
7. **A IR influencia a escuta.** As IRs Factory são filtros sintéticos,
   não capturas. A FFT particionada e a conversão de taxa já existem; a
   recomendação antiga do README sobre convolução somente direta foi corrigida.

## O que os testes verificam

`test_amp_models` percorre as nove receitas em 44,1/48/96 kHz, nos padrões
e com todos os controles nos extremos. Verifica saída finita, áudio no
padrão, reset, bypass e igualdade entre blocos de 1 e 128 amostras.
Os testes específicos do Ironheart também cobrem seleção de canal e EQ.
Isso detecta regressões de software, mas não comprova resposta de válvulas,
fidelidade de cada EQ, ausência de aliasing ou preferência sonora.

## Áudios para comparar

Gerados localmente em `audio/previews/amps-review-completo/`:

- `00-dry.wav`: frase sintética com notas fracas/fortes e acorde.
- Um WAV por modelo/modo, com a mesma IR Factory Brit 4x12.
- `comparativo.wav`: os 13 resultados na ordem da tabela, com 0,5 s entre eles.

Os trechos são ajustados a RMS 0,07, limitado por pico de 0,95. É um ajuste
simples para escuta, não normalização por LUFS. Os números da tabela foram
medidos ANTES desse ajuste e sem a IR. Os áudios não são gravações de guitarra
real nem foram usados como prova de fidelidade.

Para repetir:

```sh
cmake --build build-ironheart --target ibifx_amp_audit test_amp_models -j 6
./build-ironheart/ibifx_amp_audit
./build-ironheart/ibifx_amp_audit audio/previews/nova-comparacao
ctest --test-dir build-ironheart -R 'amp_models|ironheart|preamp|power_amp|tone_stack|factory_presets|gear_library' --output-on-failure
```

A exportação exige uma pasta nova para não sobrescrever comparações anteriores.
Execute na raiz do projeto para localizar a IR Factory.

## Ordem sugerida das próximas mudanças sonoras

1. Suavizar os controles da potência, mantendo a resposta com controles parados.
2. Redistribuir ganho no Plexi/800/Modern e medir a resposta ao volume da guitarra.
3. Balancear os canais do Ironheart com uma DI de referência e ganho de saída explícito.
4. Estudar o acoplamento final/DC e separar master de potência de trim de saída.
5. Criar uma variante do Jubilee com canais/Rhythm Clip, comparada com o Silver atual.

Cada mudança deve ter comparação A/B com a mesma DI, IR e nível de saída.
A sensação de tocar precisa ser avaliada além dos números do seno.

## Fontes primárias

- [Yeh e Smith: Discretization of the '59 Fender Bassman Tone Stack, DAFx-06](https://www.dafx.de/paper-archive/2006/papers/p_001.pdf): base da rede passiva interativa.
- [Marshall Silver Jubilee 2555X](https://www.marshall.com/in/en/product/silver-jubilee-2555x-vintage-reissue-head): o hardware expõe Input Gain/Rhythm Clip, Lead Master, Output Master, EQ e Presence. A receita atual não implementa todos esses modos.
- [Laney IRT120H](https://www.laney.co.uk/discontinued/irt120h): referência de funções do modelo Ironheart, sem confirmação de componentes internos.
