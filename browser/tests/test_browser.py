"""Checks the calculator-side C code of the browser on this computer.  Run: python3 test_browser.py  (needs clang)
 1. page model unit test (scrolling, link selection, limits)
 2. the page model reads exactly what browserd.py sends: compared with an independent decoder
 3. the typing layout on the calculator equals the Mac-side layout (calckeys.py)"""
import os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "mac"))
sys.path.insert(0, os.path.join(HERE, "..", "..", "reels", "mac"))
import browserd as B
import calckeys as K
from wire_decode import decode

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


# 1
unit = build("pagemodel_test", os.path.join(HERE, "pagemodel_test.c"), os.path.join(SRC, "pagemodel.c"))
r = subprocess.run([unit], capture_output=True, text=True)
check("pagemodel unit test", r.returncode == 0 and "all passed" in r.stdout, r.stdout)

# 2
dump = build("pagemodel_dump", os.path.join(HERE, "pagemodel_dump.c"), os.path.join(SRC, "pagemodel.c"))
LONG = "<body>" + "".join(f"<p>paragraph {i} with a <a href='/l{i}'>link number {i}</a> and more words to wrap around</p>" for i in range(60)) + "</body>"
pages = [B.reader_page("https://site.test/x", "<title>Sine</title><h1>Head</h1><p>Hi <a href='/a'>there you</a> and <a href='/b'>b</a></p>"),
         B.reader_page("https://site.test/x", LONG),
         B.results_page("sine", [("Law of sines", "https://en.wikipedia.org/wiki/Law_of_sines", "In trigonometry the law of sines is an equation relating lengths of sides to angles."),
                                 ("Another result with a rather long title that wraps", "https://example.org/a/b/c/d/e/f/g/h", "More text.")]),
         B.message_page("Could not open that page: could not connect.")]
for chunk in (1, 5, 64, 100000):
    for page in pages:
        stream = B.encode_status("Loading...") + B.encode_page(page)
        sp = os.path.join(tmp, "s.bin")
        open(sp, "wb").write(stream)
        out = subprocess.run([dump, sp, str(chunk)], capture_output=True, text=True).stdout.splitlines()
        want_rows = [f"ROW {s} {t}" + "".join(f" [{i} {a} {b}]" for i, a, b in spans) for s, t, spans in page.rows]
        got_rows = [ln for ln in out if ln.startswith("ROW ")]
        check(f"rows equal (chunk {chunk}, {page.title[:12]})", got_rows == want_rows, f"{len(got_rows)} vs {len(want_rows)}")
        check("title equal", f"TITLE {B.ascii_text(page.title)[:38]}" in out)
        check("screen kind", f"SCREEN {dict(R=1, P=2, M=3)[page.kind]}" in out)
        check("loading cleared by the end of the page", "LOADING 0" in out and "STATUS " in out)
    home = B.encode_home([("Wikipedia", "u"), ("Desmos", "v"), ("A name that is long enough to cut", "w")])
    open(sp, "wb").write(home)
    out = subprocess.run([dump, sp, str(chunk)], capture_output=True, text=True).stdout.splitlines()
    check("home bookmarks", "BOOKMARKS 3|Wikipedia|Desmos|A name that is long enou" in out, out[4] if len(out) > 4 else out)

# 3
typ = build("typing_host_test", os.path.join(HERE, "typing_host_test.c"), os.path.join(SRC, "typing.c"))
cases, want = [], []
for (g, bit), name in K.KEYS.items():
    for alpha in (0, 1):
        for shift in (0, 1):
            cases.append(f"{g * 8 + bit} {alpha} {shift}")
            if name == "ALPHA":
                want.append(f"layer {1 - alpha} {shift}")
            elif name == "2ND":
                want.append(f"layer {alpha} {1 - shift}")
            else:
                ch = (K.LETTERS.get(name) if alpha else None) or (K.SHIFTED.get(name) if shift else None) or K.NORMAL.get(name)
                if ch and alpha and shift and ch.isalpha():
                    ch = ch.upper()
                want.append(f"{ord(ch)} {alpha} 0" if ch else f"none {alpha} {shift}")
got = subprocess.run([typ], input="\n".join(cases), capture_output=True, text=True).stdout.splitlines()
bad = [(c, g, w) for c, g, w in zip(cases, got, want) if g != w]
check("typing matches calckeys.py for every key and layer", not bad and len(got) == len(want), bad[:3])
print(f"{fails} FAILED" if fails else "browser (calculator side): all passed")
sys.exit(1 if fails else 0)
