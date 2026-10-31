# MATH

A guided, beginner-friendly math app for the TI-84 Plus CE, opened from CALC's **Math** tile (it replaced SOLVER there on
2026-10-30; SOLVER still exists for typing free-form equations). It works with nothing plugged in.

Flow: **home** (a list of problem types with a one-line hint each) -> for a triangle, **"What kind?"** (right triangle / any
triangle) -> a **form** popup (fill the values you know) -> **"Answers are:"** with a diagram drawn from the numbers -> **Graph**
shows the rule used with your numbers plugged in. The help line at the bottom always says what the keys do.

- Problem types: Triangle (SSS, SAS, ASA/AAS, SSA with both triangles of the ambiguous case, right triangle from any 2 values),
  Pythagorean, Quadratic, Equation (a x + b = c x + d), Unit circle (sin/cos/tan with exact forms, radians, quadrant), Stats of a
  list (mean, median, mode, range, standard deviation).
- Impossible inputs are refused with the reason in plain words (which sides can't meet, which sine would pass 1, ...).
- Keys in a form: the number keys, the dot and (-) type; Del erases one character; Enter moves to the next field and then to
  Solve; arrows move; Clear goes back. On the answers: Enter = a new problem, Graph = diagram/steps, Left/Right = the other
  triangle when two fit, Clear = back to the form with the values kept. Home: the **Decimals** row (0 to 4, default 2).
- With the Wi-Fi box plugged in and Online, **Y=** on the answers sends the problem in words to ChatGPT through the same link
  CALCGPT uses and shows the answer (Up/Down scroll, Clear back).

Files: `src/app.c` the screens and keys (no drawing), `src/tri.c` the triangle solver and the to-scale corner positions,
`src/probs.c` the other types, `src/form.c` the form widget, `src/num.c` number formatting / a tiny printf for the steps,
`src/screens.c` all graphx drawing, `src/main.c` the loop and the link. Shared with CALC/CALCGPT: `../shared` (link, draw,
typing, time) and `../calcgpt/src/chatmodel.c`.

Build: `make` (CE C toolchain), then `python3 ../tools/check_send.py --fix` copies `bin/MATH.8xp` to `../send/`. Send MATH.8xp
with TI Connect CE (and `clibs.8xg` once). Tests (no calculator): `python3 tests/test_math.py` (also checks the built size).
