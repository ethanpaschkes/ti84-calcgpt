"""Checks that what the Mac script sends is drawn correctly by the calculator's own viewer code.
Run: python3 test_viewer.py   (needs clang). Builds the viewer for the computer, replays streams, compares screen memory."""
import os, random, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "mac"))
import screenshare as S

fails = 0


def check(what, ok, detail=""):
    global fails
    if not ok:
        print("FAIL", what, detail)
        fails += 1


def expected_screen(mode, packed):
    """What the screen should hold for a packed picture, worked out independently of the C code."""
    out = bytearray()
    if mode == "mono":
        for b in packed:
            out += bytes((b >> (7 - i)) & 1 for i in range(8))
    elif mode == "gray":
        for b in packed:
            out += bytes((b >> (6 - 2 * i)) & 3 for i in range(4))
    else:
        for y in range(120):
            row = bytearray()
            for b in packed[y * 80:(y + 1) * 80]:
                row += bytes((b >> 4, b >> 4, b & 15, b & 15))
            out += row + row
    return bytes(out)


def record(data):
    return len(data).to_bytes(4, "little") + data


def make_pictures(mode, rng):
    size = S.ROW_BYTES[mode] * S.ROW_COUNT[mode]
    rb, n = S.ROW_BYTES[mode], S.ROW_COUNT[mode]
    base = bytearray(rng.randrange(256) for _ in range(size))
    pics = [bytes(base)]
    def edit(rows):
        for y in rows:
            base[y * rb:(y + 1) * rb] = bytes(rng.randrange(256) for _ in range(rb))
        pics.append(bytes(base))
    edit([0])                                  # first row only
    edit([n - 1])                              # last row only
    edit(range(10, 30))                        # a band
    edit([5, 6, 50, 51, 52, 100, 101])         # several separate runs
    pics.append(bytes(base))                   # nothing changed
    edit(range(n))                             # every row
    edit(rng.sample(range(n), n // 3))         # a third of the rows, scattered
    edit([rng.randrange(n)])
    return pics


# build the calculator viewer for this computer
tmp = tempfile.mkdtemp()
exe = os.path.join(tmp, "viewer_host_test")
r = subprocess.run(["clang", "-Wall", "-Wextra", "-o", exe, os.path.join(HERE, "viewer_host_test.c"),
                    os.path.join(HERE, "..", "src", "viewer.c")], capture_output=True, text=True)
if r.returncode != 0:
    sys.exit("could not build the viewer:\n" + r.stderr)

for mode in ("mono", "gray", "color"):
    for chunk in (1, 7, 64, 4096):
        rng = random.Random(7)
        pics = make_pictures(mode, rng)
        records, wants, prev, sent_bytes, full_bytes = [], [], None, 0, 0
        palette = bytes(range(48))
        records.append(record(b"P" + palette)); wants.append(None)
        records.append(record(b"I" + bytes([5]) + b"MOUSE")); wants.append(None)
        for pic in pics:
            msg = S.picture_messages(mode, prev, pic)
            full = S.FULL_LETTER[mode] + pic
            check(f"{mode}: never longer than the whole picture", len(msg) <= len(full))
            records.append(record(msg))
            wants.append(expected_screen(mode, pic))
            sent_bytes += len(msg)
            full_bytes += len(full)
            prev = pic
        records.append(record(b"I" + bytes([0]))); wants.append(None)
        sp, dp = os.path.join(tmp, "stream.bin"), os.path.join(tmp, "dumps.bin")
        open(sp, "wb").write(b"".join(records))
        out = subprocess.run([exe, sp, dp, str(chunk)], capture_output=True, text=True)
        check(f"{mode}/{chunk}: viewer ran", out.returncode == 0, out.stderr)
        dumps = open(dp, "rb").read()
        screen = 320 * 240
        check(f"{mode}/{chunk}: one dump per record", len(dumps) == screen * len(records), f"{len(dumps)}")
        for i, want in enumerate(wants):
            if want is not None:
                got = dumps[i * screen:(i + 1) * screen]
                check(f"{mode}/{chunk}: screen after record {i}", got == want,
                      f"{sum(1 for a, b in zip(got, want) if a != b)} pixels differ")
        lines = out.stdout.splitlines()
        check(f"{mode}/{chunk}: palette received", f"0 PAL {palette.hex()}" in lines)
        check(f"{mode}/{chunk}: badge shown with the pictures", "1 BADGE MOUSE" in lines and f"2 BADGE MOUSE" in lines)
        check(f"{mode}/{chunk}: badge hidden", lines[-1].endswith("BADGE ") or "BADGE " == lines[-1][-6:])
        if chunk == 64:
            print(f"{mode}: sent {sent_bytes} bytes instead of {full_bytes} ({100 * sent_bytes // full_bytes}%) over {len(pics)} pictures")

# a corrupt or oversized row update must not crash or draw outside the screen
bad = b"m" + bytes((250, 30)) + bytes(1200) + b"g" + bytes((0, 0)) + b"F" + bytes(9600)
sp, dp = os.path.join(tmp, "bad.bin"), os.path.join(tmp, "baddumps.bin")
open(sp, "wb").write(record(bad))
out = subprocess.run([exe, sp, dp, "64"], capture_output=True, text=True)
check("out-of-range rows are ignored without crashing", out.returncode == 0)
print(f"{fails} FAILED" if fails else "viewer: all passed")
sys.exit(1 if fails else 0)
