"""Tests the whole wireless path on this computer: a fake calculator (a virtual serial port) -> bridge_linux.py -> TCP -> calcnet.py
-> a program reading CALC_PORT. Not covered: the real USB hardware, and the bridge's automatic reconnecting.  Run: python3 test_bridge.py"""
import os, pty, random, select, sys, tempfile, threading, time, tty
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import calcnet as N
import bridge_linux as BL

fails = 0
def check(what, got, want):
    global fails
    if got != want:
        print(f"FAIL {what}\n   got  {got!r}\n   want {want!r}")
        fails += 1

tmp = tempfile.mkdtemp()
token = N.load_token(os.path.join(tmp, "token"))
link = os.path.join(tmp, "calc-port")
ready = threading.Event(); box = []
threading.Thread(target=N.serve, args=("127.0.0.1", 0, link, token), kwargs={"ready": lambda p: (box.append(p), ready.set())}, daemon=True).start()
ready.wait(5); port = box[0]

# the fake calculator: the master end of a virtual serial port; the bridge opens the slave end like /dev/ttyACM0
calc_master, calc_slave = pty.openpty()
tty.setraw(calc_master); tty.setraw(calc_slave)
device = os.ttyname(calc_slave)
stop = threading.Event(); logs = []
threading.Thread(target=BL.run, args=("127.0.0.1", port, token, device), kwargs={"log": logs.append, "stop": stop.is_set}, daemon=True).start()
for _ in range(100):
    if os.path.islink(link): break
    time.sleep(0.05)
check("the link appears once the bridge has logged in", os.path.islink(link), True)
check("the bridge says it is connected", any(l.startswith("connected") for l in logs), True)

prog = os.open(link, os.O_RDWR | os.O_NOCTTY); tty.setraw(prog)
rng = random.Random(11)

def read_all(fd, n, seconds=10):
    got, end = b"", time.time() + seconds
    while len(got) < n and time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.2)
        if r: got += os.read(fd, 65536)
    return got

# calculator -> program (key reports)
keys = bytes(rng.randrange(256) for _ in range(30_000))
def calc_sends():
    i = 0
    while i < len(keys):
        n = rng.randrange(1, 300); os.write(calc_master, keys[i:i + n]); i += n
threading.Thread(target=calc_sends, daemon=True).start()
check("30 KB from the calculator reach the program", read_all(prog, len(keys)), keys)
# program -> calculator (pictures)
pics = bytes(rng.randrange(256) for _ in range(150_000))
def prog_sends():
    view = memoryview(pics)
    while view: view = view[os.write(prog, view[:4096]):]
threading.Thread(target=prog_sends, daemon=True).start()
check("150 KB from the program reach the calculator", read_all(calc_master, len(pics)), pics)

os.close(prog)
stop.set()
print(f"{fails} FAILED" if fails else "bridge: all passed")
sys.exit(1 if fails else 0)
