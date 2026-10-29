"""Compiles and runs the loader firmware's pure parts (main/payload.c) on this computer.  Run: python3 test_payload.py  (needs clang)"""
import os, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__))
exe = os.path.join(tempfile.mkdtemp(), "test_payload")
r = subprocess.run(["clang", "-Wall", "-Wextra", "-o", exe, os.path.join(here, "test_payload.c"), os.path.join(here, "..", "main", "payload.c")],
                   capture_output=True, text=True)
if r.returncode != 0:
    sys.exit("could not build the test:\n" + r.stderr)
out = subprocess.run([exe], capture_output=True, text=True)
print(out.stdout.strip())
sys.exit(out.returncode)
