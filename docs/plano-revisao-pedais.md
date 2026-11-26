# Plano de revisão dos pedais do IbiFX

Revisar os 21 pedais do [catálogo inicial](../src/GearLibrary.cpp) em rodadas pequenas, começando pelo Green Screamer. Cada rodada parte de um comportamento que queremos melhorar, registra como o pedal soa hoje e compara uma mudança com essa versão antes de incorporá-la.

Para pedais inspirados em hardware, escolher a versão de referência e os comportamentos que queremos reproduzir. Usar manual para conferir controles, esquema ou estudo de circuito para orientar o DSP e medições de uma unidade física quando disponíveis. Quando a referência for inferida ou o pedal tiver proposta própria, definir primeiro o som e a função esperados no IbiFX. A [lista de assets](assets-equipamentos.md) serve como ponto de partida para identificar referências.

## Método de revisão

1. **Registrar o estado atual.** Anotar o ID do pedal, o commit, a cadeia de módulos, os valores dos controles e a posição típica no rig. Capturar os presets afetados e escolher de uma a três medições que possam mostrar se a mudança funcionou.
2. **Medir o pedal isolado.** Montar a receita real do catálogo com `GearLibrary::buildSignalChain`, pelo mesmo núcleo usado pelo app. Usar senos para frequência, nível e harmônicos, impulsos para atrasos e caudas, e rajadas ou uma DI para dinâmica. Começar em 48 kHz com blocos de 128 amostras. Para mudanças de DSP, verificar também 44,1 e 96 kHz e outro tamanho de bloco. Registrar parâmetros, níveis e janelas de medição. A [revisão dos amplificadores](revisao-amplificadores.md) tem um protocolo que pode servir de base.
3. **Ouvir no rig.** Reprocessar a mesma DI de guitarra, de preferência com single coil e humbucker, com palhetadas leves e fortes, acordes e palm mutes. Usar o mesmo amp e gabinete em cada comparação. Para drives, ouvir em amp limpo e em amp no limite da saturação. Ajustar o nível de escuta depois da cadeia para o A/B, e guardar também o áudio sem ajuste para avaliar volume e headroom. Guardar os WAVs de trabalho em [`audio/`](../audio/README.md).
4. **Mudar uma coisa por comparação.** Começar por ganho, faixa de controle ou EQ quando isso resolver o achado. Criar DSP dedicado quando o comportamento depender da topologia ou de controles que a receita atual não representa. Conferir o efeito em outros pedais sempre que a mudança tocar um módulo compartilhado.
5. **Fechar com evidência.** Registrar a medição antes/depois, os trechos ouvidos, o que melhorou e o que ficou em aberto. Rodar os testes do módulo, do catálogo e dos presets conforme o impacto. No desktop, conferir a resposta dos controles, bypass e salvar/reabrir um preset. Em mudanças com novo custo ou atraso, comparar CPU e latência no mesmo dispositivo e tamanho de bloco.

## Ordem de revisão

Cada linha indica o que investigar e o que deve aparecer na medição ou na escuta. Um pedal pode sair da rodada com uma correção, uma variante ou apenas com o comportamento atual documentado.

