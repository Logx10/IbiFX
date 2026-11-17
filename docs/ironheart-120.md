# Ironheart 120 — pré e master inspirados no Laney IRT120H

A entrada e o Pre-Boost já usam a rede linear derivada do esquema de 2011.
Veja a [implementação e suas hipóteses](ironheart-entrada-implementacao.md),
o [primeiro triodo V1A com polarização dinâmica](ironheart-v1a.md)
suas [saídas passivas com carga dos canais](ironheart-saidas-v1a.md),
os [estágios V1B/V2 e a correção de R187](ironheart-v1b-v2.md),
os [seguidores V3 e EQs próprios](ironheart-v3-eq.md),
além do [plano de evolução dos demais blocos](ironheart-circuito-2011.md).

No catálogo de amplificadores, selecione **Ironheart 120** para usar o pré e
o master juntos. Acrescente um gabinete/IR depois dele: o modelo não inclui
falante. **Ironheart Preamp** oferece somente a saída do pré.

## Controles

Os 28 controles do painel são salvos nos presets. Chaves aparecem como
botões com posições nomeadas; knobs usam escala de 0 a 10.

| Seção | Controles e comportamento |
| --- | --- |
| Seleção | Channel alterna Lead e Clean/Rhythm; Clean/Rhythm seleciona o modo da segunda seção. Lead tem prioridade. |
| Entrada | Boost On e Pre-Boost aumentam o sinal antes da saturação, inclusive em Clean. |
| Clean | Clean Volume; usa o EQ Rhythm e ignora Rhythm Gain/Volume. |
| Rhythm | Gain, Bass, Middle, Treble, Deep, Mid Shift, Treble Shift e Volume. |
| Lead | Gain, Bass, Middle, Treble, Deep, Mid Shift, Treble Shift e Volume independentes. |
| Master | Dynamics, Tone, Reverb On, Reverb, Watts, Power e Standby/Play. |

Deep estende a resposta grave; Mid Shift desloca os médios para baixo;
Treble Shift amplia a faixa do controle de agudos. As duas seções conservam
os ajustes ao trocar de canal. Os controles sem efeito no canal atual
continuam visíveis para permitir preparar o próximo timbre.
Os ramos passivos de entrada continuam carregando V1A: o bright do Clean
pode ter uma interação pequena com os outros modos, mesmo sem selecionar
a saída Clean. Clean Volume inclui agora o capacitor de 470 pF do esquema.

Dynamics varia o grave que entra na potência. Tone escurece/abre a saída
depois da saturação. Watts atenua a saída mantendo o drive da potência;
é uma escala normalizada de volume, sem correspondência calibrada em watts
físicos. Power Off e Standby silenciam com uma rampa curta, sem tempo de
aquecimento simulado. Reverb On corresponde à chave de habilitação remota.

Na rodada de calibração, Clean recebeu maior recuperação de nível depois da
saturação e Watts passou a usar curva linear de volume. Após integrar a rede
de entrada, triodos de ganho, seguidores e EQs, os três canais ficam a menos de 1 dB entre si nos
padrões para o seno de referência, com boost desligado. Veja
[as medições e efeitos sobre presets](melhorias-amplificadores.md).

## Loop de efeitos e uso só do pré

Na cadeia livre, monte esta ordem:

```text
Ironheart Preamp → delay/chorus/outros efeitos → Ironheart Master → gabinete/IR → Output Limiter
```

**Ironheart Master** fica na categoria Rack. Essa divisão permite inserir
efeitos na posição do FX Send/Return ou combinar o pré com outra potência.
O modelo integrado **Ironheart 120** já contém as duas seções consecutivas.

O controle por MIDI usa os parâmetros comuns do IbiFX; Channel, Clean,
Boost On e Reverb On podem ser mapeados pelo mecanismo de MIDI existente.
Não há conexão com o footswitch físico FS4 nem emulação de jacks elétricos.
DI balanceada, ground lift, impedâncias de falante e seleção de válvulas
da potência não pertencem à implementação do pré. A IR e os dispositivos
de áudio continuam sendo escolhidos nos controles normais do aplicativo.

## Como o DSP funciona

`IronheartInputStage` processa entrada, realimentação do Pre-Boost e
acoplamento para a grade antes das saturações, dentro do mesmo oversampling
de 4 vezes. O boost inclui filtragem dependente da resistência e fornece
cerca de 21,21 dB a 1 kHz no máximo. O knob usa uma curva log estimada;
o TL072 e a chave são ideais, sem corrente de grade ou efeito Miller.
Essa rede substitui o ganho plano estimado da primeira versão.

`IronheartTriodeStage` substitui a primeira saturação genérica por V1A com
R18/R40/C18, alimentação de 383 V e corrente de placa de Koren. O capacitor
de cátodo tem estado dinâmico. `IronheartChannelNetwork` resolve os
acoplamentos e potenciômetros de Lead/Rhythm/Clean e devolve sua carga
ao solver de V1A na mesma amostra. Os três cursores selecionados chegam
à entrada de V1B por R31/C5/R4.
`IronheartGainStages` resolve V1B/V2A/V2B com seus acoplamentos e cargas.
R187 é um shunt para terra em V2B, fechado em Clean; a terminação anterior
de 172 kΩ em CH1 foi removida por uma correção de leitura do esquema.
As três curvas genéricas de saturação e seus filtros estimados foram
removidos. Os divisores de grade de V3 carregam V2B em DC e AC.
`IronheartCathodeFollower` resolve V3A/V3B com placa em 410 V e resistor
de cátodo de 100 kΩ, junto à carga do EQ. Corrente de grade e capacitâncias
internas continuam pendentes. Gain atua nos potenciômetros da rede passiva.
Oversampling de 4 vezes reduz aliasing.

