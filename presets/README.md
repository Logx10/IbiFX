# presets/

Pasta onde a janela desktop (`ibifx_desktop`) salva e lista presets
(`.ibifxpreset`, formato de texto descrito em `src/Serialization.h`).

O CLI também lê e grava arquivos aqui, se você apontar pra cá:

```sh
./build/ibifx audio/entrada.wav audio/saida.wav --gain 4 --save-preset presets/meu-som.ibifxpreset
./build/ibifx_desktop --preset presets/meu-som.ibifxpreset
```

Os arquivos salvos diretamente nesta pasta **não são versionados**: são
experimentação pessoal. Os presets em `presets/factory/` vêm com o projeto,
aparecem na mesma lista da janela e são versionados. Salvar pela janela
continua criando arquivos pessoais aqui, sem sobrescrever os de fábrica.
