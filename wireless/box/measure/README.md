# How the calculator was measured (2026-10-29)

Scripts (run with the venv's Python, `python -I measure/<script> ...`; they need Pillow + numpy):

- `grid.py photo out.png [x0 y0 x1 y1 [scale]]` draws a labelled pixel grid over a (brightened) crop so a feature can be located by eye.
- `blobs.py photo rows cols dark_thresh bright_thresh` prints where each row/column is brighter than a threshold (body edges) and the centre + size of every bright blob (the white keys, the lit screen).
- `silhouette.py photo rows cols thresh` prints the dark runs (the calculator against the silver Mac mini in `calc-front-straight.jpg`).

Photos used:

| Photo | What it gave |
|---|---|
| `calc-front-straight.jpg` (calculator lying on the Mac mini, keyboard only) | the cleanest silhouette: width 698 px = 86.9 mm → 8.03 px/mm; bottom edge; all white keys; corner radius |
| `board-wired-beside-calc.jpg` (whole front, screen on, mini-USB plug in the port) | the lit screen, the key rows above the number pad, the port's height (level with the 2nd/mode/del row); width 754 px → 8.68 px/mm; the bottom is ~3 % closer to the camera than the top |
| `calc-front-upside-down-next-to-mac-mini.jpg` (screen off) | the black screen window and the bezel above it (perspective, ±2 mm) |
| `calc-side-with-cover.jpg`, `calc-side-edge-on.jpg` | the slide cover: full length, side lips, grip ridges at the bottom end, a notch in the right lip at the port |

Results in mm, scale = TI's width 86.9 mm (so every PHOTO number is proportional to that):

- key columns at 13.95 / 28.7 / 43.45 / 58.2 / 72.95 from the left edge (pitch 14.75)
- rows from the bottom edge: 0 . (-) 14.7 · enter/on 21 · 1 2 3 27.8 · + 32.5 · 4 5 6 41.1 · − 44.0 · 7 8 9 / log 54.3 · × 55.4 · x² ÷ 66.9 · x⁻¹ 78.2 · math 89.6 · alpha 100.9 · 2nd 112.5 · y= 125.4
- arrow pad centre (65.5, 106.6), outer diameter 19.3, hub 7.7
- key caps: number keys 10.5 × 9.2, operators/enter 10.5 × 7.6, top row 10.5 × 6.3, dark keys 10.3 × 7.4
- lit screen 61 × 46, black window 70.5 × 54 with the window's bottom 10 mm above the y= row, 11 mm of bezel above the window
- body corner radius ≈ 12, port centre level with the 2nd row (≈113 from the bottom) on the RIGHT side edge
- cover: full length, lips ≈ 5 mm up the sides, grip ridges on the bottom 45 mm, a ≈12 mm notch at the port

**Scale problem (settled 2026-10-29 by ruler: the bare body is about 193 mm long, so `body_l = 192.8` and `y_fix = 0.962`).** Adding the photo heights up gives a length of about 200 mm, 4 % more than TI's 192.8 mm (7.59 in).
Either TI's width is a little generous or the length is. `ti84ce.scad` sets `body_l = 200.4` (the photo value) for now, so
`y_fix = body_l / photo_l = 1` and every PHOTO height is used as measured. One ruler measurement settles it: measure the bare
body's length and width (cover off) and set `body_l` / `body_w`; `y_fix` then rescales every PHOTO height (the key rows, the
screen, the port) to the real length automatically. With TI's 192.8 the port would sit at 108.7 mm from the bottom instead of 113.
