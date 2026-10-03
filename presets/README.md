# presets/

Pasta onde a janela desktop (`ibifx_desktop`) salva e lista presets
(`.ibifxpreset`, formato de texto descrito em `src/Serialization.h`).

O CLI também lê e grava arquivos aqui, se você apontar pra cá:

```sh
./build/ibifx audio/entrada.wav audio/saida.wav --gain 4 --save-preset presets/meu-som.ibifxpreset
./build/ibifx_desktop --preset presets/meu-som.ibifxpreset
```

Os arquivos desta pasta **não são versionados** — só este README é, pelo
mesmo motivo de `audio/README.md`: são experimentação pessoal, não
pertencem ao histórico do código.
