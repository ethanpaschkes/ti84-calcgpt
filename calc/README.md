# CALC

A phone-like home screen for the TI-84 Plus CE, made for the Wi-Fi bridge box. Four tiles:

- **Messages**: "Coming soon" (messages between calculators through the box, later).
- **Settings**: the Wi-Fi setup screen (the same code as CALCGPT's [mode] screen, `../shared/wifi_screen.c`): scan, pick a network,
  type its password, "Other network..." for a hidden one.
- **ChatGPT**: runs the CALCGPT program and comes back here when it quits.
- **Math**: runs the SOLVER program and comes back.

The status bar shows the time and **Online** (the box is plugged in and its board has a server) or **Offline**. With the box
unplugged, Math and Settings still work. Arrows move, Enter opens, Clear leaves CALC (in a screen: back).

It runs on top of the calculator's own operating system (it does not replace it). Started from `../calcos` on 2026-10-29.

- `src/main.c`: drawing and the main loop. `src/home.c`: the tiles and their keys (no drawing, tested on a computer). `src/launch.c`: programs are started through a one-line TI-BASIC program CALCRUN = `Asm(prgmNAME)` (a direct launch crashed on OS 5.8.5).
- Shared with CALCGPT, in `../shared`: `link.c` (USB serial link), `wifi_screen.c` + `wifi_draw.c` (Wi-Fi screen), `draw.c`,
  `typing.c`, `keys.h` (one set of key numbers). The board's messages are parsed by `../calcgpt/src/chatmodel.c`.

Build: `make` (CE C toolchain), then `python3 ../tools/check_send.py --fix` copies `bin/CALC.8xp` to `../send/`. Send CALC.8xp,
CALCGPT.8xp and SOLVER.8xp (and `clibs.8xg` once) with TI Connect CE. Tests (no calculator): `python3 tests/test_calc.py`.
