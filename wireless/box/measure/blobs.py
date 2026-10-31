"""Find the calculator body outline and the bright keys in a front photo. Usage: blobs.py photo [lo_thresh hi_thresh]"""
import sys
import numpy as np
from PIL import Image, ImageOps
im = ImageOps.exif_transpose(Image.open(sys.argv[1])).convert("L")
a = np.asarray(im).astype(float)
H, W = a.shape
# body outline: the table is nearly black; print the luminance profile along a few rows/columns
def runs(mask):
    out, start = [], None
    for i, v in enumerate(mask):
        if v and start is None: start = i
        if not v and start is not None: out.append((start, i - 1)); start = None
    if start is not None: out.append((start, len(mask) - 1))
    return [r for r in out if r[1] - r[0] > 30]
rows = [int(v) for v in sys.argv[2].split(",")] if len(sys.argv) > 2 else [300, 700, 1000, 1400, 1700]
cols = [int(v) for v in sys.argv[3].split(",")] if len(sys.argv) > 3 else [250, 500, 750]
t = float(sys.argv[4]) if len(sys.argv) > 4 else 28
for r in rows: print("row", r, "runs brighter than", t, ":", runs(a[r] > t))
for c in cols: print("col", c, "runs brighter than", t, ":", runs(a[:, c] > t))
# bright blobs (white keys): threshold and label by flood fill
hi = float(sys.argv[5]) if len(sys.argv) > 5 else 150
m = a > hi
lab = np.zeros_like(a, dtype=int); n = 0
from collections import deque
for y in range(H):
    for x in range(W):
        if m[y, x] and lab[y, x] == 0:
            n += 1; q = deque([(y, x)]); lab[y, x] = n
            while q:
                cy, cx = q.popleft()
                for dy, dx in ((1,0),(-1,0),(0,1),(0,-1)):
                    ny, nx = cy + dy, cx + dx
                    if 0 <= ny < H and 0 <= nx < W and m[ny, nx] and lab[ny, nx] == 0: lab[ny, nx] = n; q.append((ny, nx))
print("blobs (area > 800 px): cx cy w h  x0 y0 x1 y1")
for i in range(1, n + 1):
    ys, xs = np.nonzero(lab == i)
    if len(ys) > 800:
        print(f"{xs.mean():7.1f} {ys.mean():7.1f} {xs.max()-xs.min()+1:5d} {ys.max()-ys.min()+1:5d}   {xs.min()} {ys.min()} {xs.max()} {ys.max()}")
