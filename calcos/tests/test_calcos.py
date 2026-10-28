"""Compiles and runs the tests of CalcOS's pure parts (app list, time and date text, notes editor) on this computer.  Run: python3 test_calcos.py"""
import os, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__))
src = os.path.join(here, "..", "src")
exe = os.path.join(tempfile.mkdtemp(), "calcos_test")
r = subprocess.run(["clang", "-Wall", "-Wextra", "-o", exe, os.path.join(here, "calcos_test.c")] + [os.path.join(src, f) for f in ("registry.c", "timefmt.c", "notesmodel.c")], capture_output=True, text=True)
if r.returncode != 0:
    sys.exit("could not build the test:\n" + r.stderr)
out = subprocess.run([exe], capture_output=True, text=True)
print(out.stdout.strip())
sys.exit(out.returncode)
