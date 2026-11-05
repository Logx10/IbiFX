# Simulação de captadores na entrada

O IbiFX oferece três modos globais no seletor **Captador** da interface desktop:
**Original**, **Single → Humbucker** e **Humbucker → Single**. A conversão ocorre
depois da captura da DI seca e do afinador, antes do primeiro módulo da cadeia.
Assim, qualquer pedal ou amplificador recebe o sinal convertido, e a simulação
não ocupa um slot nem é gravada no preset de efeitos. A DI seca do reamp mantém
o sinal que veio da guitarra; a faixa processada inclui a conversão.

## O que outros simuladores fazem

- O [Blue Cat Re-Guitar](https://www.bluecataudio.com/Doc/Product_ReGuitar/)
  pede o tipo do captador de origem e o tipo desejado; também oferece correção
  de brilho e ganho. Essa separação é necessária porque o mesmo alvo precisa
  de tratamento diferente conforme o sinal recebido.
- A [Line 6 Variax](https://l6c-acdn2.line6.net/data/6/0a060b316ac34f05944058588/application/pdf/Variax%20600%20User%20Manual%20%28%20Rev%20A%20%29.pdf)
  usa um captador piezo por corda e modela características físicas e elétricas
  do instrumento. Essa entrada por corda permite um grau de simulação que não
  existe numa DI mono convencional.
- Medições de [captadores e carga de cabo da Universidade de Illinois](https://courses.physics.illinois.edu/phys406/sp2017/406emi_guitar_pickup_results.html)
  mostram que indutância, capacitância e carga elétrica movem a frequência de
  ressonância e alteram a resposta em frequência.

## Perfis usados pelo IbiFX

Cada perfil genérico é representado por um filtro ressonante de segunda ordem.
Uma conversão aplica a razão entre a resposta do alvo e a da origem, incluindo
uma diferença de nível estimada de 5 dB. A forma dos perfis foi ajustada a
[medições públicas CC0 do pickupAnalyseR-data](https://github.com/strayr/pickupAnalyseR-data),
revisão `7760780fca9abba5bba7c7954ebe244307598ad1`, sob carga de 200 kΩ e
470 pF. O script [fit_pickup_profiles.py](../tools/fit_pickup_profiles.py)
reproduz o ajuste usando apenas a biblioteca padrão de Python. Ele retira a
inclinação de 20 dB por década do método de medição, como faz o
[código original de análise](https://github.com/strayr/pickupAnalyseR/blob/stable/bodeUtils.R),
e minimiza o erro da resposta de 180 Hz a 9 kHz. A mediana de três singles é
**2,85 kHz, Q 1,14**; a de quatro humbuckers é **1,90 kHz, Q 1,11**.

O grupo single usa Squier Affinity Strat, DiMarzio True Velvet e Squier Tele
Classic Vibe ponte. O grupo humbucker usa Epiphone LP ponte, Gibson 498T,
Seymour Duncan JB e IronGear SteamHammer. Esses valores descrevem a resposta
elétrica média dessa pequena amostra, não uma guitarra específica. As medições
não fornecem uma comparação confiável da sensibilidade magnética absoluta entre
os modelos; **os 5 dB de diferença de nível continuam sendo uma hipótese**.
Além disso, outra carga de cabo/potenciômetro desloca as ressonâncias. A troca
de modo é suavizada em cerca de 10 ms para evitar estalos. O processamento é
causal, sem detecção de notas e sem latência de bloco adicional.

As [gravações limpas EGFxSet](https://zenodo.org/records/7044411) cobrem as
cinco posições de uma Strat e servem para observar a variação entre singles.
O [G.A.D.A.](https://gingerpianist.github.io/GADA_demo_page/) inclui DI de
guitarras com singles e humbuckers. Como são instrumentos diferentes, esse
material serve para audição exploratória, não para medir uma transferência
isolada entre os dois tipos de captador. Por isso, o ajuste numérico acima usa
as curvas elétricas medidas, e não uma comparação espectral de riffs diferentes.

O resultado aproxima **timbre e nível**. Um sinal mono já captado não contém a
vibração de cada corda separadamente nem o sinal de uma segunda bobina. Por
isso, não reproduz exatamente o cancelamento de ruído de um humbucker, a janela
espacial de duas bobinas ou a mudança entre ponte e braço. Na conversão inversa,
amplificar agudos também pode amplificar ruído já presente. Para modelos mais
fiéis, o próximo passo é medir pares de DI com diferentes captadores, cabos e
posições, ajustar os perfis e oferecer calibração de brilho e nível por guitarra.
