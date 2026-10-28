"""Checks calckeys.py with a fake injector: nothing here touches the real mouse or keyboard.  Run: python3 test_calckeys.py"""
import sys, time
import calckeys as K

fails = 0


def check(what, got, want):
    global fails
    if got != want:
        print(f"FAIL {what}\n   got  {got}\n   want {want}")
        fails += 1


class FakeScreen:
    width, height = 1000.0, 800.0

    def __init__(self):
        self.pos = [500.0, 400.0]

    def pointer(self):
        return tuple(self.pos)


class FakeInjector:
    def __init__(self):
        self.calls, self.button_down, self.screen = [], False, FakeScreen()

    def move(self, dx, dy):
        self.calls.append(("move", round(dx, 1), round(dy, 1)))

    def button(self, down, right=False, clicks=1):
        self.button_down = down if not right else self.button_down
        self.calls.append(("button", down, right, clicks))

    def click(self, right=False, count=1):
        self.calls.append(("click", right, count))

    def scroll(self, lines):
        self.calls.append(("scroll", lines))

    def key(self, name):
        self.calls.append(("key", name))

    def text(self, ch):
        self.calls.append(("text", ch))


def report(down_keys):
    """The 7-byte keyboard report for a set of key names being held."""
    groups = [0] * 8
    for (g, bit), name in K.KEYS.items():
        if name in down_keys:
            groups[g] |= 1 << bit
    return b"S" + bytes(groups[1:8])


def press(router, *names):
    out = router.feed(report(set(names)))
    router.feed(report(set()))
    return out


# --- parser: every key decodes, and only changes give events
p = K.KeyParser()
check("graph down", p.feed(report({"GRAPH"})), [("GRAPH", True)])
check("same report again: nothing new", p.feed(report({"GRAPH"})), [])
check("graph up", p.feed(report(set())), [("GRAPH", False)])
check("two keys at once", sorted(p.feed(report({"UP", "ENTER"}))), [("ENTER", True), ("UP", True)])
p2 = K.KeyParser()
whole = report({"7"}) + report(set())
check("split across reads", p2.feed(whole[:5]) + p2.feed(whole[5:]), [("7", True), ("7", False)])
check("stray bytes skipped", K.KeyParser().feed(b"xyz" + report({"0"})), [("0", True)])
check("all 49 keys are distinct", len(set(K.KEYS.values())), 49)

# --- view only (no injector): the Graph key does nothing, Clear quits, arrows and Enter become picture commands
r = K.Router()
check("view: up", press(r, "UP"), ["U"])
check("view: enter", press(r, "ENTER"), ["E"])
check("view: graph ignored", press(r, "GRAPH"), [])
check("view: clear", press(r, "CLEAR"), ["Q"])

# --- control: starts in VIEW and acts on nothing until Graph is pressed
inj = FakeInjector()
badges = []
r = K.Router(inj, badges.append)
check("first badge", badges, ["VIEW"])
check("view with control: up is a picture command", press(r, "UP"), ["U"])
check("view with control: nothing sent to the Mac", inj.calls, [])
press(r, "GRAPH")
check("graph -> MOUSE badge", badges[-1], "MOUSE")
check("mouse: arrows are not picture commands", press(r, "UP"), [])
check("mouse: a tap nudges the pointer", inj.calls[-1], ("move", 0, -6))
press(r, "RIGHT")
check("mouse: right nudge", inj.calls[-1], ("move", 6, 0))
inj.calls.clear()
r.feed(report({"ENTER"}))
r.feed(report(set()))
check("mouse: enter presses then releases the left button", inj.calls, [("button", True, False, 1), ("button", False, False, 1)])
inj.calls.clear()
press(r, "YEQU")
press(r, "TRACE")
check("mouse: right click, double click", inj.calls, [("click", True, 1), ("click", False, 2)])
# holding an arrow builds speed
inj.calls.clear()
t = [1000.0]
real_time = time.time
time.time = lambda: t[0]
r.last_tick = t[0]
r.feed(report({"RIGHT"}))
inj.calls.clear()
for _ in range(20):
    t[0] += 0.1
    r.tick()
