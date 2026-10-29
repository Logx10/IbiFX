# Interface desktop — análise e plano da branch `Interface`

> Estado: a cadeia linear com ordem livre já está na `DesktopUI` (ImGui/SDL3),
> com `Preset::chainItems`, linhas `item` no preset e
> `GearLibrary::buildSignalChain()`. Ainda faltam `instanceId`, a linha
> `format 2` e a `--gear` da CLI. A tecnologia da interface não foi migrada.
> Este documento não autoriza mudanças no `LiveEngine`, nos processadores DSP
> ou no callback de áudio para atender apenas à apresentação.

## Objetivo

Construir uma interface de montagem modular inspirada no fluxo de trabalho do
AmpliTube, com identidade própria do IbiFX, e separar a área de prática da
pedaleira. O motor C++ continua independente da tecnologia usada para desenhar
a janela.

Referência de fluxo: [página oficial do AmpliTube 5](https://www.ikmultimedia.com/products/amplitube5/).

## Arquitetura encontrada

O sentido atual das dependências é adequado:

```text
DesktopUI / PedalboardUI
          |
          | API de controle e leitura de estado
          v
      LiveEngine
          |
          | CommandQueue da UI + CommandQueue do MIDI
          v
      ModuleChain -> AudioModule -> DSP
          ^
          |
          +--- callback do dispositivo de áudio
```

- `ibifx_core` não depende de SDL3, ImGui ou das interfaces.
- `DesktopUI` recebe um `LiveEngine&`, percorre a cadeia para montar controles
  genéricos e mantém seus próprios alvos de knobs em `m_targets`.
- Parâmetro, bypass e reset são enviados por uma fila SPSC. O callback retira
  os comandos no começo do bloco, sem bloqueio e sem alocação.
- MIDI usa outra fila SPSC, porque cada fila admite um único produtor.
- Picos e estados do transporte são publicados com atômicos e lidos pela UI.
- Trocar preset, rig ou backing track é estrutural: a interface para o
  dispositivo, altera os dados no domínio de controle e religa o áudio.

Há uma separação de dependência, mas `DesktopUI` ainda acumula apresentação e
orquestração: desenho, catálogo, arquivos, presets, construção de rig e ciclo
de vida do dispositivo estão na mesma classe. Isso funciona para C++/ImGui,
porém é o principal ponto a estudar antes de conectar outro frontend.

## Montagem de cadeia que já existe

| Capacidade | Situação atual | Limite atual |
| --- | --- | --- |
| Cadeia dinâmica | `ModuleChain` possui `add`, `remove`, `move` e `clear`. | Mudanças estruturais só são seguras com o áudio parado. |
| Controles genéricos | A UI descobre módulos e parâmetros por `AudioModule`. | Endereçamento é por índice; reconstruir a cadeia invalida índices antigos. |
| Ajuste durante reprodução | Parâmetro, bypass e reset usam `CommandQueue`. | A fila da UI admite um produtor e tem capacidade fixa. |
| Equipamentos | `GearLibrary` descreve Stomp, Amp, Cab e Rack como receitas de módulos. | O modelo `Rig` fixa Stomp → Amp → Cab → Rack, com um Amp e um Cab. |
| Ordem | Stomps e racks são vetores ordenados e já podem ser reordenados. | A interface usa botões; ainda não existe arrastar e soltar. |
| Presets | Capturam ordem, bypass, parâmetros, IR e identificação do rig. | Aplicar reconstrói a cadeia e causa uma interrupção breve. |
| Cabinets | IRs são descobertas, importadas, removidas e aplicadas ao rig. | O arquivo precisa ser carregado no domínio de controle. |
| MIDI | Comandos normalizados entram por uma fila própria. | Não muda a estrutura da cadeia. |
| Roteamento | A cadeia DSP atual é linear e mono; `ModuleChain` já pode processar tipos repetidos, inclusive mais de um amp/cab expandido em módulos. | O `Rig` atual impede essas combinações; ramos paralelos e mixer não existem no motor. |

Assim, uma montagem visual com navegador, seleção, remoção e reordenação de
stomps/racks pode reutilizar o motor atual. Arrastar um item é um novo gesto de
interface que pode terminar em `applyRig()`. Roteamento paralelo e mistura de
caminhos são funcionalidades novas do motor, não tarefas de desenho.

## Decisão arquitetural: cadeia linear com ordem livre

A ordem Stomp → Amp → Cab → Rack passa a ser um template de rig, não uma
restrição do modelo. A cadeia de processamento continua linear e mono, mas
qualquer equipamento pode ser acrescentado, removido ou movido para qualquer
posição. O mesmo modelo pode aparecer várias vezes e nenhum tipo é obrigatório.

O estado canônico que produz áudio continua sendo a lista plana e ordenada de
`Preset::ModuleState`. O catálogo apenas fornece receitas para criar novos
itens. Depois da criação, o preset contém os módulos expandidos e não depende
do catálogo para restaurar o som.

### Impacto por componente

#### `Rig`

É o componente mais afetado. Seus quatro campos codificam ordem, cardinalidade
e categorias:

```cpp
stomps[] -> amp? -> cabinet? -> rack[]
```

Ele não representa dois amplificadores, um stomp depois do cabinet ou um
cabinet antes do amp. A migração não deve apagar `Rig` imediatamente. Ele
permanece temporariamente como:

- formato de entrada das flags antigas da CLI;
- metadado de presets antigos;
- gerador do template de rig padrão;
- adaptador de compatibilidade para a interface atual.

O novo modelo de aplicação será uma lista ordenada, sem campos por categoria.
Uma representação incremental possível é:

```cpp
struct ChainItemState
{
    std::string instanceId;  // identifica esta ocorrência na UI/preset
    std::string gearId;      // origem no catálogo; vazio para módulo avulso
    std::size_t moduleCount; // quantos ModuleState consecutivos formam o item
};

struct SignalChainLayout
{
    std::vector<ChainItemState> items;
};
```

`instanceId` diferencia duas ocorrências do mesmo `gearId`. A primeira
posição de cada item é derivada pela soma dos `moduleCount` anteriores, em vez
de ser persistida como índice duplicado. A soma deve cobrir exatamente
`Preset::modules`.

Esse layout é metadado de edição. O áudio continua sendo reconstruído somente
de `Preset::modules`. Se um modelo desaparecer do catálogo, o preset ainda
pode tocar; a UI cai para a visualização genérica dos módulos daquele item.

#### `GearLibrary`

`GearCategory` continua útil para organizar e filtrar o navegador, mas deixa
de validar posição na cadeia. A biblioteca passa a ter duas responsabilidades:

1. listar e descrever modelos disponíveis;
2. instanciar uma cópia da receita de um `gearId`.

O caminho genérico não deve chamar `require(id, categoriaEsperada)`, impor um
amp/cab único nem acrescentar `Limiter` automaticamente. O limiter pode fazer
parte do template padrão e continuar removível como qualquer outro item.
Removê-lo pode permitir saída acima da faixa; isso deve ser uma escolha visível
do usuário, não uma regra escondida no catálogo.

`rigModels()`, `buildPreset(Rig)`, `matchesRig()` e `inferRig()`
continuam durante a transição. Depois passam para um adaptador legado ou são
removidos quando os presets e frontends não dependerem mais deles.

`GearControl::moduleOffset` continua válido: ele é relativo ao início da
receita de uma instância, independentemente da posição global ou da existência
de outra instância do mesmo modelo.

#### `ModuleChain`

Já atende ao núcleo da decisão:

- guarda uma sequência linear de `unique_ptr<AudioModule>`;
- aceita cadeia vazia;
- aceita tipos repetidos;
- possui `add`, `remove`, `move` e `clear`;
- processa rigorosamente na ordem do vetor;
- não conhece `Rig`, `GearLibrary` ou categorias.

Não é necessário alterar seu processamento. As operações estruturais continuam
fora do callback e, inicialmente, com o dispositivo parado.

Há um cuidado adicional: comandos pendentes são endereçados por índices. Antes
de reconstruir a cadeia, o domínio de controle deve garantir que comandos da
revisão anterior não sejam aplicados aos novos índices. A migração deve testar
essa situação e decidir entre descartar filas com o motor parado ou marcar
comandos com uma revisão da cadeia. Isso é uma correção de consistência, não
uma razão para levar mudanças estruturais ao callback.

#### Presets e serialização

O formato atual já preserva a parte mais importante:

- cada linha `module` mantém a ordem completa;
- parâmetros e bypass pertencem a cada ocorrência;
- tipos repetidos são válidos;
- `preset::apply()` ignora `Preset::rig` e reconstrói a lista plana.

As linhas `rig ...` são apenas metadados visuais. Portanto, presets antigos
podem continuar produzindo exatamente o mesmo áudio.

Para preservar agrupamento por equipamento e instâncias repetidas, a proposta
é introduzir um formato versionado e aditivo:

```text
format 2
preset Meu som
item item-1 stomp.yellow-drive 1
module SoftClipper 0
param drive 4
item item-2 amp.brit-800 7
module HighPass 0
...
item item-3 - 1
module Limiter 0
```

Os nomes são uma proposta de contrato, não uma implementação pronta.
`item` descreve agrupamento; as linhas `module`, `param` e `ir`
continuam sendo a verdade do áudio.

Regras de compatibilidade:

- ausência de `format` significa formato atual;
- presets atuais carregam sem alterar módulos, parâmetros ou bypass;
- um `Rig` antigo válido é convertido para itens na ordem antiga;
- o Limiter acrescentado pelo rig antigo vira um item removível;
- se não houver rig, cada módulo pode virar um item genérico;
- `inferRig()` pode ajudar na importação, mas nunca deve ser necessário para
  tocar o preset;
- modelo ausente ou receita alterada perde o painel especializado, mas não o
  áudio salvo;
- metadado de layout inválido é descartado com aviso; os módulos permanecem;
- novos presets serão salvos no formato novo depois que o leitor antigo
  estiver coberto por testes de regressão.

#### `LiveEngine`

Não precisa conhecer equipamentos nem layouts. Ele continua responsável por:

- dispositivo e callback;
- preparação da cadeia;
- fila de comandos de parâmetros/bypass/reset;
- medidores e serviços de prática/gravação.

A primeira versão da edição estrutural reutiliza o fluxo atual:

```text
validar nova cadeia -> parar dispositivo -> aplicar preset -> preparar/religar
-> reconstruir o snapshot/índices da UI
```

Não serão adicionados comandos estruturais à fila SPSC nesta migração. Uma
troca de cadeia sem corte de áudio exigiria double buffering ou troca atômica
de grafo e permanece uma evolução separada.

#### Interface, CLI e templates

`DesktopUI::m_rig` e `RigSegment` passam gradualmente para uma lista de
instâncias ordenadas. O navegador continua separado e usa categorias apenas
como filtros. IDs ImGui devem incluir `instanceId` para duas cópias do mesmo
equipamento não colidirem.

As flags existentes `--stomp`, `--amp`, `--cab` e `--rack` continuam
montando o template clássico. Uma opção repetível como `--gear ID` pode ser
adicionada depois para preservar exatamente a ordem digitada e permitir
duplicatas. As flags antigas só devem ser depreciadas depois que todos os
frontends entenderem o modelo novo.

O rig clássico atual, inclusive seu Limiter final, torna-se um template
inicial. O usuário pode começar vazio, escolher o template ou carregar um
preset.

### Migração incremental proposta

1. **Congelar compatibilidade.** Adicionar fixtures textuais do formato atual e
   testes para ordem, parâmetros, bypass, IR, rigs antigos e tipos repetidos.
2. **Adicionar o modelo novo sem remover `Rig`.** Criar o layout ordenado,
   validação de cobertura dos módulos e IDs de instância. Nenhum frontend muda.
3. **Separar instanciação do catálogo.** Acrescentar uma operação por
   `gearId` que devolva a receita sem impor categoria ou Limiter. Manter
   `buildPreset(Rig)` como adaptador legado.
4. **Introduzir serialização versionada.** Ler formato atual e formato novo;
   salvar no novo apenas depois dos testes de migração. `preset::apply()`
   continua trabalhando com a lista plana.
5. **Migrar a `DesktopUI` preservando o visual.** Substituir internamente
   `m_rig` pelo layout ordenado, mas manter inicialmente os mesmos botões e
   a mesma ordem produzida pelo template clássico.
6. **Liberar a edição.** Habilitar adicionar, remover, duplicar e reordenar
   qualquer item. Cada alteração estrutural usa o stop/apply/start existente e
   atualiza todos os índices/alvos da UI.
7. **Evoluir a CLI.** Adicionar a entrada ordenada genérica e manter as flags
   antigas como atalhos do template clássico.
8. **Desativar o legado gradualmente.** Depois que presets, CLI e interfaces
   estiverem no modelo novo, mover inferência e conversão de `Rig` para um
   leitor de compatibilidade. Remover somente após confirmar que não há mais
   consumidores.

### Testes necessários durante a migração

- cadeia vazia e cadeia sem Limiter;
- duas ou mais instâncias do mesmo equipamento;
- amp depois de cabinet e stomp depois de rack;
- mover, remover e inserir itens com receitas de vários módulos;
- parâmetros e bypass diferentes em duas instâncias iguais;
- ida e volta do novo formato sem perder ordem ou agrupamento;
- carregamento de todos os formatos antigos;
- preset antigo com e sem linhas `rig`;
- fallback quando o `gearId` não existir mais;
- comandos pendentes durante uma reconstrução estrutural;
- manutenção das flags antigas e do template clássico;
- callback sem novas alocações, locks ou I/O.

## Ideia registrada: aba exclusiva de Prática

A pedaleira e a prática passam a ser espaços de trabalho distintos:

```text
+--------------------------------------------------------------+
| Preset | Pedaleira | Prática                 áudio | medidores |
+--------------------------------------------------------------+
| Prática: transporte, tempo, metrônomo e seleção da faixa      |
+--------------------------------------------------------------+
| backing track  [~~~~ forma de onda ~~~~]                      |
| loop A/B              [================]                      |
| gravação      [--------- tomada / progresso --------]         |
|                              ^ agulha                         |
+--------------------------------------------------------------+
| início | tocar/pausar | gravar | A | B | loop | zoom/scrub    |
+--------------------------------------------------------------+
```

### O que os objetos atuais já fornecem

- play, stop e seek;
- posição atual em amostras;
- duração da backing track;
- início, fim e estado do loop A/B;
- estado da gravação da sessão;
- BPM, metrônomo e volumes;
- gravação DI/processada separada e gravação da sessão misturada.

Com isso já é possível desenhar a aba, a régua de tempo, a região A/B, a
agulha, o scrub e o progresso temporal da gravação sem mudar o motor.

### O que pode ficar fora do motor de áudio

- A forma de onda da backing track pode ser reduzida a picos mínimos/máximos
  lendo o WAV no domínio de controle. A UI desenha esse resumo e nunca acessa
  amostras pelo callback.
- Depois que uma gravação termina, sua forma de onda pode ser calculada do WAV
  salvo pelo mesmo processo.
- A lista de tomadas, nomes, cores, zoom e seleção são estado de aplicação e
  apresentação.

Esse cálculo deve ser assíncrono ou previamente armazenado se o arquivo for
grande, para não congelar a janela. Ele não pertence a um processador DSP.

### O que seria funcionalidade nova

- forma de onda atualizada ao vivo durante a gravação requer um resumo seguro
  produzido fora do callback ou dados de progresso expostos à aplicação;
- clipes editáveis, várias tomadas alinhadas, overdub e desfazer exigem um
  modelo de sessão que ainda não existe;
- o loop atual repete uma região do transporte. Um looper de guitarra que
  grava e sobrepõe camadas é outro recurso;
- edição não destrutiva, cortes e movimentação de clipes também são recursos
  novos, embora possam reutilizar partes de gravação e transporte.

Esses itens não justificam acoplar UI ao `LiveEngine`. Se forem priorizados,
devem ganhar contratos próprios de aplicação/áudio e testes próprios.

## Apresentação versus suporte novo

| Pedido de produto | Apenas apresentação/orquestração atual | Suporte novo necessário |
| --- | --- | --- |
| Abas Pedaleira e Prática | Sim | Não. |
| Cadeia sempre visível | Sim | Não. |
| Busca, filtros e equipamento selecionado | Sim | Não. |
| Drag and drop de stomp/rack | Sim, traduzindo o resultado para `Rig`. | Não, aceitando a pausa estrutural atual. |
| Forma de onda da backing track | Sim, resumo calculado do arquivo fora do callback. | Não no motor. |
| Agulha, scrub e loop A/B | Sim, o transporte já expõe os dados. | Não. |
| Mostrar gravação em andamento | Sim para estado e tempo. | Resumo ao vivo exige uma fonte de dados nova e segura. |
| Reordenar qualquer equipamento sem regras | Parcial. | Exige rever `Rig`/`GearLibrary`; pode continuar com cadeia linear. |
| Troca estrutural sem corte de áudio | Não. | Exige estratégia de troca de cadeia no motor. |
| Rotas paralelas e wet/dry/wet | Não. | Exige grafo/roteamento no motor. |
| Mixer de múltiplos cabs/microfones | Não. | Exige múltiplos caminhos e mistura no motor. |
| Looper com camadas/overdub | Não. | Exige modelo e processamento de looper. |

## A API atual serve diretamente para React?

Ainda não. A independência do motor torna a adaptação possível, mas a API
pública atual foi desenhada para outro código C++ no mesmo processo:

- expõe referências para `LiveEngine`, `ModuleChain`, `AudioModule`,
  `PracticeSession` e gravadores;
- usa `std::string`, `std::vector`, classes virtuais, exceções e ownership
  C++, que não formam uma ABI estável para Rust/TypeScript;
- a UI descobre parâmetros diretamente e envia comandos por índices;
- não existe um DTO/snapshot serializável do estado completo da aplicação;
- ações de arquivo, rig e ciclo de vida estão dentro de `DesktopUI`;
- `CommandQueue` exige um produtor. Handlers Tauri síncronos/assíncronos não
  devem chamar a mesma fila concorrentemente de threads diferentes.

Uma ponte segura precisaria preservar este caminho:

```text
React
  | invoke/event
  v
Tauri (Rust)
  | FFI com API C estável
  v
Controlador C++ serializado (único produtor da fila da UI)
  | comandos / snapshots
  v
LiveEngine -> CommandQueue -> callback
```

O controlador seria dono do ciclo de vida e transformaria comandos com IDs
estáveis em índices válidos no instante da execução. Medidores e transporte
seriam snapshots pequenos, consultados ou publicados numa cadência de UI. O
callback não faria IPC, serialização, alocação ou chamada de WebView.

Para desktop, uma biblioteca C++ no mesmo processo, chamada por FFI, tende a
preservar melhor ciclo de vida e baixa latência que um processo sidecar. Um
sidecar continua possível, mas acrescenta protocolo, sincronização, falhas de
processo e empacotamento por arquitetura.

Para a web, o React pode ser compartilhado, mas o backend Tauri não. Seria
necessário outro adaptador:

```text
React compartilhado
  +-- Desktop: Tauri IPC -> Rust -> FFI C++
  +-- Web:     adapter TS -> C++/WASM -> AudioWorklet
```

O contrato de comandos e snapshots pode ser comum aos dois adaptadores. A
implementação do transporte e do áudio será diferente.

## Comparação das duas direções

| Critério | Evoluir ImGui + SDL3 | React + Tauri |
| --- | --- | --- |
| Reuso do motor | Direto; já usa os alvos CMake e as APIs C++. | Alto, mas exige uma ponte Rust/C++ e um contrato serializável. |
| Risco para tempo real | Baixo se mantido o caminho atual. | Controlável, desde que IPC termine no controlador e nunca no callback. |
| Complexidade inicial | Baixa; a base já compila e funciona. | Alta: Node/React, Rust/Tauri, FFI, DTOs e sincronização de estado. |
| Liberdade visual | Boa com desenho customizado; exige mais código manual. | Muito alta com HTML, CSS, SVG/Canvas e ecossistema web. |
| Timeline e waveform | Viáveis com `ImDrawList` e resumo de picos. | Naturais em Canvas/SVG; há mais ferramentas de layout. |
| Drag and drop | Suportado pelo ImGui; precisa implementar os gestos. | Natural no DOM; ainda precisa traduzir o gesto para comandos C++. |
| Manutenção | Principalmente C++ e CMake. | C++, Rust, TypeScript, bundler e contratos entre camadas. |
| Testes de UI | Mais manuais ou testes do controlador em C++. | Componentes React são testáveis isoladamente; integração FFI/IPC cresce. |
| Consumo/runtime | Janela nativa leve, sem WebView. | Usa a WebView do sistema e um processo/core Tauri; precisa ser medido. |
| Empacotamento | Pipeline atual precisa amadurecer `.app`/Windows. | Tauri oferece bundles, mas o motor C++ deve ser ligado e assinado junto. |
| Reuso futuro na web | ImGui/SDL pode compartilhar praticamente a UI inteira em WASM. | Componentes React podem ser compartilhados; backend e áudio precisam de adaptador web. |
| Sofisticação responsiva | Possível, com layout e widgets feitos no projeto. | CSS facilita responsividade, acessibilidade e temas complexos. |
| Custo de migração | Nenhum. | Reescrita da camada gráfica e criação da fronteira de integração. |

## Plano A — evoluir a interface atual

1. Executar a migração incremental da cadeia livre mantendo a aparência e o
   comportamento atuais até a `DesktopUI` usar o layout ordenado.
2. Separar visualmente `Pedaleira` e `Prática` por abas, ainda em
   `DesktopUI`, sem alterar o motor DSP.
3. Na aba Pedaleira, manter preset/status no topo, cadeia livre visível, área
   de edição e navegador de equipamentos.
4. Adicionar drag and drop para qualquer item da cadeia, traduzindo o resultado
   para a lista ordenada e usando o fluxo seguro de reconstrução.
5. Na aba Prática, montar transporte e timeline com backing track, loop A/B,
   gravação e agulha usando o estado atual.
6. Gerar um resumo visual da backing track no domínio de controle e desenhá-lo
   com ImGui. Não passar samples pelo callback para a janela.
7. Só extrair um controlador independente de `DesktopUI` quando houver uma
   segunda interface concreta ou quando testes de orquestração justificarem a
   separação.
8. Validar presets, IRs, parâmetros, MIDI, prática e comportamento durante
   troca estrutural em macOS e Windows.

## Plano B — avaliar React + Tauri sem migrar

1. Definir, em documento, comandos e snapshots independentes de GUI:
   cadeia, catálogo, parâmetro, rig, preset, medidores e prática.
2. Fazer um protótipo mínimo de API C com handle opaco, IDs estáveis e erros
   explícitos; sem expor classes C++ pela ABI.
3. Criar um controlador com uma única thread/produtor para serializar comandos
   vindos do Tauri e alimentar a fila atual.
4. Ligar Rust à API C no mesmo processo e expor poucos comandos Tauri.
5. Provar apenas um fluxo vertical: listar a cadeia, girar um knob, alternar
   bypass e ler medidores. Não reescrever a aplicação inteira.
6. Criar um adapter React para esse contrato e um adapter web de prova para a
   futura saída WASM.
7. Medir em Mac e Windows: uso de CPU/memória, tempo de abertura, fluidez de
   controles, comandos descartados, estabilidade do áudio, tamanho do pacote
   e complexidade do CI.
8. Fazer uma decisão explícita de continuar ou encerrar o experimento. A
   migração só começa depois desse gate.

## Direção recomendada agora

Migrar primeiro o modelo de montagem para a cadeia linear livre e continuar com
ImGui/SDL3 para validar a experiência modular e a aba de Prática. O motor e a
UI atual já entregam quase todo o comportamento necessário para essa evolução.
ImGui permite timeline, waveform, drag and drop e widgets próprios sem
introduzir uma segunda toolchain agora.

React + Tauri permanece uma alternativa válida para maior liberdade visual e
reuso de componentes com a web. Antes de qualquer migração, o protótipo da
ponte deve demonstrar que comandos, snapshots, lifecycle e single-producer
funcionam sem levar IPC ou sincronização bloqueante ao callback.

## Regras de aceite para qualquer direção

- Nenhum include de ImGui, SDL3, React, Tauri ou Rust entra no core/DSP.
- O callback não faz IPC, I/O, locks, serialização ou alocação para a UI.
- Parâmetros, bypass e reset continuam passando pelas filas existentes.
- Cada fila SPSC mantém exatamente um produtor.
- Mudança estrutural usa o domínio de controle até existir outra estratégia
  de motor projetada e testada.
- A interface consome snapshots/atômicos e mantém estado visual local.
- Funcionalidade visual não força alteração no DSP.
- A futura interface web reutiliza um contrato; não presume que o backend
  Tauri exista no navegador.

## Referências técnicas para o experimento

- [Arquitetura do Tauri](https://v2.tauri.app/concept/architecture/)
- [Modelo de processos do Tauri](https://v2.tauri.app/concept/process-model/)
- [Chamadas do frontend para Rust](https://v2.tauri.app/develop/calling-rust/)
- [Wasm Audio Worklets no Emscripten](https://emscripten.org/docs/api_reference/wasm_audio_worklets.html)
