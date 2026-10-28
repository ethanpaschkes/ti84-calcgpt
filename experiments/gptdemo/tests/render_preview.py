#!/usr/bin/env python3
"""Turns the preview program's pictures (NAME_n.ppm plus NAME_n.txt) into PNG files with the text drawn in a readable font.
Usage: render_preview.py NAME_0 [NAME_1 ...]    (needs Pillow; only used for previews)"""
import sys
from PIL import Image, ImageDraw, ImageFont

ZOOM = 3
FONT = "/System/Library/Fonts/Menlo.ttc"


def main():
    for base in sys.argv[1:]:
        img = Image.open(base + ".ppm").resize((320 * ZOOM, 240 * ZOOM), Image.NEAREST)
        draw = ImageDraw.Draw(img)
        for line in open(base + ".txt"):
            x, y, sc, red, green, blue, text = line.rstrip("\n").split(" ", 6)
            x, y, sc = int(x), int(y), int(sc)
            font = ImageFont.truetype(FONT, int(13.2 * sc * ZOOM))
            for i, ch in enumerate(text):   # one character per 8-pixel cell, like the calculator's fixed-width font
                draw.text(((x + i * 8 * sc) * ZOOM, (y - sc) * ZOOM), ch, font=font, fill=(int(red), int(green), int(blue)))
        img.save(base + ".png")


if __name__ == "__main__":
    main()
