# irs/

Pasta onde a `GearLibrary` procura **cabinets** (gabinete + microfone),
cada um gravado como uma impulse response `.wav`.

Uma pasta = um gabinete; cada `.wav` dentro dela = um microfone (ou
posição de microfone) gravado naquele gabinete:

```text
irs/
  4x12 Brit/
    SM57 On-Axis.wav
    SM57 Off-Axis.wav
    Ribbon Room.wav
  1x12 Open Back/
    Condenser.wav
```

Isso vira, no catálogo, os cabinets `cab.4x12-brit.sm57-on-axis`,
`cab.4x12-brit.ribbon-room`, `cab.1x12-open-back.condenser`...

Use IRs no **mesmo sample rate** do motor (48 kHz no `--live`/desktop) e,
de preferência, **curtas** (até ~200 ms): o `Cabinet` ainda convolve de
forma direta, sem FFT, e o custo cresce com o tamanho da IR.

Os `.wav` desta pasta **não são versionados**, só este README, porque
cada pacote de IR tem a sua própria licença.

A exceção são as IRs sintéticas e originais em `irs/Factory/`: elas vêm com
o IbiFX para que os presets de fábrica funcionem sem download. São filtros
genéricos criados por `tools/generate_factory_irs.py`, não capturas de
um gabinete ou microfone real. Você pode substituir o cabinet do preset
por uma IR sua para chegar mais perto do timbre desejado.

## Exemplo: Celestion Cenzo Townshend Mix

A Celestion disponibiliza esse mix para [download no site oficial](https://www.celestionplus.com/free-download/).
Cada usuário deve obtê-lo por lá: a licença da Celestion não permite
redistribuir os arquivos WAV sem autorização por escrito.

O pacote vem separado por sample rate e duração. Para o motor do IbiFX,
escolha a versão **48.0 kHz / 200 ms** e coloque uma cópia diretamente na
pasta do gabinete, assim:

```text
irs/
  Celestion Vintage30 - Cenzo Townshend Mix/
    Cenzo Celestion V30 Mix.wav
```

O catálogo procura arquivos `.wav` diretamente dentro de cada pasta de
gabinete; as subpastas `48.0 kHz/200 ms/` do download original não são
percorridas. A cópia local continua ignorada pelo Git e não entra nas builds
publicadas no GitHub.
