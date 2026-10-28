"""Reads the calculator's keyboard reports and, if allowed, turns them into mouse and keyboard actions on the Mac.

The calculator (REELS) sends 'S' plus 7 bytes whenever a key goes down or up: its whole keyboard. This module
follows those, and a Router decides what they mean in three modes, cycled with the Graph key:

  VIEW   keys control the picture (arrows, Enter, Clear); nothing is sent to the Mac's own mouse or keyboard
  MOUSE  arrows move the pointer (hold to speed up), Enter clicks (hold + arrows to drag), Y= right click,
         Trace double click, Window / Zoom scroll up / down
  TYPE   number keys type digits and symbols, the Alpha key toggles letters, 2nd is a one-shot shift (capitals, or the
         symbol printed on a key's top row), Del = backspace, Enter = Return, arrows = arrow keys, Mode = Tab,
         Window = Escape

Clear always quits. Mouse and keyboard control only exist when the script is started with --control and macOS has
allowed the app to control the computer (System Settings > Privacy & Security > Accessibility)."""
import ctypes, ctypes.util, time

# (kb_Data group, bit) -> key name, from the calculator's keyboard matrix
KEYS = {
    (1, 0): "GRAPH", (1, 1): "TRACE", (1, 2): "ZOOM", (1, 3): "WINDOW", (1, 4): "YEQU", (1, 5): "2ND", (1, 6): "MODE", (1, 7): "DEL",
    (2, 1): "STO", (2, 2): "LN", (2, 3): "LOG", (2, 4): "SQUARE", (2, 5): "RECIP", (2, 6): "MATH", (2, 7): "ALPHA",
    (3, 0): "0", (3, 1): "1", (3, 2): "4", (3, 3): "7", (3, 4): "COMMA", (3, 5): "SIN", (3, 6): "APPS", (3, 7): "GRAPHVAR",
    (4, 0): "DECPNT", (4, 1): "2", (4, 2): "5", (4, 3): "8", (4, 4): "LPAREN", (4, 5): "COS", (4, 6): "PRGM", (4, 7): "STAT",
    (5, 0): "CHS", (5, 1): "3", (5, 2): "6", (5, 3): "9", (5, 4): "RPAREN", (5, 5): "TAN", (5, 6): "VARS",
    (6, 0): "ENTER", (6, 1): "ADD", (6, 2): "SUB", (6, 3): "MUL", (6, 4): "DIV", (6, 5): "POWER", (6, 6): "CLEAR",
    (7, 0): "DOWN", (7, 1): "LEFT", (7, 2): "RIGHT", (7, 3): "UP",
}

# what each key types: with the Alpha key on (letters), and normally (digits and symbols), plus the 2nd-key versions
LETTERS = {"MATH": "a", "APPS": "b", "PRGM": "c", "RECIP": "d", "SIN": "e", "COS": "f", "TAN": "g", "POWER": "h", "SQUARE": "i",
           "COMMA": "j", "LPAREN": "k", "RPAREN": "l", "DIV": "m", "LOG": "n", "7": "o", "8": "p", "9": "q", "MUL": "r", "LN": "s",
           "4": "t", "5": "u", "6": "v", "SUB": "w", "STO": "x", "1": "y", "2": "z", "0": " ", "DECPNT": ":", "CHS": "?", "ADD": '"',
           "GRAPHVAR": "'", "VARS": ";", "STAT": "!"}
NORMAL = {"0": "0", "1": "1", "2": "2", "3": "3", "4": "4", "5": "5", "6": "6", "7": "7", "8": "8", "9": "9", "DECPNT": ".",
          "COMMA": ",", "CHS": "-", "SUB": "-", "ADD": "+", "MUL": "*", "DIV": "/", "POWER": "^", "LPAREN": "(", "RPAREN": ")",
          "STO": ":", "SIN": "{", "COS": "}", "TAN": "|", "LN": "\\", "LOG": "`", "SQUARE": "[", "RECIP": "]", "MATH": "%",
          "APPS": "@", "PRGM": "#", "VARS": "'", "STAT": '"', "GRAPHVAR": "~"}
SHIFTED = {"1": "!", "2": "@", "3": "#", "4": "$", "5": "%", "6": "^", "7": "&", "8": "*", "9": "(", "0": ")", "DECPNT": ">",
           "COMMA": "<", "CHS": "_", "SUB": "_", "ADD": "=", "MUL": "~", "DIV": "?", "STO": ";", "LPAREN": "[", "RPAREN": "]",
           "POWER": "|"}
# macOS virtual key codes for keys that are not characters
VK = {"RETURN": 0x24, "TAB": 0x30, "BACKSPACE": 0x33, "ESCAPE": 0x35, "LEFT": 0x7B, "RIGHT": 0x7C, "DOWN": 0x7D, "UP": 0x7E}
SPECIAL_KEYS = {"ENTER": "RETURN", "DEL": "BACKSPACE", "MODE": "TAB", "WINDOW": "ESCAPE",
                "LEFT": "LEFT", "RIGHT": "RIGHT", "UP": "UP", "DOWN": "DOWN"}
