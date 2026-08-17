# Arquitetura — IbiFX

> Visão inicial. Este documento descreve **intenção**, não implementação.
>
> Quase nada aqui descrito existe em código ainda. A única exceção é o
> princípio 2 (DSP independente da UI), que o `GainProcessor` já respeita por
> não ter interface alguma — o que é fácil quando também não há UI.

## Princípios

1. **Core desacoplado da plataforma.**
   O núcleo do IbiFX não deve conhecer sistema operacional, driver de áudio,
   navegador ou toolkit gráfico.

2. **DSP independente da UI.**
   Algoritmos de processamento não sabem que existe interface. A UI observa e
   comanda; ela não contém lógica de áudio.

3. **Arquitetura modular.**
   Efeitos, amp e cabinet são unidades conectáveis com um contrato comum, e não
   uma cadeia fixa escrita no código.

4. **Separação entre real-time domain e control domain.**
   A thread de áudio executa trabalho previsível. Interface, MIDI, presets e
   disco vivem no domínio de controle. A comunicação entre os dois é sempre uma
   decisão explícita de projeto.

5. **Portabilidade futura: desktop e WebAssembly.**
   O mesmo código de DSP deve, sempre que possível, servir às duas plataformas.

6. **Evitar dependência direta do Core em JUCE.**
   JUCE poderá entrar futuramente como camada de plataforma (áudio, UI,
   dispositivos). O Core não deve incluí-lo.

## Visão conceitual

```text
                    IbiFX

                      │
                      ▼

                  IbiFX Core

          ┌───────────┼───────────┐
          │           │           │
          ▼           ▼           ▼

         DSP      Audio Graph   Control

          │                       │
          ▼                       ▼

       Modules                UI / MIDI
```

## Fronteira entre domínios

```text
REAL-TIME DOMAIN
    audio callback
    DSP
    audio graph
    buffers

        ▲   │
        │   │  comunicação explícita e controlada
        │   ▼

CONTROL DOMAIN
    UI
    MIDI
    presets
    filesystem
```

## Estrutura de diretórios

Diretórios sob `src/` serão criados **apenas quando houver código que os
justifique**. A direção prevista é:

```text
src/core/       conceitos centrais (módulos, parâmetros, estado)
src/audio/      fluxo de áudio (buffers, engine, clock)
src/dsp/        algoritmos de processamento
src/modules/    efeitos concretos
src/control/    comandos, eventos, parâmetros
src/io/         entrada e saída
src/presets/    serialização de estado
src/platform/   integração com sistema operacional / navegador
src/ui/         interface
```

Nenhum desses diretórios existe ainda. O `GainProcessor` mora direto em `src/`
de propósito: com um único módulo, criar `src/dsp/` seria organizar uma pasta
antes de haver o que organizar. A subdivisão acontece quando o segundo ou
terceiro módulo tornar a raiz confusa.

## Nota

Esta arquitetura é uma hipótese de trabalho e **deverá evoluir** conforme os
requisitos reais aparecerem. Decisões relevantes serão registradas em
`docs/adr/` quando surgirem.
