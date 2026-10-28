# CalcOS

A shell for the TI-84 Plus CE: a home screen with app tiles, a status bar (date, time, battery), built-in apps, and every program on
the calculator, which it launches and returns from. It runs **on top of** the calculator's own operating system; it does not replace it
(replacing the OS is risky, it can brick the calculator, and it would break the approved test mode).

- Home screen: arrow keys move over the tiles, Enter opens, Clear leaves CalcOS. The known apps of this project (SOLVER = Math,
  CALCGPT = AI Chat, BROWSER = Web, REELS = Screen) appear first if they are on the calculator, then the built-in apps, then every
  other program in alphabetical order.
- **Clock**: clock (Enter switches 12/24 hour), stopwatch, countdown timer (Up/Down minutes, Window/Zoom 10 seconds).
- **Notes**: a text editor with the Alpha/2nd typing layer; saved in the variable CALCNOTE (Graph saves, Clear saves and leaves).
- **System**: OS version, free RAM, battery, number of programs.
- Launching uses the calculator's own "run program, then call me back" feature, so when an app quits you land on the home screen.

Build: `make` (CE C toolchain), send `bin/CALCOS.8xp`, run it from the PRGM menu. (It cannot start itself when the calculator turns on.)
Tests (no calculator): `python3 tests/test_calcos.py`. Not yet run on a real calculator: all the drawing and launching.
