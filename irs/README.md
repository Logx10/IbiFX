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
