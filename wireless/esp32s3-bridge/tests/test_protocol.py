"""Compiles and runs the bridge firmware's pure parts (main/protocol.c) on this computer.  Run: python3 test_protocol.py  (needs clang)"""
import os, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__))
exe = os.path.join(tempfile.mkdtemp(), "test_protocol")
r = subprocess.run(["clang", "-Wall", "-Wextra", "-o", exe, os.path.join(here, "test_protocol.c"), os.path.join(here, "..", "main", "protocol.c")],
                   capture_output=True, text=True)
if r.returncode != 0:
    sys.exit("could not build the test:\n" + r.stderr)
out = subprocess.run([exe], capture_output=True, text=True)
print(out.stdout.strip())
sys.exit(out.returncode)
