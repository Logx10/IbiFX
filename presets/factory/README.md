# Presets de fábrica

Os seis arquivos `.ibifxpreset` daqui aparecem automaticamente na lista de
presets do desktop. São **interpretações para uma Stratocaster de três single
coils** e para tocar em fones, com ganho moderado e cabinet sempre ativo.
Não reproduzem uma gravação de estúdio nem incluem todos os instrumentos e
camadas de guitarra de cada faixa.

| Preset | Referência e decisão de timbre |
|---|---|
| **California Dusk Lead** | Coda elétrica de *Hotel California*: Tweed com saturação leve, eco discreto e sala. Don Felder descreveu sua Les Paul e Tweed Deluxe; a faixa também tem várias guitarras sobrepostas. [Entrevista de Felder](https://www.guitarworld.com/features/don-felder-eagles-dear-guitar-2013), [entrevista sobre as camadas](https://www.goldminemag.com/interviews/guitar-icon-don-felder-i-plan-to-rock-til-i-drop). |
| **Psycho Funk Clean** | Parte limpa e percussiva de *Sir Psycho Sexy*: ataque claro, pouca ambiência e sem distorção de pedal. Frusciante descreveu os sons limpos e funk do período de *Blood Sugar Sex Magik*. O wah usado em partes da faixa ainda não existe no IbiFX. [Entrevista de 1991](https://invisible-movement.net/press/1991-11-guitarworld), [créditos da faixa](https://www.rhcpsessions.com/sir-psycho-sexy). |
| **Voltage 75 Crunch** | *High Voltage*: amp britânico com crunch aberto, médios presentes e sem delay. Angus Young descreve seu Marshall Super Lead e o registro da faixa em uma tomada. [Entrevista de Angus e Malcolm](https://www.guitarworld.com/features/angus-malcolm-young-dirty-deeds). |
| **Estranged Singing Lead** | Lead de *Estranged*: amp britânico com médios, agudos arredondados, eco e reverb. Slash descreveu o uso de uma Goldtop no captador do braço com o tone fechado para esse solo. [Entrevista de Slash](https://www.theskinny.co.uk/music/interviews/slashs-guitar-clinic). |
| **Youthanasia Tight Rhythm** | Base de *Youthanasia*: saturação em vários estágios, grave contido e médios ainda audíveis, com ambiência mínima. Marty Friedman relatou que usou sua configuração de amplificadores de palco no álbum. [Entrevista de Friedman](https://megadeth.com.ar/entrevistas/megadeth-madhouse-1997-octubre). |
| **Reflective Fusion Lead** | Lead melódico de *Reflective*, do álbum *Sounds of Innocence*: sustain com delay curto e médios definidos. O próprio Kiko descreveu a faixa como de andamento médio e publicou imagens das gravações. [Entrevista de Kiko](https://www.metalkaoz.com/interviews/9808-kiko-loureiro), [vídeo oficial da gravação](https://www.youtube.com/watch?v=nX-UCijeKwo). |

Os cabinets `irs/Factory/` são IRs **sintéticas originais**, apenas um ponto
de partida. Troque pelo seu cabinet preferido depois de carregar o preset.
O Noise Gate foi deixado de fora para não esconder palhetadas leves; se o
ruído dos single coils incomodar, adicione o pedal e regule o threshold a
partir de `0.005`.

Para recriar os arquivos após alterar receitas ou parâmetros:

```sh
python3 tools/generate_factory_irs.py
cmake --build build-macos26 --target ibifx_factory_presets
./build-macos26/ibifx_factory_presets
```
