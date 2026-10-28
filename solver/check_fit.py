#!/usr/bin/env python3
"""Does the built program fit in the calculator's RAM? Run after `make`.

A TI-84 Plus CE program loads at 0xD1A87F and RAM for it ends at 0xD3FFFF; past that is
the screen buffer, so a program that is too big crashes the calculator ("RAM cleared").
Note the .8xp file is COMPRESSED: its size is not the size in memory. This reads the real size
from the linker map. Keep it under about 75% so there is room to spare."""
import re, sys
m = open("bin/SOLVER.map").read()
def sec(name):
    r = re.search(r"^\%s\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)" % name, m, re.M)
    return (int(r.group(1), 16), int(r.group(2), 16)) if r else (0, 0)
end = max(a + b for a, b in (sec(".text"), sec(".rodata"), sec(".data")) if b)
used = (end - 0xD1A87F) / (0xD3FFFF - 0xD1A87F)
print("program ends at 0x%X: %d bytes to spare, %.0f%% of program RAM used" % (end, 0xD3FFFF - end, used * 100))
if used > 0.75:
    print("TOO BIG: shrink it before sending to the calculator"); sys.exit(1)
