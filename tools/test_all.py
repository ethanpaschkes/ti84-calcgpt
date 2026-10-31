"""Runs every test in the project on this computer (no calculator, no board, no model):
calculator programs (clang), Mac/cloud Python, the bridge firmware's pure parts, and the send/ freshness check.
Run: python3 tools/test_all.py        Add --build to also run `make` for CALC, CALCGPT and MATH first (needs ~/CEdev)."""
import os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TESTS = [
    "calcgpt/tests/test_calcgpt.py",
    "calcgpt/mac/test_calcgptd.py",
    "calcgpt/mac/test_tools.py",
    "calc/tests/test_calc.py",
    "math/tests/test_math.py",
    "calcos/tests/test_calcos.py",
    "cloud/test_server.py",
    "wireless/esp32s3-bridge/tests/test_protocol.py",
    "wireless/esp32s3-loader/tests/test_payload.py",
    "wireless/test_calcnet.py",
    "tools/check_send.py",
]
BUILDS = ["calc", "calcgpt", "math"]


def run(args, cwd=None, env=None):
    r = subprocess.run(args, cwd=cwd, env=env, capture_output=True, text=True)
    return r.returncode, (r.stdout + r.stderr).strip()


def main():
    failed = []
    if "--build" in sys.argv[1:]:
        env = dict(os.environ)
        cedev = env.get("CEDEV", os.path.expanduser("~/CEdev"))
        env["CEDEV"] = cedev
        env["PATH"] = os.path.join(cedev, "bin") + os.pathsep + env["PATH"]
        for folder in BUILDS:
            code, out = run(["make"], cwd=os.path.join(ROOT, folder), env=env)
            # the linkers missing .note.GNU-stack lines come from CEdevs own library objects, not from this code
            warnings = [ln for ln in out.splitlines() if ("warning:" in ln or "error:" in ln) and "GNU-stack" not in ln]
            ok = code == 0 and not warnings
            print(("ok   " if ok else "FAIL ") + f"make in {folder}" + ("" if ok else "\n" + out))
            if not ok:
                failed.append(f"make {folder}")
    for test in TESTS:
        code, out = run([sys.executable, os.path.join(ROOT, test)])
        last = out.splitlines()[-1] if out else ""
        print(("ok   " if code == 0 else "FAIL ") + f"{test}: {last}")
        if code != 0:
            print(out)
            failed.append(test)
    if failed:
        print(f"\n{len(failed)} FAILED: " + ", ".join(failed))
        return 1
    print("\nall tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
