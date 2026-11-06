# Simulador de captadores do IbiFX

Uma Strat com single coil pode alimentar a pedaleira com um timbre aproximado
de humbucker, sem trocar o captador físico. O IbiFX faz essa conversão na
entrada de áudio, antes dos pedais e amplificadores virtuais.

> **Estado atual:** protótipo funcional com perfis genéricos baseados em
> medições públicas. A semelhança com um humbucker específico ainda precisa
> ser avaliada com gravações comparáveis de instrumentos reais.

## Como usar

Na interface desktop, localize **Captador** ao lado dos medidores **IN** e
**OUT** e escolha um dos modos:

| Modo | Use quando | Resultado esperado |
| --- | --- | --- |
| Original | Quiser ouvir a guitarra sem conversão | Sinal de entrada preservado |
| Single → Humbucker | Estiver usando um single coil, de preferência o da ponte | Mais nível e menos brilho relativo |
| Humbucker → Single | Estiver usando um humbucker | Menos nível e mais brilho relativo |

O modo é global e não ocupa espaço na cadeia de efeitos. Ao mudar de preset,
a escolha do captador continua ativa durante a sessão. A troca de modo é
suavizada para evitar estalos.

```text
Guitarra → DI seca / afinador → simulador de captador → pedais → amp → saída
```

A gravação de reamp guarda a DI original e o sinal processado separadamente.
Assim, o material seco pode receber outro tratamento depois.

## Como funciona

Captadores passivos têm uma resposta elétrica ressonante que depende da bobina,
dos controles da guitarra e da carga do cabo. O IbiFX aproxima essa resposta
com dois perfis genéricos e aplica a razão entre o captador desejado e o
captador de origem. O processamento roda em tempo real, sem detecção de notas
ou latência de bloco adicional.

Os perfis foram ajustados com [medições públicas de captadores](https://github.com/strayr/pickupAnalyseR-data)
sob uma carga de referência de 200 kΩ e 470 pF:

| Perfil | Ressonância aproximada | Q |
| --- | ---: | ---: |
| Single coil | 2,85 kHz | 1,14 |
| Humbucker | 1,90 kHz | 1,11 |

O ajuste pode ser reproduzido com [este script](../tools/fit_pickup_profiles.py).
O ganho de **+5 dB** na conversão para humbucker e de **−5 dB** na inversa
ainda é uma estimativa: as medições usadas não permitem determinar com
segurança o volume absoluto de captadores instalados em guitarras diferentes.

## O que esperar

A simulação aproxima **timbre e nível**. Uma entrada mono não contém o sinal
isolado de cada corda ou de uma segunda bobina. Por isso, o software não
reproduz o cancelamento físico de ruído de um humbucker nem transforma a
posição do captador da ponte em uma captação de braço. A conversão de
humbucker para single também pode realçar ruído que já está na gravação.

As [gravações limpas do EGFxSet](https://zenodo.org/records/7044411) e as
[DI do G.A.D.A.](https://gingerpianist.github.io/GADA_demo_page/) oferecem
referências para audição. Como não são pares controlados de single e humbucker
na mesma guitarra e execução, os parâmetros numéricos foram ajustados às
curvas elétricas medidas.

## Estado da implementação

- O seletor está na [interface desktop](../src/gui/DesktopUI.cpp).
- O processamento está no [PickupSimulator](../src/PickupSimulator.cpp), antes
  da [cadeia de efeitos](../src/platform/LiveEngine.cpp).
- O build desktop e os [testes do simulador](../tests/test_pickup_simulator.cpp)
  passaram no macOS.

Os próximos passos são comparar gravações DI mais controladas, ajustar o nível
e o brilho por guitarra e oferecer perfis específicos de captadores. Os
detalhes técnicos e as fontes estão em [Simulação de captadores na entrada](simulacao-de-captadores.md).
