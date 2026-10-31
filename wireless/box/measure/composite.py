"""Side-by-side and overlay comparison of the rendered calculator with the photos. Usage: composite.py render.png out.png"""
import sys
import numpy as np
from PIL import Image, ImageOps
render = Image.open(sys.argv[1]).convert("RGB")
a = np.asarray(render).astype(int)
bg = a[0, 0]
mask = np.abs(a - bg).sum(axis=2) > 40
ys, xs = np.nonzero(mask)
r = render.crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))      # the calculator's bounding box in the render
print("render bbox", r.size)
# photo 1: calc-front-straight.jpg, body x 690..1388 (698 px = 86.9 mm), bottom edge y 1410, top off-frame
p1 = ImageOps.exif_transpose(Image.open("photos/calc-front-straight.jpg")).convert("RGB")
s1 = 698 / r.width
r1 = r.resize((int(r.width * s1), int(r.height * s1)), Image.LANCZOS)
over = p1.copy()
# paste the render so its bottom-left corner lands on the photo's bottom-left corner, 50 % blend
box = (690, 1410 - r1.height, 690 + r1.width, 1410)
region = over.crop(box)
blend = Image.blend(region, r1, 0.5)
over.paste(blend, box)
over = over.crop((450, 0, 1650, 1500))
# photo 2: board-wired-beside-calc.jpg, body x 148..902, bottom 1786 (top off-frame)
p2 = ImageOps.exif_transpose(Image.open("photos/board-wired-beside-calc.jpg")).convert("RGB")
s2 = 754 / r.width
r2 = r.resize((int(r.width * s2), int(r.height * s2)), Image.LANCZOS)
over2 = p2.copy()
box2 = (148, 1786 - r2.height, 148 + r2.width, 1786)
y0 = max(0, box2[1]); crop_top = y0 - box2[1]
region2 = over2.crop((box2[0], y0, box2[2], box2[3]))
blend2 = Image.blend(region2, r2.crop((0, crop_top, r2.width, r2.height)), 0.5)
over2.paste(blend2, (box2[0], y0))
over2 = ImageOps.autocontrast(over2.crop((100, 0, 1000, 1900)), cutoff=1)
H = 1500
def fit(im): return im.resize((int(im.width * H / im.height), H), Image.LANCZOS)
panels = [fit(ImageOps.autocontrast(p2.crop((100, 0, 1000, 1900)), cutoff=1)), fit(r), fit(over2), fit(over)]
W = sum(p.width for p in panels) + 10 * (len(panels) + 1)
out = Image.new("RGB", (W, H + 20), (255, 255, 255))
x = 10
for p in panels: out.paste(p, (x, 10)); x += p.width + 10
out.save(sys.argv[2]); print(sys.argv[2], out.size)
