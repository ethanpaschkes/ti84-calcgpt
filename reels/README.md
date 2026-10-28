# Reels on the calculator

Plays video clips on a TI-84 Plus CE. The Mac turns each clip into small 160x120, 16-colour frames
and sends them over the USB cable; the calculator draws them at double size. Sound plays on the Mac.

- `src/main.c` - the calculator program (USB link, keyboard reports); `src/viewer.c` understands the picture messages (the format is in `src/viewer.h`) and has no calculator-specific code, so `tests/test_viewer.py` can run it on the Mac and check every pixel (`make` here, needs CEdev). Send `bin/REELS.8xp` (also copied to `../send/`).
- `mac/reels.py` - the Mac side. Needs ffmpeg. `python3 mac/reels.py [folder or videos] [--fps 15] [--fill] [--shuffle] [--no-audio]`
- `mac/screenshare.py` - live screen share. `python3 mac/screenshare.py [--view follow|whole] [--mode mono|gray|color] [--zoom 0-5] [--dither]`.
  Default is the *follow* view: a window of the screen around the mouse pointer at the calculator's own resolution, so text is
  readable (black and white, crisp). Up/Down zoom, Left/Right change mode, Enter pauses, Clear quits. `--view whole` shrinks the
  whole screen instead (Up/Down pick fresh colours). Only the changed rows of the picture are sent.
  First run: allow the app running it to record the screen (System Settings > Privacy & Security > Screen & System Audio Recording).
- `mac/calckeys.py` - reads the calculator's keyboard reports and, with `--control`, turns them into mouse and keyboard actions
  on the Mac. `mac/test_calckeys.py` checks it with a fake injector (nothing real is touched).
  Control is off unless you start `screenshare.py --control`. It also needs Accessibility permission (System Settings > Privacy &
  Security > Accessibility). The Graph key cycles VIEW (picture keys only) > MOUSE > TYPE; the calculator shows the mode in a badge.
  Clear always quits. MOUSE: arrows move (hold to speed up), Enter = click (hold + arrows = drag), Y= right click, Trace double
  click, Window / Zoom scroll. TYPE: digits and symbols as printed; Alpha toggles letters (the green ones); 2nd = one-shot
  shift (capital, or a symbol: 2nd+1 is !); Del = backspace, Enter = Return, Mode = Tab, Window = Escape, arrows = arrows.
- `clips/` - links to test videos (the player uses this folder when you give none).

Keys on the calculator: Down / Right = next reel, Up = previous, Left = restart, Enter = pause, Clear = quit.

Measured link speed (see `../usbtest`): about 182 KB/s. One frame is 9600 bytes, so 15 frames/s needs about 141 KB/s.
Run on a real calculator: the clip player, whole-screen and follow-view sharing, and mouse/typing control (typing confirmed).
Not yet run on a real calculator: the changed-rows picture updates (checked by compiling the calculator code on a computer and
replaying the Mac script's bytes through it, pixel by pixel). ffmpeg names its 1-bit formats confusingly: `monob` has 1 = white
(what the calculator wants), `monow` has 1 = black.
