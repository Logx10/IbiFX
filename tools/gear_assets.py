#!/usr/bin/env python3
"""Prepara as imagens geradas no chat para o app (ver docs/prompt-assets.md).

O chat entrega PNGs opacos, com fundo cinza liso, em tamanhos quaisquer. Este
script tira o fundo, recorta, centraliza e reduz cada peça para ~2x o tamanho
em que o app a desenha:

    python3 tools/gear_assets.py body   ENTRADA.png SAIDA.png [--width 480]
    python3 tools/gear_assets.py round  ENTRADA.png SAIDA.png [--size 256]
    python3 tools/gear_assets.py shape  ENTRADA.png SAIDA.png [--width 256]
    python3 tools/gear_assets.py holes  CORPO.png

- body:  corpo do equipamento. Fundo removido a partir das bordas.
- round: peça redonda (knob, LED). Recorta o círculo, com o centro do
         círculo no centro da imagem — o app gira o knob em torno dele.
- shape: peça de outro formato (footswitch). Fundo removido a partir das
         bordas, recortada no conteúdo.
- holes: lista as regiões quase pretas de um corpo JÁ processado (buracos
         de knob, soquetes de LED, furo do footswitch), com centro e
         diâmetro em fração da largura/altura — os números que vão em
         src/gui/GearSkins.cpp.

Requer Pillow (pip install pillow). Só ferramenta de desenvolvimento: nem o
build nem o app dependem dele.
"""

import argparse
from collections import deque

from PIL import Image, ImageDraw, ImageFilter


def luminance(pixel):
    return (pixel[0] * 299 + pixel[1] * 587 + pixel[2] * 114) // 1000


def remove_background(image, tolerance):
    """Apaga, a partir das bordas, os pixels parecidos com a cor do canto.

    Flood fill em vez de "toda cor parecida", para não furar partes do
    objeto que tenham a mesma cor do fundo (um pedal branco sobre cinza
    claro). A borda do recorte é suavizada com um desfoque leve do alfa.
    """
    image = image.convert("RGBA")
    width, height = image.size
    pixels = image.load()

    corners = [pixels[2, 2], pixels[width - 3, 2], pixels[2, height - 3], pixels[width - 3, height - 3]]
    background = tuple(sum(c[i] for c in corners) // 4 for i in range(3))

    def is_background(pixel):
        return max(abs(pixel[i] - background[i]) for i in range(3)) <= tolerance

    # O contorno do objeto costuma ter pixels soltos parecidos com o fundo
    # (a lateral cinza de um pedal branco). Por essas falhas o flood fill
    # entraria no objeto. Um fechamento morfológico (dilatar e erodir) do
    # que NÃO é fundo tapa as falhas antes de o fundo ser apagado.
    solid = Image.new("L", image.size, 0)
    solid_pixels = solid.load()
    for y in range(height):
        for x in range(width):
            if not is_background(pixels[x, y]):
                solid_pixels[x, y] = 255
    solid = solid.filter(ImageFilter.MaxFilter(7)).filter(ImageFilter.MinFilter(7))
    solid_pixels = solid.load()

    mask = Image.new("L", image.size, 255)
    mask_pixels = mask.load()
    seen = bytearray(width * height)
    queue = deque()
    for x in range(width):
        queue.extend([(x, 0), (x, height - 1)])
    for y in range(height):
        queue.extend([(0, y), (width - 1, y)])

    while queue:
        x, y = queue.popleft()
        if x < 0 or y < 0 or x >= width or y >= height or seen[y * width + x]:
            continue
        seen[y * width + x] = 1
        if solid_pixels[x, y]:
            continue
        mask_pixels[x, y] = 0
        queue.extend([(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)])

    mask = mask.filter(ImageFilter.GaussianBlur(0.8))
    image.putalpha(mask)
    return image.crop(image.getchannel("A").getbbox())


def resize_to_width(image, width):
    height = round(image.height * width / image.width)
    return image.resize((width, height), Image.LANCZOS)


def round_piece(image, size, tolerance):
    """Recorta o círculo do objeto e o centraliza num quadrado."""
    cut = remove_background(image, tolerance)
    width, height = cut.size
    diameter = min(width, height)
    left = (width - diameter) // 2
    top = (height - diameter) // 2
    square = cut.crop((left, top, left + diameter, top + diameter))

    # Máscara circular com supersampling para a borda sair lisa.
    scale = 4
    circle = Image.new("L", (diameter * scale, diameter * scale), 0)
    ImageDraw.Draw(circle).ellipse((0, 0, diameter * scale - 1, diameter * scale - 1), fill=255)
    circle = circle.resize((diameter, diameter), Image.LANCZOS)

    alpha = Image.composite(square.getchannel("A"), Image.new("L", square.size, 0), circle)
    square.putalpha(alpha)
    return square.resize((size, size), Image.LANCZOS)


def list_holes(image):
    image = image.convert("RGBA")
    width, height = image.size
    pixels = image.load()
    dark = bytearray(width * height)
    for y in range(height):
        for x in range(width):
            pixel = pixels[x, y]
            if pixel[3] > 200 and luminance(pixel) < 14:
                dark[y * width + x] = 1

    seen = bytearray(width * height)
    regions = []
    for start in range(width * height):
        if not dark[start] or seen[start]:
            continue
        queue = deque([start])
        seen[start] = 1
        count, xs, ys = 0, [], []
        while queue:
            index = queue.popleft()
            x, y = index % width, index // width
            count += 1
            xs.append(x)
            ys.append(y)
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < width and 0 <= ny < height:
                    neighbor = ny * width + nx
                    if dark[neighbor] and not seen[neighbor]:
                        seen[neighbor] = 1
                        queue.append(neighbor)
        box_w = max(xs) - min(xs) + 1
        box_h = max(ys) - min(ys) + 1
        # Só regiões aproximadamente redondas e não minúsculas.
        if count > 80 and 0.75 < box_w / box_h < 1.33:
            regions.append(((min(xs) + max(xs) + 1) / 2, (min(ys) + max(ys) + 1) / 2, (box_w + box_h) / 2))

    print(f"corpo {width}x{height}")
    for cx, cy, diameter in sorted(regions, key=lambda r: (r[1], r[0])):
        print(f"centro ({cx / width:.4f}, {cy / height:.4f})  diametro {diameter / width:.4f} da largura"
              f"   [{cx:.0f}, {cy:.0f}, {diameter:.0f} px]")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("kind", choices=["body", "round", "shape", "holes"])
    parser.add_argument("input")
    parser.add_argument("output", nargs="?")
    parser.add_argument("--width", type=int)
    parser.add_argument("--size", type=int, default=256)
    parser.add_argument("--tolerance", type=int, default=10)
    args = parser.parse_args()

    image = Image.open(args.input)

    if args.kind == "holes":
        list_holes(image)
        return

    if args.output is None:
        parser.error("informe o arquivo de saída")

    if args.kind == "body":
        result = resize_to_width(remove_background(image, args.tolerance), args.width or 480)
    elif args.kind == "round":
        result = round_piece(image, args.size, args.tolerance)
    else:
        result = resize_to_width(remove_background(image, args.tolerance), args.width or 256)

    result.save(args.output, optimize=True)
    print(f"{args.output}: {result.width}x{result.height}")


if __name__ == "__main__":
    main()
