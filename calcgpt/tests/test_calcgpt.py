"""Checks the calculator-side chat code on this computer.  Run: python3 test_calcgpt.py  (needs clang)
 1. the chat store unit test
 2. real replies from calcgptd.py, fed to the chat store in random pieces, give the same wrapped rows as an independent reference
 3. the typing table copy is identical to the browser's (one source: reels/mac/calckeys.py)"""
import filecmp, os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "mac"))
import calcgptd as G

SRC = os.path.join(HERE, "..", "src")
tmp = tempfile.mkdtemp()
fails = 0


def check(what, ok, detail=""):
    global fails
    if not ok:
        print("FAIL", what, detail)
        fails += 1


def build(name, *files):
    exe = os.path.join(tmp, name)
    r = subprocess.run(["clang", "-Wall", "-Wextra", "-o", exe] + list(files), capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"could not build {name}:\n{r.stderr}")
    return exe


unit = build("chatmodel_test", os.path.join(HERE, "chatmodel_test.c"), os.path.join(SRC, "chatmodel.c"))
r = subprocess.run([unit], capture_output=True, text=True)
check("chat store unit test", r.returncode == 0 and "all passed" in r.stdout, r.stdout)

dump = build("chat_dump", os.path.join(HERE, "chat_dump.c"), os.path.join(SRC, "chatmodel.c"))


def ref_rows(role, text):
    """The wrapped rows of one message, worked out independently: a row is a slice of the text from the first to the last word on it."""
    width = 36 if role == "U" else 38
    rows = []
    for para in text.split("\n"):
        if para == "":
            rows.append("")
            continue
        words, i = [], 0
        while i < len(para):
            if para[i] == " ":
                i += 1
                continue
            j = i
            while j < len(para) and para[j] != " ":
                j += 1
            words.append((i, j))
            i = j
        start = None
        end = None
        for a, b in words:
            while b - a > width:
                if start is not None:
                    rows.append(para[start:end]); start = end = None
                rows.append(para[a:a + width]); a += width
            if start is None:
                start, end = a, b
            elif b - start <= width:
                end = b
            else:
                rows.append(para[start:end]); start, end = a, b
        if start is not None:
            rows.append(para[start:end])
    return rows


ANSWERS = [
    ["The sine of 30 degrees is exactly 1/2.\n", "In a 30-60-90 triangle the short side is half the hypotenuse, ", "so sin 30 = 1/2."],
    ["<think>plan the answer</think>**Law of sines**: a/sin A = b/sin B = c/sin C.\n\n", "Use it when you know two angles and a side, ", "or two sides and an angle opposite one of them."],
    ["Sure!\n\n## Steps\n* Add 3 to both sides\n* Divide by 2\nx = 4\n", "word " * 60],
    ["supercalifragilisticexpialidocious" * 3 + " end"],
]
QUESTIONS = ["what is sin 30", "explain the law of sines please and make it quite long so that it wraps around the screen", "solve 2x + 3 = 11", "x"]
for chunk in (1, 3, 16, 5000):
    lines, want = [], []
    for q, pieces in zip(QUESTIONS, ANSWERS):
        chat = G.Chat(stream=lambda body, p=pieces: iter(p), busy=lambda: False)
        wire = b"".join(chat.reply(q))
        lines.append("U " + q)
        lines.append("F " + wire.hex())
        # what the model's text turns into on the screen
        cleaned = G.Cleaner()
        text = "".join(cleaned.feed(p) for p in pieces) + cleaned.flush()
        want += [("U", row) for row in ref_rows("U", q)] + [("A", row) for row in ref_rows("A", text.lstrip())]
    sp = os.path.join(tmp, "script.txt")
    open(sp, "w").write("\n".join(lines) + "\n")
    out = subprocess.run([dump, sp, str(chunk)], capture_output=True, text=True).stdout.splitlines()
    got = [(ln[4], ln[8:]) for ln in out if ln.startswith("ROW ")]
    check(f"rows match the reference (chunk {chunk})", got == want, f"{len(got)} rows vs {len(want)}; first difference: "
          + str(next(((g, w) for g, w in zip(got, want) if g != w), None)))
    check(f"not busy and no status at the end (chunk {chunk})", "BUSY 0" in out and "STATUS " in out)
    check(f"every row fits the screen (chunk {chunk})", all(len(t) <= (36 if r == "U" else 38) for r, t in got))

check("typing table is the same as the browser's",
      filecmp.cmp(os.path.join(SRC, "typing_tables.h"), os.path.join(HERE, "..", "..", "browser", "src", "typing_tables.h"), shallow=False)
      if os.path.exists(os.path.join(HERE, "..", "..", "browser", "src", "typing_tables.h")) else True)
check("typing code is the same as the browser's",
      all(filecmp.cmp(os.path.join(SRC, f), os.path.join(HERE, "..", "..", "browser", "src", f), shallow=False) for f in ("typing.c", "typing.h"))
      if os.path.exists(os.path.join(HERE, "..", "..", "browser", "src", "typing.c")) else True)
print(f"{fails} FAILED" if fails else "calcgpt (calculator side): all passed")
sys.exit(1 if fails else 0)
