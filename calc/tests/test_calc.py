"""Compiles and runs the tests of CALC's pure parts (the home screen keys, Settings = the shared Wi-Fi screen) on this computer.
Run: python3 test_calc.py  (needs clang)"""
import os, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__))
root = os.path.join(here, "..", "..")
shared = os.path.join(root, "shared")
exe = os.path.join(tempfile.mkdtemp(), "calc_test")
sources = [os.path.join(here, "calc_test.c"), os.path.join(here, "..", "src", "home.c"), os.path.join(here, "..", "src", "launch.c"), os.path.join(root, "calcgpt", "src", "chatmodel.c")]
sources += [os.path.join(shared, f) for f in ("wifi_screen.c", "outbuf.c", "typing.c")]
r = subprocess.run(["clang", "-Wall", "-Wextra", f"-I{shared}", f"-I{os.path.join(root, 'calcgpt', 'src')}", "-o", exe] + sources,
                   capture_output=True, text=True)
if r.returncode != 0:
    sys.exit("could not build the test:\n" + r.stderr)
out = subprocess.run([exe], capture_output=True, text=True)
print(out.stdout.strip())
sys.exit(out.returncode)