VIEW_LETTERS = {"UP": "U", "DOWN": "D", "LEFT": "L", "RIGHT": "R", "ENTER": "E", "CLEAR": "Q"}


class KeyParser:
    """Turns the byte stream from the calculator into (key name, is down) events."""
    def __init__(self):
        self.buf = b""
        self.state = [0] * 8  # indexed by group 1..7

    def feed(self, data):
        self.buf += data
        events = []
        while self.buf:
            if self.buf[0:1] != b"S":
                self.buf = self.buf[1:]  # not a key report: skip it
                continue
            if len(self.buf) < 8:
                break
            report, self.buf = self.buf[1:8], self.buf[8:]
            for g in range(1, 8):
                old, new = self.state[g], report[g - 1]
                if old != new:
                    for bit in range(8):
                        name = KEYS.get((g, bit))
                        if name and (old ^ new) & (1 << bit):
                            events.append((name, bool(new & (1 << bit))))
                    self.state[g] = new
        return events


class CGPoint(ctypes.Structure):
    _fields_ = [("x", ctypes.c_double), ("y", ctypes.c_double)]


class Injector:
    """Moves the pointer, clicks, scrolls and types on the Mac through CoreGraphics. Needs Accessibility permission."""
    def __init__(self, screen):
        self.screen = screen
        cg = self.cg = ctypes.CDLL(ctypes.util.find_library("CoreGraphics"))
        self.cf = ctypes.CDLL(ctypes.util.find_library("CoreFoundation"))
        cg.CGEventCreateMouseEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint32, CGPoint, ctypes.c_uint32]
        cg.CGEventCreateMouseEvent.restype = ctypes.c_void_p
        cg.CGEventPost.argtypes = [ctypes.c_uint32, ctypes.c_void_p]
        cg.CGEventSetIntegerValueField.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int64]
        cg.CGEventCreateKeyboardEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint16, ctypes.c_bool]
        cg.CGEventCreateKeyboardEvent.restype = ctypes.c_void_p
        cg.CGEventKeyboardSetUnicodeString.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_uint16)]
        cg.CGEventCreateScrollWheelEvent2.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32,
                                                      ctypes.c_int32, ctypes.c_int32, ctypes.c_int32]
        cg.CGEventCreateScrollWheelEvent2.restype = ctypes.c_void_p
        self.cf.CFRelease.argtypes = [ctypes.c_void_p]
        self.button_down = False

    def trusted(self):
        """Has macOS allowed this app to control the computer?"""
        ax = ctypes.CDLL(ctypes.util.find_library("ApplicationServices"))
        ax.AXIsProcessTrusted.restype = ctypes.c_bool
        return bool(ax.AXIsProcessTrusted())

    def _post(self, event):
        if event:
            self.cg.CGEventPost(0, event)  # 0 = the hardware event stream
            self.cf.CFRelease(event)

    def _mouse(self, kind, x, y, button=0, clicks=None):
        ev = self.cg.CGEventCreateMouseEvent(None, kind, CGPoint(x, y), button)
        if ev and clicks:
            self.cg.CGEventSetIntegerValueField(ev, 1, clicks)  # click count: 2 makes a double click
        self._post(ev)

    def move(self, dx, dy):
        x, y = self.screen.pointer()
        x = min(max(x + dx, 0), self.screen.width - 1)
        y = min(max(y + dy, 0), self.screen.height - 1)
        self._mouse(6 if self.button_down else 5, x, y)  # 6 = dragging with the button held, 5 = just moving

    def button(self, down, right=False, clicks=1):
        x, y = self.screen.pointer()
        if right:
            self._mouse(3 if down else 4, x, y, 1, clicks)
        else:
            self._mouse(1 if down else 2, x, y, 0, clicks)
            self.button_down = down

    def click(self, right=False, count=1):
        for n in range(1, count + 1):
            self.button(True, right, n)
            self.button(False, right, n)

    def scroll(self, lines):
        self._post(self.cg.CGEventCreateScrollWheelEvent2(None, 1, 1, lines, 0, 0))

    def key(self, name):
        for down in (True, False):
            self._post(self.cg.CGEventCreateKeyboardEvent(None, VK[name], down))

    def text(self, ch):
        units = (ctypes.c_uint16 * 1)(ord(ch))
        for down in (True, False):
            ev = self.cg.CGEventCreateKeyboardEvent(None, 0, down)
            if ev:
                self.cg.CGEventKeyboardSetUnicodeString(ev, 1, units)
            self._post(ev)


