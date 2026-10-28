# SOLVER

One program for TI-84 Plus CE trig class: a Desmos-style notebook that solves what you type.

Build: `make` (needs the CEdev toolchain in ~/CEdev). Result: `bin/SOLVER.8xp` (needs the CE C libraries, `clibs.8xg`, on the calculator).

Tests (run on the Mac, no calculator needed): `tests/t_*.c`, e.g.

    cd tests && clang -o t_solve t_solve.c ../src/solve.c ../src/poly.c ../src/exact.c ../src/mathx.c -I ../src -lm && ./t_solve

Notes
- All maths is 32-bit `float` (see `src/mreal.h`). The calculator does `long double` in software at about 2 ms per
  operation (measured: multiply 1.8 ms, `sinl` 26 ms) against 0.034 ms and 0.5 ms for float, so tolerances everywhere
  allow for about 6 digits and recognised exact forms are kept small.
- The calculator's own stack is only about 4 KB. `main()` in `src/main.c` switches to a 32 KB stack in ordinary memory
  (`big_stack`) and the header shows the deepest it has gone, so an overflow shows up as a number before it crashes.
- `src/preset.c` holds the guided forms (Window key); `tests/t_preset.c` checks their answers.
- `src/notebook.c` builds the answer lines without any drawing, so `tests/t_notebook.c` can check the exact text shown.
- Verified on a calculator: solving, speed after the float switch. Not yet: the preset forms.

Size limit (learned the hard way)
- On the calculator a program is loaded into RAM at 0xD1A87F, and RAM for programs ends at 0xD3FFFF. Past that is the
  screen buffer, so a program that is too big crashes the calculator ("RAM cleared").
- The .8xp file is compressed (zx7); the real size is the linker map. Run `python3 check_fit.py` after `make`.
- 64-bit whole numbers (`long long`) and lots of `long double` code make programs huge on this processor. The solver uses
  32-bit integers (`int32_t`) and builds with `-fno-inline`.
