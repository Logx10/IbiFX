# audio/

Pasta para arquivos de áudio de teste.

Coloque seus `.wav` aqui e processe:

```sh
./build/ibifx audio/entrada.wav audio/saida.wav
```

No desktop, a aba **Prática** também lista os WAVs desta pasta e os de
`assets/samples/`. Escolha uma faixa, toque ou clique na linha do tempo,
marque **A** no início e **B** no fim do trecho. Ao marcar B, a repetição
começa automaticamente. **Repetir trecho** liga ou desliga a repetição;
**Limpar trecho** remove as marcas. O loop repete a faixa de apoio, não
grava uma frase tocada na guitarra. Para registrar a mistura, use
**Gravar sessão**.

Faixas em 44,1 kHz são convertidas à taxa do dispositivo ao carregar.
Arquivos estéreo são reduzidos a mono, como o restante do motor de prática.

Os arquivos de áudio desta pasta **não são versionados** — só este README é.
Áudio ocupa muito espaço e não pertence ao histórico do código.
