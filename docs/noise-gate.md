# Noise Gate: preservar sustain e reduzir chiado

Posicione **Noise Gate antes da distorção/amp**. Ajuste Threshold começando
baixo, subindo só até controlar o ruído com as cordas abafadas. Se as notas
fracas não abrem o gate, abaixe o Threshold: Release não altera o nível
necessário para reconhecer uma nota.

Release é um tempo: **0,1 segundo = 100 ms**. Quanto maior, mais lentamente
o gate atenua a cauda. Na skin do pedal, o tooltip agora mostra milissegundos
em vez de uma escala normalizada de 0 a 10. A faixa de 10 a 1000 ms e os
valores dos presets existentes foram preservados. Para testar bends,
comece em 150–300 ms e ajuste ouvindo o resultado; esses valores não são uma
calibração para sua guitarra.

## Correção do corte de notas

O detector acompanha a amplitude retificada com um filtro de 5 ms. Uma nota
precisa ultrapassar Threshold para abrir; depois é reconhecida até metade
desse nível (histerese de aproximadamente 6 dB).

Foram corrigidos dois problemas:

1. O Hold de 50 ms agora mede tempo **consecutivo** abaixo do limiar de
   fechamento. Voltar acima dele rearma o contador, mesmo sem atingir o
   limiar superior. Vales separados de vibrato não se acumulam.
2. Durante o Release, uma nota que recupera nível acima do limiar inferior
   volta ao ganho aberto. Não precisa de outra palhetada acima do limiar
   superior. Só depois que a redução alcança 100 dB o gate volta a exigir
   um novo ataque; nesse ponto o ganho é zerado.

O alvo do ganho abre em 2 ms e fecha pela constante de tempo de Release.
Em 100 ms de Release o ganho cai a cerca de 37%, não a zero; zerar só ocorre
após várias constantes de tempo. O histórico atravessa os blocos e é
limpo por `reset()`, sem alocação ou bloqueios em `process()`.

O gate continua fechando para ruído abaixo do limiar inferior. Ele usa
nível, portanto não distingue uma nota de um ruído de mesmo nível:
Threshold alto ainda pode cortar sustain, e baixo demais deixa ruído passar.

## Testes

`tests/test_noise_gate.cpp` inclui regressões para vales repetidos,
recuperação durante fechamento, bend com batimentos de amplitude e
supressão de ruído após a nota. São sinais sintéticos: a validação final
com sua guitarra depende de experimentar a nova versão.

```sh
cmake --build build-ironheart --target test_noise_gate
ctest --test-dir build-ironheart -R noise_gate --output-on-failure
```