class Router:
    """Decides what key events mean. feed() returns picture commands (U D L R E Q) for the script; tick() must be called often."""
    def __init__(self, injector=None, send_badge=None):
        self.inj = injector  # None: view-only, the Graph key does nothing
        self.send_badge = send_badge or (lambda text: None)
        self.parser = KeyParser()
        self.mode = "VIEW"
        self.alpha = False
        self.shift = False
        self.down = {}       # key name -> time it went down
        self.repeat = None   # (action, next time) for a held typing key
        self.last_tick = time.time()
        self.scroll_next = 0.0
        if injector:
            self.announce()

    def badge_text(self):
        if self.mode == "TYPE":
            return "TYPE ABC" if self.alpha else "TYPE 123"
        return self.mode

    def announce(self):
        self.send_badge(self.badge_text() + (" ^" if self.shift and self.mode == "TYPE" else ""))

    def feed(self, data):
        commands = []
        for name, is_down in self.parser.feed(data):
            if is_down:
                self.down[name] = time.time()
            else:
                self.down.pop(name, None)
            if name == "CLEAR" and is_down:
                commands.append("Q")  # Clear always quits
            elif self.inj and name == "GRAPH" and is_down:
                self.cycle()
            elif self.mode == "VIEW" or not self.inj:
                if is_down and name in VIEW_LETTERS:
                    commands.append(VIEW_LETTERS[name])
            elif self.mode == "MOUSE":
                self.mouse_key(name, is_down)
            else:
                self.type_key(name, is_down)
        return commands

    def cycle(self):
        self.release_all()
        self.mode = {"VIEW": "MOUSE", "MOUSE": "TYPE", "TYPE": "VIEW"}[self.mode]
        self.alpha = self.shift = False
        self.announce()

    def close(self):
        """Lets go of anything held, so a quit never leaves the mouse button pressed."""
        self.release_all()

    def release_all(self):
        if self.inj and self.inj.button_down:
            self.inj.button(False)
        self.repeat = None

    # ---- mouse ----
    def mouse_key(self, name, is_down):
        if name == "ENTER":
            self.inj.button(is_down)
        elif not is_down:
            return
        elif name == "YEQU":
            self.inj.click(right=True)
        elif name == "TRACE":
            self.inj.click(count=2)
        elif name in ("UP", "DOWN", "LEFT", "RIGHT"):
            step = 6  # one tap nudges a little; holding builds up speed in tick()
            self.inj.move({"LEFT": -step, "RIGHT": step}.get(name, 0), {"UP": -step, "DOWN": step}.get(name, 0))

    # ---- typing ----
    def type_key(self, name, is_down):
        if not is_down:
            if self.repeat and self.repeat[0][1] == name:
                self.repeat = None
            return
        if name == "ALPHA":
            self.alpha = not self.alpha
            self.announce()
            return
        if name == "2ND":
            self.shift = not self.shift
            self.announce()
            return
        action = None
        if name in SPECIAL_KEYS:
            action = ("key", SPECIAL_KEYS[name])
        else:
            ch = (LETTERS.get(name) if self.alpha else None) or (SHIFTED.get(name) if self.shift else None) or NORMAL.get(name)
            if ch and self.alpha and self.shift and ch.isalpha():
                ch = ch.upper()
            if ch:
                action = ("text", ch)
        if not action:
            return
        self.do(action)
        if action not in (("key", "RETURN"), ("key", "TAB"), ("key", "ESCAPE")):  # these never repeat while held
            self.repeat = ((action, name), time.time() + 0.45)
        if self.shift:
            self.shift = False  # the 2nd key applies to one key press only
            self.announce()

    def do(self, action):
        kind, value = action
        if kind == "key":
            self.inj.key(value)
        else:
            self.inj.text(value)

    # ---- called every few milliseconds ----
    def tick(self):
        now = time.time()
        dt, self.last_tick = now - self.last_tick, now
        if not self.inj:
            return
        if self.mode == "MOUSE":
            vx = (("RIGHT" in self.down) - ("LEFT" in self.down))
            vy = (("DOWN" in self.down) - ("UP" in self.down))
            if vx or vy:
                held = now - max(self.down.get(k, 0) for k in ("UP", "DOWN", "LEFT", "RIGHT") if k in self.down)
                if held > 0.25:
                    speed = 150 + 900 * min(1.0, (held - 0.25) / 1.5)  # screen points per second
                    norm = (vx * vx + vy * vy) ** 0.5
                    self.inj.move(vx / norm * speed * dt, vy / norm * speed * dt)
            scroll = ("ZOOM" in self.down) - ("WINDOW" in self.down)
            if scroll and now >= self.scroll_next:
                self.inj.scroll(-3 * scroll)
                self.scroll_next = now + 0.07
        elif self.mode == "TYPE" and self.repeat and now >= self.repeat[1]:
            self.do(self.repeat[0][0])
            self.repeat = (self.repeat[0], now + 0.07)
