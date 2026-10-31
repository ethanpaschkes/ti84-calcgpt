"""Calculator silhouette against the silver Mac mini in calc-front-straight.jpg: for given rows, the dark run between bright runs; for given columns, likewise."""
import sys
import numpy as np
from PIL import Image, ImageOps
im = ImageOps.exif_transpose(Image.open(sys.argv[1])).convert("L")
a = np.asarray(im).astype(float)
t = float(sys.argv[4]) if len(sys.argv) > 4 else 90
def dark_runs(line):
    m = line < t; out, s = [], None
    for i, v in enumerate(m):
        if v and s is None: s = i
        if not v and s is not None: out.append((s, i - 1)); s = None
    if s is not None: out.append((s, len(m) - 1))
    return [r for r in out if r[1] - r[0] > 40]
for r in [int(v) for v in sys.argv[2].split(",")]: print("row", r, dark_runs(a[r]))
for c in [int(v) for v in sys.argv[3].split(",")]: print("col", c, dark_runs(a[:, c]))
