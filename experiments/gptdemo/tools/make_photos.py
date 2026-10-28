#!/usr/bin/env python3
"""Makes src/photos.h: the pictures GPTDEMO shows as "photos of a handwritten equation".

Each picture becomes 144x86 pixels in one shared 16-colour palette (the calculator can show about that much), packed two
pixels per byte. The detection box GPTDEMO draws is stored with each picture.

  python3 make_photos.py                                  draws three photo-style pictures first (photos/photo1..3.png)
  python3 make_photos.py --photo me.jpg:0.2,0.4,0.8,0.6 ...   uses your own photos instead (give each one three times, or
                                                              fewer to mix): FILE:left,top,right,bottom of the equation,
                                                              as fractions of the picture width and height
Needs Pillow (pip install pillow); it is only used here, never on the calculator."""
import argparse, math, os, random
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops, ImageEnhance

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_H = os.path.join(HERE, "..", "src", "photos.h")
PHOTO_DIR = os.path.join(HERE, "..", "photos")
W, H = 1440, 860            # drawn at 10x the size shown on the calculator, then shrunk
PW, PH = 144, 86

FONTS = ["/System/Library/Fonts/Supplemental/Bradley Hand Bold.ttf", "/System/Library/Fonts/Noteworthy.ttc",
         "/System/Library/Fonts/MarkerFelt.ttc"]

# (segments of (text, raised?), font index, ink colour, paper kind, tilt degrees)
SCENES = [
    ([("2x + 3 = 11", 0)], 0, (22, 36, 110), "lined", -3.0),
    ([("x", 0), ("2", 1), (" - 5x + 6 = 0", 0)], 1, (50, 52, 60), "grid", 2.5),
    ([("2 sin x = 1", 0)], 0, (18, 18, 24), "lined", -1.5),
]


