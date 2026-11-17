# Melhorias dos amps — 16/11/2026

Implementação da primeira rodada de melhorias identificadas na
[revisão dos amplificadores](revisao-amplificadores.md). O objetivo foi
recuperar resposta à intensidade da entrada, melhorar os controles e
equilibrar canais, preservando o Brit Silver aprovado na escuta.

## O que mudou

- **Potência:** Drive, Sag e Presence usam rampas de 20 ms. O valor alvo
  continua no `Parameter`; cada `SmoothedValue` pertence ao módulo e avança
  por amostra. O estado é inicializado em `prepare()` e restaurado no reset.
- **Ganhos por estágio:** `Preamp` ganhou `stage2` e `stage3`, multiplicadores
  do drive nos estágios seguintes. Ambos valem 1 por padrão para manter a
  cascata de presets anteriores. Plexi/800/Modern usam distribuições próprias.
- **Plexi:** menos saturação acumulada no pré e mais recuperação de nível
  antes da potência. O padrão agora responde mais a entradas fracas.
- **800 e Modern:** menor ganho concentrado na entrada e nos estágios
  seguintes. Os ajustes iniciais seguem saturados, com maior variação de
  volume em resposta à entrada. O Gain ainda alcança saturação mais forte.
- **Power Master / Output:** nos sete amps tradicionais, o primeiro controla
  o ganho imediatamente antes da potência; o segundo regula o nível final.
  Isso permite saturar a potência e compensar o volume de escuta.
- **Acoplamento final:** `PowerAmp` aceita `coupling` em Hz. As receitas novas
  usam 5 Hz para bloquear DC, exceto o Silver. É um filtro de primeira ordem,
  não um modelo completo de transformador ou carga de falante.
- **Ironheart:** recuperação fixa de nível Clean passou de 2x para 16x,
  depois da cascata. Clean Volume continua antes da cascata para controlar
  volume/breakup. Watts passou de `0,5 * posição²` para `0,75 * posição`,
  oferecendo uma faixa mais útil e elevando o nível padrão dos três canais.

Essas mudanças não introduzem alocação, I/O nem bloqueios em `process()`.
Filtros, interpoladores e históricos são membros dos próprios módulos.

## Antes e depois

Mesma metodologia da revisão: seno de 220 Hz, 48 kHz, sem gabinete/limiter,
últimos 0,5 s de um segundo de sinal. A amplitude de entrada varia de 0,01
a 0,3, uma diferença de 29,54 dB. Controles nos padrões do catálogo.

| Medida | Antes | Depois |
| --- | ---: | ---: |
| Variação de saída Plexi | 2,46 dB | 14,18 dB |
| Variação de saída 800 | 0,73 dB | 5,91 dB |
| Variação de saída Modern | 0,10 dB | 5,09 dB |
| Variação de saída Silver | 6,81 dB | 6,81 dB |
| Diferença Clean/Rhythm no Ironheart, entrada 0,08 | 18,11 dB | 0,05 dB |
| DC médio na saída do Plexi | 0,005087 | abaixo de 0,000001 |
| DC médio na saída do Tweed | 0,002813 | abaixo de 0,000001 |

O RMS de saída com entrada média ficou em -13,17 dBFS (Plexi), -14,27 (800),
-14,75 (Modern), -14,86 (Silver), -16,33 (Ironheart Clean), -16,28 (Rhythm)
e -16,18 (Lead). São medições deste estímulo; não representam uma regra de
normalização para guitarras e músicas diferentes.

No Ironheart, a melhora de equilíbrio é no ponto de referência. Ao aumentar
a entrada, Clean continua crescendo mais que Rhythm/Lead; esse comportamento
é esperado para um canal com mais headroom. Não há nivelamento automático
da palhetada. O pré isolado pode ultrapassar amplitude 1 com entradas fortes;
a cadeia em ponto flutuante admite esse headroom, e o limite final depende
do Output Limiter.

## Silver e compatibilidade

O Silver mantém sua receita, EQ e clipagem. A exportação WAV com frase e IR
iguais ficou **idêntica byte por byte** ao arquivo anterior. Isso verifica
esse caso de comparação; ao mover Sag/Presence, a transição agora é suavizada.
O DC residual do Silver permanece porque seu acoplamento continua em 0 Hz.

Presets existentes dos amps tradicionais mantêm os valores armazenados.
Quando não contêm `stage2`, `stage3` ou `coupling`, os valores padrão são
1, 1 e 0. Eles não são migrados silenciosamente para as novas receitas.
O antigo Master aparece como **Output**, ligado ao mesmo parâmetro de antes.
Os presets Ironheart são afetados pela calibração interna de Clean e Watts.

## Como experimentar

Reabra o executável recompilado:

```sh
./build-macos26/ibifx_desktop --amp amp.brit-plexi
```

Selecione uma IR, adicione novamente o amp ou use **Reset** no painel para
carregar seus novos ajustes de catálogo. Ao abrir um preset salvo, seus
ajustes próprios continuam valendo. Power Master aumenta a excitação da
potência; Output compensa o volume final. No Ironheart, Watts controla a saída.

Áudios locais, com frase sintética e a mesma IR Factory Brit 4x12:

- Antes: `audio/previews/amps-review-completo/`.
- Depois: `audio/previews/amps-improved-20261116/`.
- `comparativo.wav` em cada pasta segue a ordem dos modelos/modos da revisão.

Os WAVs de escuta têm níveis RMS alinhados; as medições acima foram feitas
antes desse ajuste e sem IR. Falta comparar tocando guitarra real para
avaliar ataque, sustain, articulação de acordes e preferência de timbre.

## Verificação

52 testes passaram, incluindo regressões para suavização, rejeição de DC,
preservação do Mi grave, ganho por estágio, dinâmica, equilíbrio de canais,
Power Master/Output, presets, extremos e divisão em blocos. O filtro de
acoplamento mudou menos de 0,1 dB o nível de um seno de 82 Hz no teste.
O desktop foi recompilado. Para repetir:

```sh
cmake --build build-ironheart -j 6
ctest --test-dir build-ironheart --output-on-failure -j 6
cmake --build build-macos26 --target ibifx_desktop -j 6
./build-ironheart/ibifx_amp_audit
```

O EQ ainda troca coeficientes por bloco; a suavização implementada nesta
rodada cobre os controles da potência. A simulação continua comportamental,
sem resolver circuitos completos de válvulas, transformadores e NFB.

Para estudar: acompanhe `stageGain` no pré e compare o mesmo sinal com
Power Master reduzido e Output compensado. Por que alterar o ganho antes
da curva muda os harmônicos, enquanto alterar o ganho depois muda o volume?
