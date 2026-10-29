# Identidade visual

Como a identidade da marca ibiFX ("Vintage × Modular × Preciso × Boutique")
aparece na interface desktop.

## Logo

O logo em script fica em `assets/brand/`:

- `ibifx-logo.png`: fonte, com fundo transparente;
- `ibifx-logo.bmp`: a mesma imagem em BMP 32 bits, carregada pelo desktop
  com `SDL_LoadBMP` (o SDL3 não lê PNG sem biblioteca extra).

O app procura o BMP em `assets/brand/` a partir do diretório de trabalho, como
já faz com `presets` e `irs`. Se o arquivo não existir, o cabeçalho mostra o
nome em texto. O pacote do CI copia a pasta `assets`.

## Paleta

| Nome | Hex | Uso na interface |
|---|---|---|
| Ivory | `#E8E0CC` | Texto principal, controles das miniaturas |
| Charcoal | `#111315` | Fundo da janela, corpo dos amps |
| Warm Gray | `#9D978A` | Texto secundário, bordas e separadores |
| Vintage Gold | `#C7B08A` | Único acento: títulos, seleção, aba ativa, placa dos amps |

As constantes ficam no topo de `src/gui/DesktopUI.cpp` (`kIvory`, `kCharcoal`,
`kWarmGray`, `kVintageGold`). Cores com função própria continuam fora da
paleta: verde, amarelo e vermelho dos medidores, LED vermelho do pedal e o
indicador de gravação.

## Tipografia

- **Script da marca:** só no logo, como imagem.
- **Rótulos e nomes:** [Unbounded](https://github.com/googlefonts/unbounded)
  SemiBold, uma sans larga com licença OFL, no lugar da Monument Extended
  (comercial) da folha de marca. É usada em caixa alta com espaçamento largo
  (`trackedCaps()`) nos títulos de seção e no slogan, nas abas Pedaleira e
  Prática e no nome serigrafado dos painéis de equipamento.
- **Texto corrido:** a fonte padrão do ImGui.

O arquivo fica em `assets/fonts/Unbounded-SemiBold.ttf`, com a licença em
`assets/fonts/Unbounded-OFL.txt`. Se o arquivo não existir, o app usa a fonte
padrão em todos os lugares.

## Elementos

- Botões, campos e abas em pílula (`FrameRounding` 12).
- Linha dourada fina sob os títulos de seção.
- Os quatro traços inclinados (`drawBrandStripes()`) no canto do editor de
  equipamento.
- O swoosh não é desenhado pela interface; aparece só no logo.