moves = [c[1] for c in inj.calls if c[0] == "move"]
check("held: nothing moves during the first quarter second (2 ticks of 0.1 s)", len(moves) >= 17, True)
check("held: it speeds up", moves[-1] > moves[3] > 0, True)
r.feed(report(set()))
inj.calls.clear()
r.feed(report({"ZOOM"}))
for _ in range(5):
    t[0] += 0.1
    r.tick()
check("zoom key held scrolls down", all(c == ("scroll", -3) for c in inj.calls) and len(inj.calls) >= 3, True)
r.feed(report(set()))
inj.calls.clear()
r.feed(report({"WINDOW"}))
t[0] += 0.1
r.tick()
check("window key held scrolls up", inj.calls[:1], [("scroll", 3)])
r.feed(report(set()))

# --- typing
press(r, "GRAPH")
check("graph -> TYPE 123 badge", badges[-1], "TYPE 123")
inj.calls.clear()
for k in ("2", "ADD", "3", "DIV", "LPAREN", "RPAREN", "DECPNT", "STO"):
    press(r, k)
check("digits and symbols", "".join(c[1] for c in inj.calls), "2+3/().:")
inj.calls.clear()
press(r, "2ND"); press(r, "1")
press(r, "2ND"); press(r, "ADD")
press(r, "2"); 
check("2nd is one-shot: shifted then normal", [c[1] for c in inj.calls], ["!", "=", "2"])
press(r, "ALPHA")
check("alpha badge", badges[-1], "TYPE ABC")
inj.calls.clear()
for k in ("MATH", "APPS", "PRGM", "0", "SIN"):
    press(r, k)
press(r, "2ND"); press(r, "MATH")
check("letters", "".join(c[1] for c in inj.calls), "abc eA")
inj.calls.clear()
press(r, "ENTER"); press(r, "DEL"); press(r, "LEFT"); press(r, "MODE"); press(r, "WINDOW")
check("special keys", inj.calls, [("key", "RETURN"), ("key", "BACKSPACE"), ("key", "LEFT"), ("key", "TAB"), ("key", "ESCAPE")])
# holding a character repeats, holding Return does not
inj.calls.clear()
r.feed(report({"DEL"}))
for _ in range(12):
    t[0] += 0.1
    r.tick()
n_del = len([c for c in inj.calls if c == ("key", "BACKSPACE")])
r.feed(report(set()))
check("held backspace repeats", n_del >= 5, True)
inj.calls.clear()
r.feed(report({"ENTER"}))
for _ in range(12):
    t[0] += 0.1
    r.tick()
check("held Return does not repeat", inj.calls, [("key", "RETURN")])
r.feed(report(set()))
check("clear still quits while typing", press(r, "CLEAR"), ["Q"])
press(r, "GRAPH")
check("graph -> back to VIEW", badges[-1], "VIEW")
check("view again: up is a picture command", press(r, "UP"), ["U"])
# leaving mouse mode while a drag is held lets go of the button
r2inj = FakeInjector()
r2 = K.Router(r2inj, lambda s: None)
press(r2, "GRAPH")
r2.feed(report({"ENTER"}))
r2inj.button_down = True
r2.feed(report({"ENTER", "GRAPH"}))
check("leaving MOUSE releases a held button", ("button", False, False, 1) in r2inj.calls, True)
time.time = real_time

# --- every printable key typed is a single character the calculator can produce
alltyped = set(K.NORMAL.values()) | set(K.SHIFTED.values()) | set(K.LETTERS.values()) | {c.upper() for c in K.LETTERS.values()}
want = set("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ ") | set("!@#$%^&*()_+-=[]{}|\\`~:;'\",.<>/?")
check("missing characters", sorted(want - alltyped), [])
print(f"{fails} FAILED" if fails else "calckeys: all passed")
sys.exit(1 if fails else 0)
