# Assets dos equipamentos

Lista dos equipamentos do catálogo (`src/GearLibrary.cpp`) para produção de
ilustrações. O checklist de progresso está em `assets-equipamentos.xlsx`.

O código usa nomes genéricos. Só o Tweed '59 (e o tone stack, que é o mesmo
circuito) tem a referência escrita no código: o Fender '59 Bassman. As demais
são uma leitura do nome, da cor e do som descrito, marcadas como *inferido*.

Os equipamentos são homenagens white label aos modelos que serviram de base
para a modelagem. O asset pode lembrar o original no formato, nas cores e no
painel, mas não leva logo, nome nem marca do fabricante. O nome exibido é
sempre o do IbiFX (Green Screamer, Brit 800...).

## Pedais (Stomp)

| Nome no IbiFX | Id | Provável referência | Knobs |
|---|---|---|---|
| Noise Gate | `stomp.gate` | Boss NS-2 (o Decay dele = nosso Release; sem o knob Mode) *(inferido)* | Threshold, Release |
| Orange Squeeze | `stomp.orange-squeeze` | MXR Dyna Comp (a versão laranja dos anos 70/80) | Sensitivity, Output |
| Clean Boost | `stomp.clean-boost` | MXR Micro Amp / booster limpo *(inferido)* | Gain |
| Green Screamer | `stomp.green-screamer` | Ibanez Tube Screamer (TS808/TS9) *(inferido)* | Drive, Low Cut, Level |
| Yellow Drive | `stomp.yellow-drive` | Boss SD-1 pela cor; o som (simétrico, transparente) lembra um overdrive transparente *(inferido)* | Drive, Level |
| Black Rodent | `stomp.black-rodent` | ProCo RAT *(inferido)* | Distortion, Low Cut, Level |
| Round Fuzz | `stomp.round-fuzz` | Dallas Arbiter Fuzz Face *(inferido)* | Fuzz, Bias |
| Blue Chorus | `stomp.blue-chorus` | Boss CE-2 (chorus dos anos 80) *(inferido)* | Rate, Depth, Mix |
| Analog Echo | `stomp.analog-echo` | Boss DM-2 / MXR Carbon Copy *(inferido)* | Time, Feedback, Mix |
| Spring Room | `stomp.spring-room` | Reverb de mola de amplificador *(inferido)* | Decay, Damping, Mix |

## Amps

Todos os amps têm o mesmo painel; só os dois Brit têm Presence.

| Nome no IbiFX | Id | Provável referência | Knobs |
|---|---|---|---|
| American Clean | `amp.american-clean` | Fender Twin Reverb *(inferido)* | Gain, Bass, Middle, Treble, Sag, Master |
| Tweed '59 | `amp.tweed-59` | Fender '59 Bassman | Gain, Bass, Middle, Treble, Sag, Master |
| Brit Plexi | `amp.brit-plexi` | Marshall 1959 Super Lead "Plexi" *(inferido)* | Gain, Bass, Middle, Treble, Presence, Sag, Master |
| Brit 800 | `amp.brit-800` | Marshall JCM800 2203 *(inferido)* | Gain, Bass, Middle, Treble, Presence, Sag, Master |
| Modern Hi-Gain | `amp.modern-hi-gain` | Mesa Dual Rectifier / EVH 5150 *(inferido)* | Gain, Bass, Middle, Treble, Sag, Master |

## Rack

Efeitos de estúdio sem modelo específico; podem ser unidades de rack genéricas.

| Nome no IbiFX | Id | Provável referência | Knobs |
|---|---|---|---|
| Slapback | `rack.slapback` | Eco curto rockabilly (tape slap, tipo Echoplex) *(inferido)* | Time, Feedback, Mix |
| Digital Delay | `rack.digital-delay` | Delay digital limpo (tipo TC 2290) *(inferido)* | Time, Feedback, Mix |
| Long Echo | `rack.long-echo` | Eco longo para solo *(inferido)* | Time, Feedback, Mix |
| Room Reverb | `rack.room` | Reverb de sala pequena *(inferido)* | Decay, Damping, Mix |
| Hall Reverb | `rack.hall` | Reverb de salão (tipo Lexicon) *(inferido)* | Decay, Damping, Mix |
| Output Limiter | `rack.output-limiter` | Limiter de saída genérico *(inferido)* | Threshold |

## EQ

Na signal chain usam o formato de rack.

| Nome no IbiFX | Id | Provável referência | Knobs |
|---|---|---|---|
| High Pass | `eq.high-pass` | Filtro corta-grave genérico *(inferido)* | Frequency |
| 3-Band Tone Stack | `eq.tone-stack` | Tone stack passivo do Fender '59 Bassman | Bass, Mid, Treble, Level |

## Cabinets

Cabinets não ficam no catálogo: cada pasta em `irs/` vira um cabinet com o nome da pasta. Como dependem das IRs importadas, vale ter assets genéricos além dos que vêm com o app.

| Nome no IbiFX | Id | Provável referência | Knobs |
|---|---|---|---|
| Celestion Vintage30 – Cenzo Townshend Mix | (gerado da pasta em irs/) | Caixa 4x12 com Celestion V30 (estilo Marshall 1960 / Mesa) | Mix |
| Cabinet genérico 4x12 | (asset para qualquer IR importada) | Caixa 4x12 genérica *(sugestão)* | Mix |
| Cabinet genérico 1x12/2x12 | (asset para qualquer IR importada) | Caixa combo/1x12 ou 2x12 genérica *(sugestão)* | Mix |

## Formatos e tamanhos

A signal chain desenha cinco formatos: pedal, amp head, cabinet, rack (também
usado pelo EQ) e módulo avulso. Cada asset aparece grande no editor do
equipamento e pequeno na signal chain, que encolhe até 35% do tamanho
(card de 84 × 86 px em escala 1). O asset precisa continuar reconhecível
nesse tamanho reduzido.