| Rodada | Pedal | O que conferir |
| --- | --- | --- |
| 1 | Green Screamer | Simetria da clipagem, resposta dos graves sob Drive, faixa do Tone e uso como boost. Executar o piloto abaixo. |
| 2 | Black Rodent | Varrer Distortion e Filter. Verificar direção do Filter, perda de agudos e aliasing com ganho alto. |
| 2 | Orange Turbo | Medir Turbo baixo e alto com o mesmo Dist e Level. Ouvir se a mudança dos médios e agudos aparece em bases e solos. |
| 2 | Yellow Drive | Definir se a meta é o drive transparente atual ou um modelo de hardware específico. Medir graves, harmônicos e dinâmica com Drive baixo e alto. |
| 2 | Clean Boost | Conferir volume perto da unidade e aumento de nível sem distorção no pedal isolado enquanto houver headroom. Testar quanto empurra o amp. |
| 2 | Round Fuzz | Varrer Fuzz e Bias com DI fraca e forte. Medir nível, DC residual e se há regiões quase mudas no curso útil. |
| 2 | Green Siberian | Medir o vale de médios com Tone em três posições. Ouvir sustain e definição de acordes com Sustain alto. |
| 2 | Bakersfield Fuzz | Conferir como Depth muda a fundamental, os harmônicos e o volume. Se a meta for o Fuzzrite, avaliar a contribuição dos dois estágios antes de mudar a receita. |
| 3 | Noise Gate | Usar notas com cauda e ruído entre frases. Medir tempo de fechamento e checar se Threshold e Release preservam as notas fracas. |
| 3 | Orange Squeeze | Medir ataque, recuperação e nivelamento com palhetadas leves e fortes. Conferir a faixa útil de Sensitivity e Output. |
| 3 | Iron Clamp | Comparar rajadas de palm mute e notas abertas. Checar preservação do ataque com Blend e recuperação entre batidas. |
| 3 | Seven Band EQ | Confirmar resposta plana com bandas e Level em 0 dB e medir o ganho e a frequência central de cada banda nos extremos. |
| 4 | Chrome Wah | Medir frequência e largura do pico ao longo do curso. Testar a varredura por expressão, quando disponível, e ouvir transições rápidas. |
| 4 | Orange Swirl | Medir velocidade e deslocamento dos vales do phaser. Ouvir a mudança de ressonância e nível com Feedback. |
| 4 | Silver Siren | Medir faixa de atraso e varredura. Checar se Color modifica a ressonância e se o feedback fica estável nos ajustes extremos. |
| 4 | Ensemble Chorus | Medir Rate, Depth, Mix e nível de saída. Ouvir o movimento e a definição em acordes limpos. |
| 4 | Blue Chorus | Comparar com o Ensemble usando a mesma DI. Identificar se a diferença de caráter se sustenta fora dos valores padrão, pois ambos usam `Chorus`. |
| 4 | Rotovibe | Comparar Chorus e Vibrato com a mesma DI. Medir velocidade e intensidade ao mover a prancha nos dois modos. |
| 5 | Analog Echo | Medir tempo entre ecos, queda de nível e espectro de cada repetição. A receita usa `Delay` sem filtro na realimentação. Avaliar o escurecimento por repetição e ouvir a mudança de Time durante a cauda. |
| 5 | Chapel Reverb | Medir duração e escurecimento da cauda com impulso. Ouvir se acordes e ataques continuam claros com Mix alto. |
| 5 | Spring Room | Comparar a cauda com a do Chapel. Ambos usam `Reverb`. Avaliar se a ambiência curta cumpre a proposta ou se o caráter de mola pede DSP dedicado. |

## Piloto Green Screamer