def wood(rng):
    """A desk: long streaks of grain in brown."""
    noise = Image.effect_noise((W // 30, H // 2), 60).resize((W, H), Image.BICUBIC).convert("L")
    noise = ImageEnhance.Contrast(noise).enhance(1.6)
    tint = Image.merge("RGB", [noise.point(lambda v, k=k: int(60 + v * k)) for k in (0.42, 0.28, 0.17)])
    return tint


def paper_sheet(kind, size, rng):
    pw, ph = size
    sheet = Image.new("RGB", size, (244, 240, 226))
    d = ImageDraw.Draw(sheet)
    if kind == "lined":
        for y in range(190, ph, 95):
            d.line([(0, y), (pw, y)], fill=(176, 198, 224), width=4)
        d.line([(120, 0), (120, ph)], fill=(214, 110, 110), width=4)
    else:
        for y in range(0, ph, 60):
            d.line([(0, y), (pw, y)], fill=(190, 214, 200), width=3)
        for x in range(0, pw, 60):
            d.line([(x, 0), (x, ph)], fill=(190, 214, 200), width=3)
    grain = Image.effect_noise(size, 12).convert("L")
    return ImageChops.multiply(sheet, Image.merge("RGB", [grain.point(lambda v: 232 + v // 12)] * 3))


def write_expression(sheet, segments, font_path, ink, rng):
    """Handwriting: each piece a little off the line and a little turned. Returns the box that holds it."""
    big = ImageFont.truetype(font_path, 135)
    small = ImageFont.truetype(font_path, 84)
    layer = Image.new("L", sheet.size, 0)
    x, base = 200, sheet.size[1] // 2 + 80
    x0, top, bottom = x, base, base
    for text, raised in segments:
        font = small if raised else big
        for ch in text:
            glyph = Image.new("L", (200, 260), 0)
            ImageDraw.Draw(glyph).text((30, 40), ch, font=font, fill=255, stroke_width=3)
            glyph = glyph.rotate(rng.uniform(-5, 5), resample=Image.BICUBIC)
            dy = rng.randint(-8, 8) - (62 if raised else 0)
            layer.paste(255, (x - 30, base - 135 + dy), glyph)
            box = glyph.getbbox()
            if box:
                top = min(top, base - 135 + dy + box[1])
                bottom = max(bottom, base - 135 + dy + box[3])
            x += int(font.getlength(ch)) + rng.randint(-2, 8)
    sheet.paste(Image.new("RGB", sheet.size, ink), (0, 0), layer.filter(ImageFilter.GaussianBlur(1.2)))
    return (x0 - 30, top - 25, x + 20, bottom + 25)


def make_photo(scene, rng):
    segments, font_i, ink, kind, tilt = scene
    desk = wood(rng)
    sheet = paper_sheet(kind, (1200, 700), rng)
    box = write_expression(sheet, segments, FONTS[font_i], ink, rng)
    mark = Image.new("L", sheet.size, 0)
    ImageDraw.Draw(mark).rectangle(box, fill=255)
    pos = (150 + rng.randint(-20, 20), 110 + rng.randint(-10, 10))
    sheet_r = sheet.rotate(tilt, expand=True, resample=Image.BICUBIC, fillcolor=(0, 0, 0))
    cover = Image.new("L", sheet.size, 255).rotate(tilt, expand=True, resample=Image.BICUBIC)
    mark_r = mark.rotate(tilt, expand=True, resample=Image.BICUBIC)
    shadow = Image.new("L", desk.size, 0)
    shadow.paste(cover.point(lambda v: v // 2), (pos[0] + 28, pos[1] + 36))
    desk = Image.composite(Image.new("RGB", desk.size, (20, 12, 8)), desk, shadow.filter(ImageFilter.GaussianBlur(26)))
    desk.paste(sheet_r, pos, cover)
    bb = mark_r.getbbox()
    bbox = ((pos[0] + bb[0]) / W, (pos[1] + bb[1]) / H, (pos[0] + bb[2]) / W, (pos[1] + bb[3]) / H)
    # a camera: light from one side, dark corners, grain, a little blur
    light = Image.linear_gradient("L").rotate(90).resize((W, H)).point(lambda v: 150 + v * 105 // 255)
    desk = ImageChops.multiply(desk, Image.merge("RGB", [light] * 3).point(lambda v: v))
    vig = Image.radial_gradient("L").resize((W, H)).point(lambda v: 255 - v // 3)
    desk = ImageChops.multiply(desk, Image.merge("RGB", [vig] * 3))
    desk = desk.filter(ImageFilter.GaussianBlur(1.6))
    noise = Image.effect_noise((W, H), 14).convert("L").point(lambda v: v - 128)
    desk = ImageChops.add(desk, Image.merge("RGB", [noise] * 3), 1.0, 0)
    return desk, bbox


def to_small(img):
    img = ImageEnhance.Contrast(img).enhance(1.25)
    return img.resize((PW, PH), Image.LANCZOS)


def write_header(smalls, boxes):
    montage = Image.new("RGB", (PW * len(smalls), PH))
    for i, s in enumerate(smalls):
        montage.paste(s, (i * PW, 0))
    q = montage.quantize(colors=16, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
    pal = q.getpalette()[:48]
    pal += [0] * (48 - len(pal))
    lines = ["/* Made by tools/make_photos.py. Do not edit. */", "#define PHOTO_W %d" % PW, "#define PHOTO_H %d" % PH,
             "static const uint8_t PHOTO_PALETTE[16][3] = {"]
    lines += ["    {%d, %d, %d}," % tuple(pal[i * 3:i * 3 + 3]) for i in range(16)] + ["};"]
    lines.append("/* the equation's box in each picture: left, top, right, bottom (pixels) */")
    lines.append("static const uint8_t PHOTO_BOX[%d][4] = {" % len(smalls))
    for b in boxes:
        lines.append("    {%d, %d, %d, %d}," % (round(b[0] * PW), round(b[1] * PH), round(b[2] * PW), round(b[3] * PH)))
    lines.append("};")
    lines.append("static const uint8_t PHOTO_DATA[%d][PHOTO_W * PHOTO_H / 2] = {" % len(smalls))
    for i in range(len(smalls)):
        px = [q.getpixel((i * PW + x, y)) for y in range(PH) for x in range(PW)]
        packed = [px[j] << 4 | px[j + 1] for j in range(0, len(px), 2)]
        lines.append("    {")
        for j in range(0, len(packed), 24):
            lines.append("        " + ",".join("%d" % v for v in packed[j:j + 24]) + ",")
        lines.append("    },")
    lines.append("};")
    with open(OUT_H, "w") as f:
        f.write("\n".join(lines) + "\n")
    q.convert("RGB").resize((montage.width * 4, montage.height * 4), Image.NEAREST).save(os.path.join(PHOTO_DIR, "preview_on_calculator.png"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--photo", action="append", default=[], help="FILE:left,top,right,bottom (fractions); up to 3")
    a = ap.parse_args()
    rng = random.Random(84)
    imgs, boxes = [], []
    if a.photo:
        for spec in a.photo[:3]:
            path, _, nums = spec.rpartition(":")
            imgs.append(Image.open(path).convert("RGB").resize((W, H), Image.LANCZOS))
            boxes.append(tuple(float(n) for n in nums.split(",")))
    else:
        for i, scene in enumerate(SCENES, 1):
            img, box = make_photo(scene, rng)
            img.save(os.path.join(PHOTO_DIR, "photo%d.png" % i))
            imgs.append(img)
            boxes.append(box)
    write_header([to_small(i) for i in imgs], boxes)
    print("wrote", os.path.normpath(OUT_H), "from", len(imgs), "pictures")


if __name__ == "__main__":
    main()
