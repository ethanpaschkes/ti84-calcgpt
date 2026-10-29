"""Tests for the CalcGPT cloud server with a fake cloud model: no OpenAI key, no money spent, no board needed.
Run: python3 cloud/test_server.py"""
import asyncio, hmac, json, os, re, socket, stat, struct, subprocess, sys, tempfile, threading, time
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import server as S
import add_device as A
S.log = lambda *parts: None  # the server's log lines are not needed here
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "calcgpt", "mac"))
from wire_decode import decode

fails = 0


def check(what, got, want):
    global fails
    if got != want:
        print(f"FAIL {what}\n   got  {got!r}\n   want {want!r}")
        fails += 1


tmp = tempfile.mkdtemp()
devpath = os.path.join(tmp, "devices.json")

# ---- adding devices
dev_id, secret = A.new_device(devpath, "Ethan")
check("a device id is 16 characters a-z 0-9", bool(re.fullmatch(r"[a-z0-9]{16}", dev_id)), True)
check("a device secret is 32 bytes of hex", bool(re.fullmatch(r"[0-9a-f]{64}", secret)), True)
check("the devices file is private", stat.S_IMODE(os.stat(devpath).st_mode), 0o600)
saved = json.load(open(devpath))[dev_id]
check("the device is saved on, with the default limit", (saved["name"], saved["enabled"], saved["daily_limit"]), ("Ethan", True, 200))
other_id, other_secret = A.new_device(devpath, "Maya")
limited_id, limited_secret = A.new_device(devpath, "Limited", limit=2)
off_id, off_secret = A.new_device(devpath, "Lost board")
A.set_enabled(devpath, off_id, False)
check("--disable switches it off", json.load(open(devpath))[off_id]["enabled"], False)
check("the first device is kept", json.load(open(devpath))[dev_id]["secret"] == secret, True)
check("the file stays private after changes", stat.S_IMODE(os.stat(devpath).st_mode), 0o600)
run = subprocess.run([sys.executable, os.path.join(HERE, "add_device.py"), "Cli board", "--limit", "7", "--devices", devpath],
                     capture_output=True, text=True)
lines = run.stdout.splitlines()
check("the command prints only the two lines the board needs",
      (len(lines), lines[0].startswith("DEVICE_ID="), lines[1].startswith("DEVICE_SECRET=")) if len(lines) == 2 else len(lines), (2, True, True))
check("...with a reminder that they are secret", "secret" in run.stderr.lower(), True)
cli_id = lines[0].split("=", 1)[1] if lines else ""
check("--limit is saved", json.load(open(devpath)).get(cli_id, {}).get("daily_limit"), 7)
run = subprocess.run([sys.executable, os.path.join(HERE, "add_device.py"), "--list", "--devices", devpath], capture_output=True, text=True)
check("--list never shows a secret", secret in run.stdout or other_secret in run.stdout, False)
check("--list shows the names", "Maya" in run.stdout and "Lost board" in run.stdout, True)

# ---- the per-address login limit
t = S.Throttle()
for i in range(5):
    t.fail("1.2.3.4", now=100 + i)
check("5 failures in a minute block that address", t.blocked("1.2.3.4", now=105), True)
check("...but not another address", t.blocked("5.6.7.8", now=105), False)
check("...until things go quiet", t.blocked("1.2.3.4", now=170), False)

# ---- only cloud models
mpath = os.path.join(tmp, "models.json")
json.dump([{"label": "Local", "provider": "ollama", "model": "qwen"}, {"label": "Fast", "provider": "openai", "model": "gpt-x"},
           {"label": "Smart", "provider": "openai", "model": "gpt-y"}], open(mpath, "w"))
models, notes = S.cloud_models(mpath)
check("an ollama entry is skipped", [m["label"] for m in models], ["Fast", "Smart"])
check("...and named in the notes", any("Local" in n for n in notes), True)

# ---- the PROXY protocol header
def proxy_addr(data):
    async def go():
        r = asyncio.StreamReader()
        r.feed_data(data)
        r.feed_eof()
        return await S.read_proxy_header(r)
    return asyncio.run(go())
check("PROXY v1", proxy_addr(b"PROXY TCP4 203.0.113.9 10.0.0.1 5555 7979\r\n"), "203.0.113.9")
v2 = S.PROXY_V2 + bytes([0x21, 0x11]) + struct.pack("!H", 12) + bytes([198, 51, 100, 7, 10, 0, 0, 1]) + struct.pack("!HH", 5555, 7979)
check("PROXY v2", proxy_addr(v2), "198.51.100.7")
check("no header", proxy_addr(b"CALCNET-garbage-here"), None)
check("the key is the secret's bytes, as the board's firmware uses it", S.answer_for("ab" * 32, b"C" * 16),
      hmac.new(bytes([0xab] * 32), b"C" * 16 + b"calc", "sha256").digest())


