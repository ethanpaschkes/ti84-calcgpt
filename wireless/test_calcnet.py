"""Tests for calcnet.py: login, passing bytes both ways, disconnecting, and one whole program over it.  Run: python3 test_calcnet.py"""
import glob, os, random, socket, stat, sys, tempfile, threading, time, tty
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import calcnet as N

fails = 0


def check(what, got, want):
    global fails
    if got != want:
        print(f"FAIL {what}\n   got  {got!r}\n   want {want!r}")
        fails += 1


tmp = tempfile.mkdtemp()
tokpath = os.path.join(tmp, "token")
token = N.load_token(tokpath)
check("token is 32 hex characters", len(token) == 32 and all(c in b"0123456789abcdef" for c in token), True)
check("token file is private", stat.S_IMODE(os.stat(tokpath).st_mode), 0o600)
check("token is kept", N.load_token(tokpath), token)

# ---- the login
def pair():
    a, b = socket.socketpair()
    return a, b
def bridge_login(sock, secret):
    """What a bridge does: read the hello and challenge, answer, read OK."""
    hello = N.recv_exact(sock, 24, 3)
    sock.sendall(N.answer_for(secret, hello[8:]))
    return hello
server_side, bridge_side = pair()
t = threading.Thread(target=lambda: bridge_login(bridge_side, token)); t.start()
check("right secret is accepted", N.handshake(server_side, token), True)
t.join(); check("bridge told OK", N.recv_exact(bridge_side, 2, 3), b"OK")
server_side, bridge_side = pair()
t = threading.Thread(target=lambda: bridge_login(bridge_side, b"wrong secret")); t.start()
check("wrong secret is refused", N.handshake(server_side, token), False); t.join()
server_side, bridge_side = pair()
bridge_side.sendall(b"x" * 32)
check("garbage is refused", N.handshake(server_side, token), False)
server_side, bridge_side = pair()
t0 = time.time()
check("silence is refused after the time limit", N.handshake(server_side, token, timeout=0.3), False)
check("...and does not hang", time.time() - t0 < 2, True)
server_side, bridge_side = pair(); bridge_side.close()
check("a closed connection is refused", N.handshake(server_side, token), False)
a1 = N.answer_for(token, b"A" * 16); a2 = N.answer_for(token, b"B" * 16)
check("each challenge needs its own answer", a1 != a2 and len(a1) == 32, True)

# ---- the link refuses to replace a real file
real = os.path.join(tmp, "real-file"); open(real, "w").write("keep me")
try:
    N.make_link(real)
    check("a real file is never replaced", "replaced", "refused")
except SystemExit:
    check("the real file is untouched", open(real).read(), "keep me")

# ---- a whole connection
link = os.path.join(tmp, "calc-port")
ready = threading.Event(); port_box = []
def on_ready(p): port_box.append(p); ready.set()
threading.Thread(target=N.serve, args=("127.0.0.1", 0, link, token), kwargs={"ready": on_ready}, daemon=True).start()
ready.wait(5)
port = port_box[0]

def connect(secret):
    s = socket.create_connection(("127.0.0.1", port), timeout=5)
    N.recv_exact(s, 0, 1)
    hello = N.recv_exact(s, 24, 3)
    s.sendall(N.answer_for(secret, hello[8:]))
    return s

bad = connect(b"not the secret")
bad.settimeout(2)
try:
    got = bad.recv(10)
except OSError:
    got = b""
check("a wrong secret gets no OK and no link", (got, os.path.lexists(link)), (b"", False))
bad.close()

s = connect(token)
check("right secret gets OK", N.recv_exact(s, 2, 3), b"OK")
for _ in range(50):
    if os.path.islink(link): break
    time.sleep(0.05)
check("the link appears", os.path.islink(link), True)
fd = os.open(link, os.O_RDWR | os.O_NOCTTY)
tty.setraw(fd)
rng = random.Random(3)
# program -> bridge (like screen pictures going to the calculator)
payload = bytes(rng.randrange(256) for _ in range(200_000))
def writer():
    view = memoryview(payload)
    while view:
        view = view[os.write(fd, view[:4096]):]
threading.Thread(target=writer, daemon=True).start()
check("200 KB from the program reach the bridge intact", N.recv_exact(s, len(payload), 10), payload)
# bridge -> program (like key presses coming back), in odd sizes
back = bytes(rng.randrange(256) for _ in range(50_000))
def sender():
    i = 0
    while i < len(back):
        n = rng.randrange(1, 700)
        s.sendall(back[i:i + n]); i += n
threading.Thread(target=sender, daemon=True).start()
got = b""
import select
end = time.time() + 10
while len(got) < len(back) and time.time() < end:
    r, _, _ = select.select([fd], [], [], 0.2)
    if r: got += os.read(fd, 65536)
