"""Fails when a program in send/ is not the build in its bin/ folder.

2026-10-29 bug (a): send/CALCGPT.8xp was an old build while calcgpt/bin/CALCGPT.8xp was the new one, and an hour went into a
"dead" [mode] key that the new build had fixed. Every program folder with bin/NAME.8xp is checked against send/NAME.8xp.
Run: python3 tools/check_send.py   (also run by tools/test_all.py).  --fix copies the bin/ builds over to send/."""
import filecmp, glob, os, shutil, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SEND = os.path.join(ROOT, "send")


def pairs():
    """(name, bin path, send path) for every built program"""
    out = []
    for built in sorted(glob.glob(os.path.join(ROOT, "*", "bin", "*.8xp"))):
        name = os.path.basename(built)
        out.append((name, built, os.path.join(SEND, name)))
    return out


def main(fix):
    stale, missing = [], []
    for name, built, sent in pairs():
        if not os.path.exists(sent):
            missing.append(name)
        elif not filecmp.cmp(built, sent, shallow=False):
            stale.append(name)
    if fix:
        for name, built, sent in pairs():
            if name in stale or name in missing:
                shutil.copyfile(built, sent)
                print(f"copied {os.path.relpath(built, ROOT)} -> send/{name}")
        return 0
    for name in stale:
        print(f"STALE  send/{name} differs from its bin/ build (run: python3 tools/check_send.py --fix)")
    for name in missing:
        print(f"note   send/{name} does not exist yet (bin/ has a build)")
    if stale:
        return 1
    print(f"send/ is up to date ({len(pairs()) - len(missing)} programs checked)")
    return 0


if __name__ == "__main__":
    sys.exit(main("--fix" in sys.argv[1:]))