# ---- a running server with a fake cloud model
barrier = threading.Barrier(2, timeout=5)
seen_keys = []


def fake_cloud(entry, key, messages):
    seen_keys.append(key)
    q = messages[-1]["content"]
    if q == "slow":
        for i in range(200):
            time.sleep(0.05)
            yield f"word{i} "
    elif q.startswith("pair"):
        barrier.wait()  # both boards must be answering at the same moment to get past here
        yield f"answer for {q}"
    else:
        yield "The sine of 30 "
        yield f"degrees is 1/2 ({entry['model']})."


loop = asyncio.new_event_loop()
threading.Thread(target=loop.run_forever, daemon=True).start()


def start(server):
    ready, box = threading.Event(), []
    asyncio.run_coroutine_threadsafe(S.serve(server, "127.0.0.1", 0, ready=lambda p: (box.append(p), ready.set())), loop)
    ready.wait(5)
    return box[0]


devices = S.Devices(devpath)
main = S.Server(devices, models, "sk-test", cloud_stream=fake_cloud, throttle=S.Throttle(limit=1000), login_timeout=1.0)
port = start(main)


def recv_exact(s, n):
    data = b""
    while len(data) < n:
        chunk = s.recv(n - len(data))
        if not chunk:
            raise ConnectionError("closed")
        data += chunk
    return data


def connect(device_id, key, p=None, proxy=b""):
    s = socket.create_connection(("127.0.0.1", p or port), timeout=5)
    s.sendall(proxy)
    hello = recv_exact(s, 24)
    s.sendall(device_id.encode().ljust(16, b"\0") + S.answer_for(key, hello[8:]))
    return s, hello


def closed(s):
    """True if the server closes the connection without saying anything more."""
    try:
        return s.recv(10) == b""
    except (ConnectionError, socket.timeout):
        return False
    except OSError:
        return True


class Board:
    """A test board: logs in, sends calculator messages and collects the server's messages."""
    def __init__(self, device_id, key):
        self.s, self.hello = connect(device_id, key)
        self.ok = recv_exact(self.s, 2)
        self.buf = b""

    def send(self, data):
        self.s.sendall(data)

    def ask(self, q):
        self.send(b"Q" + bytes([len(q)]) + q.encode())

    def until(self, tag, timeout=5):
        """Messages up to and including the first one with this tag."""
        end, msgs = time.time() + timeout, []
        while time.time() < end:
            while self.buf:
                c, n = chr(self.buf[0]), None
                if c in "ATL":
                    n = 2 + self.buf[1] if len(self.buf) >= 2 else None
                elif c in "DCK":
                    n = 1
                elif c == "U":
                    n = 2
                else:
                    raise ValueError(f"unknown message {c!r}")
                if n is None or len(self.buf) < n:
                    break
                msgs += decode(self.buf[:n])
                self.buf = self.buf[n:]
                if msgs[-1][0] == tag:
                    return msgs
            self.s.settimeout(max(0.01, end - time.time()))
            try:
                chunk = self.s.recv(4096)
            except socket.timeout:
                break
            if not chunk:
                break
            self.buf += chunk
        return msgs + [("timeout",)]


def text(msgs):
    return "".join(m[1] for m in msgs if m[0] == "A")


b = Board(dev_id, secret)
check("hello is CALCNET2 and a 16-byte challenge", (b.hello[:8], len(b.hello)), (b"CALCNET2", 24))
check("a good login gets OK", b.ok, b"OK")
check("then the model listing", b.until("U"), [("K",), ("L", "Fast"), ("L", "Smart"), ("U", 0)])
b.ask("sin 30?")
msgs = b.until("D")
check("a question: Thinking, cleared, the answer, then D", [m[0] for m in msgs][:2] + [m[0] for m in msgs][-1:], ["T", "T", "D"])
check("the answer arrives whole", text(msgs), "The sine of 30 degrees is 1/2 (gpt-x).")
check("the server's key is used", seen_keys[-1], "sk-test")
b.send(b"M\x01")
check("M picks a model", b.until("U")[-1], ("U", 1))
b.ask("again")
check("...and the answer comes from it", text(b.until("D")).endswith("(gpt-y)."), True)
b.send(b"R")
check("R sends the listing again", b.until("U"), [("K",), ("L", "Fast"), ("L", "Smart"), ("U", 1)])