1. **Fixar o TS808 como referência inicial.** O circuito de referência usa clipagem suave aproximadamente simétrica, ganho dependente da frequência no estágio de realimentação e um circuito de Tone após a clipagem. O Green Screamer atual usa passa-alta de 400 Hz, `AsymmetricClipper` com `bias` de 0,15 e passa-baixa de 3 kHz antes do volume, nos valores padrão. [Receita atual](../src/GearLibrary.cpp#L131) · [Controles do TS808](https://www.ibanez.com/eu/products/detail/ts808_99.html) · [Análise e modelagem do circuito](https://dafx.de/paper-archive/2007/Papers/p189.pdf)
2. **Capturar a linha de base.** Usar senos de 82, 220 e 1.000 Hz em três níveis de entrada documentados. Com Drive e Tone no mínimo, no meio e no máximo, registrar ganho da fundamental, segundo e terceiro harmônicos e nível de saída. Fazer também uma varredura em nível baixo para ver o perfil de médios antes da saturação. Guardar o preset e os WAVs da DI com Drive baixo e Level alto, além de uma regulagem de overdrive.
3. **Comparar candidatos.** Isolar primeiro a mudança de simetria. Em seguida, avaliar ganho que favoreça os médios e preserve parte do sinal de entrada mesmo quando o estágio satura. Avaliar o Tone em uma comparação própria. Tratar o Low Cut ajustável como opção do IbiFX na decisão sobre o painel.
4. **Decidir pela medição e pela escuta.** A variante deve reduzir a contribuição do segundo harmônico atribuída à assimetria, mostrar menos ganho nos graves que nos médios em nível baixo e responder de forma progressiva ao Drive e ao Tone. Conferir que funciona como boost, com ataque audível e graves controlados antes do amp. Comparar a curva e os harmônicos com a referência disponível. Havendo só esquema ou simulação, registrar a validação nesse nível. Descrever equivalência com hardware somente a partir de comparação com uma unidade medida.
5. **Validar a compatibilidade.** Começar a comparação como variante. Se a nova topologia for adotada no catálogo, verificar o painel e o áudio ao abrir presets anteriores. O preset grava a lista de módulos e seu agrupamento, enquanto o painel consulta o modelo pelo ID. Uma troca de módulos sob o mesmo ID exige mapeamento compatível, migração explícita ou ID distinto. Conferir também as rotas de inserção novas no desktop e via CLI. [Presets](../src/Preset.h) · [Painel desktop](../src/gui/DesktopUI.cpp)

### Primeira comparação de clipagem

O **Green Screamer Simétrico** está disponível com o ID `stomp.green-screamer-symmetric`. Usa a mesma cadeia e os mesmos controles da referência interna (`stomp.green-screamer`, medida na base `cf632ad`). O ajuste experimental é `bias = 0` no clipper, com o mesmo filtro de DC e a mesma sobreamostragem. O TS808 continua como referência para a resposta de ganho e Tone nas próximas comparações.

Com o pedal isolado, seno de 1 kHz e pico de entrada de 0,25, o segundo harmônico caiu de **−15,9 dBc** para **abaixo de −80 dBc**. O ganho da fundamental subiu de **+5,0 para +10,6 dB** com Drive 4, Tone em 3 kHz e Level 1,2. A mudança de nível vem junto com a troca de bias e deve ser levada em conta na escuta e ao empurrar o amp.

As medições completas ficam em `audio/previews/green-screamer-simetria/medicoes.csv`, junto dos dois presets e da rotina de medição. A configuração foi de 48 kHz, blocos de 128 amostras e senos de 1 s, com análise nos últimos 0,5 s. A matriz cobre 82, 220 e 1.000 Hz, picos de 0,01, 0,08 e 0,25, e quatro posições de Drive e Tone, incluindo os padrões, os extremos e o centro. O perfil em sinal fraco foi medido de 40 Hz a 8 kHz.

Os WAVs `*-AB.wav` tocam o original e depois o simétrico com RMS igualado após o amp e o gabinete. A fonte é uma frase sintética com o American Clean e o Tweed '59, ambos com o mesmo gabinete Factory Brit 4x12. Os WAVs `*-nivel-original.wav` guardam a saída de cada rig antes desse ajuste de escuta.

O teste do catálogo mede H2 após salvar e reabrir os presets, em 44,1, 48 e 96 kHz com blocos de 64 e 128 amostras para a variante. A primeira comparação deixa aberta a escuta com DI real e a revisão do ganho dependente da frequência e do Tone.

### Auditoria de Drive e Tone

A ferramenta [`ibifx_screamer_audit`](../tools/screamer_audit.cpp) processa 252 combinações com os dois pedais isolados. São sete frequências entre 82 Hz e 6 kHz, três picos de entrada (0,01, 0,08 e 0,25) e duas séries de ajustes. Uma varia o Tone com Drive 4. A outra varia o Drive com Tone em 4.250 Hz. A medição usa 48 kHz, blocos de 128 amostras e a metade final de cada seno de 1 s. Low Cut fica em 400 Hz e Level em 1,2.

| Medição | Ajuste | Original | Simétrico |
| --- | --- | ---: | ---: |
| Brilho, 4 kHz relativo a 1 kHz, entrada fraca | Tone mínimo / meio / máximo | −10,5 / −2,8 / −1,1 dB | −10,5 / −2,8 / −1,1 dB |
| Ganho em 1 kHz, entrada fraca | Drive mínimo / padrão / máximo | −6,3 / +5,7 / −3,5 dB | −5,6 / +12,4 / +22,0 dB |
| Grave, 82 Hz relativo a 1 kHz, entrada forte | Drive mínimo / padrão / máximo | −12,8 / −12,4 / −20,9 dB | −12,8 / −11,4 / −6,4 dB |

O Tone concentra a mudança de brilho na primeira metade do curso. A variante simétrica resolve a queda de ganho em sinais fracos com Drive alto vista no original. Com sinal forte, ela também deixa o grave mais próximo dos médios à medida que os médios saturam. A próxima comparação deve avaliar o estágio de ganho com realimentação dependente da frequência, com atenção ao corpo do grave em Drive baixo e à sua resposta sob saturação. O Tone merece uma comparação separada com a resposta do circuito de segunda ordem descrito na referência. [Modelo de circuito e curvas de referência](https://dafx.de/paper-archive/2007/Papers/p189.pdf)

Para repetir a partir da raiz do projeto, usar uma pasta de destino nova:

```sh
cmake --build build-macos26 --target ibifx_screamer_audit
./build-macos26/ibifx_screamer_audit audio/previews/green-screamer-drive-tone-novo
```

Um WAV mono de guitarra direta pode entrar como segundo argumento. Sem esse arquivo, a ferramenta gera a mesma frase sintética a cada execução. Os comparativos usam American Clean e Factory Brit 4x12, com três trechos em ordem de controle crescente e nível RMS igualado após o gabinete. Os arquivos incluem os presets, os WAVs com nível relativo preservado, `tons.csv` e `niveis-audio.csv`. Os resultados desta rodada estão em `audio/previews/green-screamer-drive-tone/`.

### Estágio de ganho do Green Screamer 808

O `stomp.green-screamer-808` soma o sinal direto ao ramo de realimentação com passa-alta em cerca de 720 Hz e clipagem suave simétrica. O Drive varia o ganho desse ramo a partir dos valores do resistor e do potenciômetro do circuito. A curva dos diodos usa uma aproximação suave e uma calibração inicial de 1 unidade digital por volt. O Tone desta variante continua usando um passa-baixa simples. [Análise do estágio e valores do circuito](https://dafx.de/paper-archive/2007/Papers/p189.pdf)

Na frase em lá blues feita com as amostras DI do FreePats, os padrões do 808 e do Simétrico deram −9,32 e −18,41 dBFS RMS na saída dos pedais, com o mesmo American Clean e o mesmo gabinete. No A/B em `audio/previews/green-screamer-808/comparativo-nivel-igualado.wav`, o 808 usa Level 0,4216 para aproximar o RMS na entrada do amp. O volume de escuta é igualado também depois do gabinete. Essa regulagem serve para esta DI. Ela depende do nível e da dinâmica da entrada.

### Tone ativo do Green Screamer 808

O **Green Screamer 808** (`stomp.green-screamer-808-circuit`) usa o estágio de clipagem acima seguido de `ScreamerTone`. A rede inclui o filtro RC de entrada, o potenciômetro de 20 kΩ e o ramo do cursor com 220 Ω e 220 nF. O movimento do cursor muda tanto a carga sobre o filtro de entrada quanto o ganho de realimentação. A implementação transforma essa resposta analógica de segunda ordem em um filtro digital e suaviza a posição do cursor em 20 ms. Usa um amplificador operacional ideal e posição elétrica linear do cursor. [Esquema e análise da rede](https://dafx.de/paper-archive/2007/Papers/p189.pdf)

O **Green Screamer 808 (Tone simples)** usa o ID anterior `stomp.green-screamer-808`. Os presets dessa versão continuam usando `LowPass` em Hz. O novo Tone vai de 0 a 1 e começa em 0,5.

A 48 kHz, o Tone isolado atenuou 4 kHz em 18,9 dB no mínimo, 14,5 dB no meio e 2,4 dB no máximo. Os testes cobrem 220 Hz, 1 kHz e 4 kHz nas três posições e verificam desvio de até 0,35 dB em relação à curva analógica calculada, em 44,1, 48 e 96 kHz. A comparação foi feita com os valores nominais do circuito.

Em `audio/previews/green-screamer-808-tone/`, `tone-ativo-tres-posicoes.wav` toca mínimo, meio e máximo. `tone-simples-vs-ativo.wav` começa com o passa-baixa de 3 kHz e segue com o Tone ativo no meio. Ambos usam a mesma DI em lá blues e o mesmo Drive, Level, amp e gabinete, com RMS igualado depois do gabinete. `curvas-tone.csv` contém a medição dos dois filtros isolados.

### Inspeção por estágio

O CLI pode processar um WAV e registrar entrada, módulos e saída no mesmo render. A partir da raiz do projeto, usar uma pasta nova:

```sh
./build-macos26/ibifx --inspect-audio audio/previews/green-screamer-di-real/la-blues-di.wav \
  audio/previews/inspecao-808 --stomp green-screamer-808 --amp american-clean --cab factory.brit-4x12
```

O `estagios.csv` registra pico, RMS, média do sinal, amostras acima de 0 dBFS e valores não finitos. O `saida.wav` registra a saída final. Em uma cadeia com caminhos paralelos, a linha do marcador mostra o buffer após a operação de roteamento. Os níveis internos são medidos nos buffers em ponto flutuante. Um pico acima de 1 dentro da cadeia indica onde o sinal excedeu a escala nominal e pode continuar representável até a saída. O comando roda offline a 128 amostras por bloco. No caso testado, o WAV foi idêntico ao render normal do CLI.

## Correção de cortes e picos nos fuzzes

A inspeção da DI em lá blues reproduziu dois problemas distintos com os pedais isolados. No Round Fuzz e no Bakersfield, a combinação de Drive e Bias estacionava a curva de clipagem perto do teto e fazia notas fracas desaparecerem. No Bakersfield e no Green Siberian, o Volume podia amplificar demais a saída depois da clipagem e do EQ.

| Pedal | Condição | Antes | Depois |
| --- | --- | --- | --- |
| Round Fuzz | DI fraca, Fuzz no mínimo e Bias no máximo | Silêncio | −44,4 dBFS RMS |
| Bakersfield Fuzz | DI fraca, Depth no máximo | −156,4 dBFS RMS | −35,5 dBFS RMS |
| Bakersfield Fuzz | DI normal, Volume no máximo | 6.477 amostras acima de 0 dBFS | 0 |
| Green Siberian | DI normal, Sustain, Tone e Volume no máximo | 251.971 amostras acima de 0 dBFS | 0 |

O Round Fuzz usa Drive interno 8 e Bias entre 0 e 0,15. O Bakersfield usa Bias fixo 0,04 e Volume até 0,65. O Green Siberian usa Volume até 0,50. A calibração mantém os mesmos estágios de DSP de cada pedal. Os 26 casos medidos e os WAVs estão em `audio/previews/fuzz-correcao/`. Os WAVs usam o mesmo ajuste de escuta antes e depois de cada correção para evidenciar a diferença de nível e o corte de notas.

Presets anteriores guardam os parâmetros da época. O botão **Reset** no painel do pedal aplica a calibração nova aos módulos do preset aberto. Para um pedal avulso sem painel agrupado, reinsira o fuzz pelo catálogo. Salve uma nova versão do preset para manter o ajuste aplicado.

## Critério para concluir cada rodada

- A medição mostra o efeito esperado dentro da faixa combinada para a revisão, e o A/B com nível ajustado confirma que a mudança ajuda no uso pretendido.
- A saída permanece finita, os controles cobrem uma faixa útil e a operação normal de controles e bypass não introduz estalos ou saltos de volume indesejados.
- Os presets anteriores abrem com áudio e controles consistentes. Quando a assinatura sonora precisar mudar de forma incompatível, a variante ou a migração tem um caminho explícito.
- O registro deixa reproduzíveis a referência, a configuração, a versão anterior, a versão candidata e a decisão.

A sincronização por BPM fica como evolução do Analog Echo após revisar tempo, cauda e transições. O IbiFX já tem BPM no [Master Transport](../src/MasterTransport.h). A revisão desse recurso pode começar pelo relógio interno, com o tempo em segundos ainda disponível e um teste de subdivisão, como 500 ms em 1/4 e 250 ms em 1/8 a 120 BPM.