`IronheartToneNetwork` substitui o `ToneStack` genérico nesta receita.
São duas redes independentes, com os resistores de 47 kΩ, capacitores e
potenciômetros do esquema, incluindo os três terminais do Mid. Deep liga
100 nF adicionais ao ramo Bass, Mid Shift acrescenta 10 nF ao ramo Mid e
Treble Shift acrescenta 470 pF ao Treble, mantendo resistores de 10 MΩ
nos contatos abertos. A carga de saída ainda é aproximada por 1 MΩ fixo;
volumes/buffers de saída permanecem pendentes. As redes processam em 4×
e mantêm sua memória ao trocar de canal; posições dos knobs são suavizadas.

`IronheartMaster` possui o `Reverb` e o `PowerAmp` existentes, além dos
filtros de Dynamics/Tone e da atenuação de saída. O reverb é o Schroeder
do projeto, sem alegação de reproduzir o algoritmo proprietário da Laney.

Cada objeto possui seus filtros e históricos. A cadeia mantém a posse dos
módulos com `unique_ptr`. `prepare()` aloca o reverb; `process()` não cria
buffers, não acessa arquivos e não bloqueia. Posições dos potenciômetros,
shunt de V2B, Dynamics, Tone
e nível de saída usam rampas de 20 ms. EQ e pull switches mudam por bloco,
como no ToneStack existente; mudanças rápidas podem gerar transientes.
`reset()` limpa os históricos e mantém os parâmetros escolhidos.

O amp combina **entrada, quatro triodos de ganho, dois seguidores e redes de EQ derivados do esquema sob hipóteses simplificadas**
com os demais blocos aproximados; não é uma reprodução medida nem uma captura do
amplificador. Falta comparar com gravações DI
de referência e ouvir com diferentes guitarras, IRs e níveis de entrada.

## Verificação

```sh
cmake --build build-ironheart -j 6
ctest --test-dir build-ironheart --output-on-failure
cmake --build build-macos26 --target ibifx_desktop -j 6
./build-macos26/ibifx_desktop --amp amp.ironheart-120
```

O diretório `build-ironheart` foi configurado sem desktop para testar o DSP.
Neste Mac, o compilador requer selecionar explicitamente o SDK 26.5:

```sh
cmake -S . -B build-ironheart -DIBIFX_BUILD_DESKTOP=OFF -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
```

Os testes cobrem a seleção e independência de canais, EQ compartilhado,
boost, pull switches, volume zero, controles master, cauda do reverb,
reset, taxas de amostragem, tamanhos de bloco, catálogo e presets.
Para estudar o código, acompanhe `configure()` e depois `process()` no pré:
por que Clean ignora os dois controles Rhythm, e por que o boost precisa
entrar antes da saturação para alterar a distorção?

## Referências

### Avaliação das sugestões de modelagem

O texto técnico compartilhado pelo usuário contribui com a organização por
estágios, filtros de acoplamento, oversampling e o uso do tone stack completo
de terceira ordem. Esses elementos já são usados pelo modelo. A sugestão de
recalcular o EQ apenas quando os controles mudam também foi incorporada.

Algumas afirmações requerem esquemático ou medições antes de alterar o som:

- O Mid Shift para cima descrito no texto contradiz a documentação da
  Laney, que indica deslocamento para baixo; mantemos a direção documentada.
- A Laney descreve Watts como controle de nível dentro da potência. Não
  adotamos a proposta de compensar automaticamente para volume constante,
  que impediria o controle de reduzir a saída como esperado.
- O esquema de serviço de 2011 permitiu conferir os componentes da entrada
  e derivar o boost original; não sustentamos a hipótese de corte de 720 Hz.
  Ele também identifica Tone e Watts duplos, ainda sem modelagem dessa rede.
  O roteamento até V3, o shunt do Clean e os tone stacks foram incorporados;
  volumes, buffers e redes posteriores ainda são aproximados. O manual BCC identifica
  outra revisão do boost, que não deve ser atribuída ao IRT120H original.
- Dynamics como controle na realimentação é uma direção útil para um modelo
  físico futuro. O módulo atual aproxima sua resposta grave com filtros;
  não resolve a malha elétrica NFB nem usa WDF/ODE.

- [IRT120H original — Laney](https://www.laney.co.uk/discontinued/irt120h).
- [Manual IRT120H/IRT60H — Laney](https://manuals.laney.co.uk/IRT120H_IRT60H_Manual_ML.pdf).
- [Manual BCC-IRT60H/120H — Laney](https://manuals.laney.co.uk/BCC-IRT60H_BCC-IRT120H_Manual_EN_2024.pdf), consultado para explicações de operação dos controles comuns. O modelo não incorpora a revisão de boost exclusiva do BCC.
