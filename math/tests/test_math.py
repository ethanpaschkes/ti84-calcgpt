"""Compiles and runs the tests of the Math app's pure parts (numbers, the triangle solver, the other problem types, the form,
the screen flow) on this computer, and checks that the last build fits in the calculator's RAM.
Run: python3 test_math.py  (needs clang)"""
import os, re, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__))
root = os.path.join(here, "..", "..")
shared = os.path.join(root, "shared")
src = os.path.join(here, "..", "src")
exe = os.path.join(tempfile.mkdtemp(), "math_test")
sources = [os.path.join(here, "math_test.c")] + [os.path.join(src, f) for f in ("num.c", "tri.c", "probs.c", "form.c", "app.c")]
sources.append(os.path.join(shared, "typing.c"))
r = subprocess.run(["clang", "-Wall", "-Wextra", f"-I{shared}", f"-I{src}", "-o", exe] + sources + ["-lm"], capture_output=True, text=True)
if r.returncode != 0:
    sys.exit("could not build the test:\n" + r.stderr)
out = subprocess.run([exe], capture_output=True, text=True)
print(out.stdout.strip())
if out.returncode:
    sys.exit(out.returncode)

# the program loads at 0xD1A87F and RAM for programs ends at 0xD3FFFF (see solver/README.md): read the real size from the map
mapfile = os.path.join(here, "..", "bin", "MATH.map")
if os.path.exists(mapfile):
    m = open(mapfile).read()
    end = 0
    for name in (".text", ".rodata", ".data"):
        hit = re.search(r"^\%s\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)" % name, m, re.M)
        if hit:
            end = max(end, int(hit.group(1), 16) + int(hit.group(2), 16))
    used = (end - 0xD1A87F) / (0xD3FFFF - 0xD1A87F)
    print("MATH uses %.0f%% of program RAM (%d bytes to spare)" % (used * 100, 0xD3FFFF - end))
    if used > 0.75:
        sys.exit("MATH is too big for the calculator")