# S stops an answer
b.ask("slow")
first = b.until("A")
b.send(b"S")
t0 = time.time()
rest = b.until("D")
check("S: the answer ends with D", rest[-1], ("D",))
check("S: it ends quickly", time.time() - t0 < 2, True)
check("S: only part of the answer was sent", len((text(first) + text(rest)).split()) < 50, True)
b.send(b"N")
b.ask("sin 30?")
check("after S and N the next question is answered", text(b.until("D")), "The sine of 30 degrees is 1/2 (gpt-y).")
b.send(b"Z")
check("Z ends the connection", closed(b.s), True)

# two boards at once
b1, b2 = Board(dev_id, secret), Board(other_id, other_secret)
b1.until("U"); b2.until("U")
b1.ask("pair one"); b2.ask("pair two")
r1, r2 = b1.until("D", 8), b2.until("D", 8)
check("two boards answered at the same time: first", text(r1), "answer for pair one")
check("two boards answered at the same time: second", text(r2), "answer for pair two")

# a new login for the same board replaces the old connection
b3 = Board(dev_id, secret)
b3.until("U")
check("the older connection of that board is closed", closed(b1.s), True)
check("the new one works", (b3.ask("sin 30?"), text(b3.until("D")))[1].startswith("The sine"), True)
b2.s.close(); b3.s.close()

# refused logins
s, _ = connect(dev_id, "0" * 64)
check("a wrong secret is closed", closed(s), True)
s, _ = connect("nosuchdevice0000", secret)
check("an unknown id is closed", closed(s), True)
s, _ = connect(off_id, off_secret)
check("a switched-off device is closed", closed(s), True)
s = socket.create_connection(("127.0.0.1", port), timeout=5)
recv_exact(s, 24)
check("silence is closed after the time limit", closed(s), True)

# the daily limit
bl = Board(limited_id, limited_secret)
bl.until("U")
for i in range(2):
    bl.ask("sin 30?")
    check(f"question {i + 1} of 2 is answered", text(bl.until("D")).startswith("The sine"), True)
bl.ask("sin 30?")
check("then: Daily limit reached + D", bl.until("D"), [("T", "Daily limit reached"), ("D",)])
usage = json.load(open(os.path.join(tmp, "usage.json")))
check("usage is saved per device per UTC day", (usage["day"], usage["counts"][limited_id]), (S.utc_day(), 2))
check("the usage file is private", stat.S_IMODE(os.stat(os.path.join(tmp, "usage.json")).st_mode), 0o600)
check("a new UTC day starts at 0", devices.used(limited_id, now=time.time() + 86400), 0)
check("usage survives a restart", S.Devices(devpath).used(limited_id), 2)
bl.s.close()

# switching a board off while it is connected
bc = Board(cli_id, lines[1].split("=", 1)[1] if len(lines) == 2 else "")
bc.until("U")
A.set_enabled(devpath, cli_id, False)
time.sleep(0.05)
bc.ask("sin 30?")
check("a board switched off while connected is closed at its next question", closed(bc.s), True)

# the login limit on a server of its own (5 failures, then no hello at all)
guarded = S.Server(S.Devices(devpath), models, "sk-test", cloud_stream=fake_cloud, login_timeout=1.0)
gport = start(guarded)
for i in range(5):
    s, _ = connect(dev_id, "0" * 64, gport)
    closed(s)
s = socket.create_connection(("127.0.0.1", gport), timeout=5)
check("after 5 failures a new connection gets no hello", closed(s), True)
try:
    connect(dev_id, secret, gport)
    check("...not even with the right secret", "logged in", "refused")
except (ConnectionError, OSError):
    pass

# behind a proxy, the limit is per board address, not the proxy's
proxied = S.Server(S.Devices(devpath), models, "sk-test", cloud_stream=fake_cloud, proxy_protocol=True, login_timeout=1.0)
pport = start(proxied)
attacker = b"PROXY TCP4 203.0.113.66 10.0.0.1 5555 7979\r\n"
for i in range(5):
    s, _ = connect(dev_id, "0" * 64, pport, attacker)
    closed(s)
s = socket.create_connection(("127.0.0.1", pport), timeout=5)
s.sendall(attacker)
check("proxied: the guessing address is blocked", closed(s), True)
s, hello = connect(dev_id, secret, pport, b"PROXY TCP4 198.51.100.7 10.0.0.1 5555 7979\r\n")
check("proxied: another board still logs in", recv_exact(s, 2), b"OK")
s.close()
s = socket.create_connection(("127.0.0.1", pport), timeout=5)
s.sendall(b"CALCNET2 not a proxy header")
check("proxied: a connection without the header is closed", closed(s), True)

print(f"{fails} FAILED" if fails else "cloud server: all passed")
sys.exit(1 if fails else 0)