check("50 KB from the bridge reach the program intact", got, back)
s.close()
ended = False
for _ in range(50):
    try:
        r, _, _ = select.select([fd], [], [], 0.2)
        if r and os.read(fd, 10) == b"":
            ended = True; break
    except OSError:
        ended = True; break
check("the program sees the link end (an empty read or an error)", ended, True)
for _ in range(50):
    if not os.path.lexists(link): break
    time.sleep(0.05)
check("the link disappears", os.path.lexists(link), False)
os.close(fd)

# ---- the bridge can connect again afterwards
s = connect(token)
check("reconnecting works", N.recv_exact(s, 2, 3), b"OK")
for _ in range(50):
    if os.path.islink(link): break
    time.sleep(0.05)
check("and the link is back", os.path.islink(link), True)

# ---- a new login replaces an old, dead-looking connection at once
old = connect(token)
check("old connection is up", N.recv_exact(old, 2, 3), b"OK")
new = connect(token)
check("the new login is accepted while the old one still sits there", N.recv_exact(new, 2, 3), b"OK")
old.settimeout(3)
try:
    gone = old.recv(10)
except OSError:
    gone = b""
check("the old connection was closed by the server", gone, b"")
for _ in range(50):
    if os.path.islink(link): break
    time.sleep(0.05)
check("the link is there for the new one", os.path.islink(link), True)
new.close(); old.close()
time.sleep(0.3)
s = connect(token)   # the later tests use this one
check("a fresh login after the takeover works", N.recv_exact(s, 2, 3), b"OK")
for _ in range(50):
    if os.path.islink(link): break
    time.sleep(0.05)

# ---- the login limit
th = N.Throttle(limit=3, window=10)
check("fresh throttle lets people in", th.blocked(100), False)
th.fail(100); th.fail(101)
check("two failures are tolerated", th.blocked(102), False)
th.fail(102)
check("the third failure blocks", th.blocked(103), True)
check("the block ends when the failures age out", th.blocked(113), False)
# a second server, so the lock-out does not affect the tests below
ready2 = threading.Event(); port2 = []
threading.Thread(target=N.serve, args=("127.0.0.1", 0, os.path.join(tmp, "calc-port2"), token),
                 kwargs={"ready": lambda p: (port2.append(p), ready2.set())}, daemon=True).start()
ready2.wait(5)
def guess(p):
    c = socket.create_connection(("127.0.0.1", p), timeout=5)
    hello = N.recv_exact(c, 24, 3)
    c.sendall(N.answer_for(b"guess", hello[8:]))
    c.settimeout(2)
    try: c.recv(10)
    except OSError: pass
    c.close()
for _ in range(5):
    guess(port2[0])
try:
    blocked = socket.create_connection(("127.0.0.1", port2[0]), timeout=3)
    blocked.settimeout(2)
    first = blocked.recv(10)
except OSError:
    first = b""
check("after five wrong answers a new connection gets no hello", first, b"")

# ---- a real program over the link: the browser's Mac side, with pages faked so no internet is needed
sys.path.insert(0, os.path.join(HERE, "..", "browser", "mac"))
import browserd as B
from wire_decode import decode
PAGES = {"https://site.test/a": "<title>A</title><h1>Page A</h1><p>text</p>"}
def fake_get(url, headers=None):
    if url in PAGES: return url, PAGES[url]
    raise ValueError("could not connect")
Real = B.Session
B.Session = lambda search, bookmarks, brave_key=None: Real("auto", [("Site A", "https://site.test/a")], get=fake_get, brave_key=None)
os.environ["CALC_PORT"] = link
sys.argv = ["browserd.py", "--quiet"]
done = threading.Thread(target=B.main, daemon=True); done.start()
buf = b""
def pump_until(marker, seconds=5):
    global buf
    end = time.time() + seconds
    while time.time() < end and marker not in buf:
        r, _, _ = select.select([s], [], [], 0.05)
        if r: buf += s.recv(65536)
pump_until(b"Site A")
check("the browser's home page arrives over the wireless link", decode(buf)[0], ("H", ["Site A"]))
buf = b""
s.sendall(b"U" + bytes([10]) + b"site.test/a"[:10])
s.sendall(b"U" + bytes([1]) + b"?")  # an odd request must not break anything
s.sendall(b"G")
pump_until(b"Site A", 3)
s.sendall(b"Z")
done.join(5)
check("the program ended when asked to quit", done.is_alive(), False)
s.close()
# a program must notice when the wireless link drops, not spin forever
for _ in range(50):
    if not os.path.lexists(link): break
    time.sleep(0.05)
s = connect(token)
N.recv_exact(s, 2, 3)
for _ in range(50):
    if os.path.islink(link): break
    time.sleep(0.05)
done = threading.Thread(target=B.main, daemon=True); done.start()
buf = b""
pump_until(b"Site A")
s.close()
done.join(6)
check("a program stops when the wireless link drops", done.is_alive(), False)
print(f"{fails} FAILED" if fails else "calcnet: all passed")
sys.exit(1 if fails else 0)
