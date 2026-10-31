"""Draw a labelled 100 px grid over a photo (or a crop of it) so features can be located by eye. Usage: grid.py photo out [x0 y0 x1 y1 [scale]]"""
import sys
from PIL import Image, ImageDraw, ImageOps
src, out = sys.argv[1], sys.argv[2]
im = ImageOps.exif_transpose(Image.open(src)).convert("RGB")
x0, y0, x1, y1 = (int(v) for v in sys.argv[3:7]) if len(sys.argv) >= 7 else (0, 0, im.width, im.height)
scale = float(sys.argv[7]) if len(sys.argv) >= 8 else 1.0
crop = im.crop((x0, y0, x1, y1))
if scale != 1: crop = crop.resize((int(crop.width * scale), int(crop.height * scale)), Image.LANCZOS)
# brighten so the dark body shows against the black table
crop = ImageOps.autocontrast(crop, cutoff=1)
d = ImageDraw.Draw(crop)
step = 100 if scale <= 1 else 50
for gx in range((x0 // step) * step, x1 + 1, step):
    px = (gx - x0) * scale
    if 0 <= px < crop.width: d.line([(px, 0), (px, crop.height)], fill=(255, 60, 60) if gx % 500 == 0 else (255, 200, 0), width=1); d.text((px + 2, 2), str(gx), fill=(0, 255, 255))
for gy in range((y0 // step) * step, y1 + 1, step):
    py = (gy - y0) * scale
    if 0 <= py < crop.height: d.line([(0, py), (crop.width, py)], fill=(255, 60, 60) if gy % 500 == 0 else (255, 200, 0), width=1); d.text((2, py + 2), str(gy), fill=(0, 255, 255))
crop.save(out)
print(out, crop.size)
