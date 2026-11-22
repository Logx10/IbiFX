"""Gera assets/brand/ibifx.ico, o ícone do executável do Windows.

O logo (assets/brand/ibifx-logo.png) é largo e claro: sozinho, num ícone
quadrado, ficaria minúsculo e sumiria numa barra de tarefas clara. Por isso
ele vai centralizado sobre um quadrado arredondado Charcoal (#111315), a cor
de fundo da janela, com um fio Vintage Gold (#C7B08A) na borda.

Uso (da raiz do projeto):
    python tools/make_icon.py

Requer Pillow. O .ico gerado é versionado; só rode de novo se o logo mudar.
"""

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
LOGO = ROOT / "assets" / "brand" / "ibifx-logo.png"
OUT = ROOT / "assets" / "brand" / "ibifx.ico"

CHARCOAL = (17, 19, 21, 255)
GOLD = (199, 176, 138, 255)
SIZE = 256


def main() -> None:
    canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle((4, 4, SIZE - 5, SIZE - 5), radius=48, fill=CHARCOAL, outline=GOLD, width=6)

    logo = Image.open(LOGO).convert("RGBA")
    target_width = int(SIZE * 0.80)
    logo = logo.resize((target_width, round(logo.height * target_width / logo.width)), Image.LANCZOS)
    canvas.alpha_composite(logo, ((SIZE - logo.width) // 2, (SIZE - logo.height) // 2))

    canvas.save(OUT, sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print(f"gerado {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
