#!/usr/bin/env python3
"""calcnet: makes a Wi-Fi connection look like the calculator's USB cable, so every Mac program in this project works
without the cable.

  Calculator <--USB--> small battery-powered bridge <--Wi-Fi--> this program <--virtual serial port--> reels / screenshare /
  browserd / calcgptd

Usage:
  python3 calcnet.py token                 show the secret to put into the bridge (made on first use, kept in ~/.calcnet-token)
  python3 calcnet.py listen [--port 7878] [--link ~/.calc-port]
                                           wait for the bridge to connect; while it is connected, ~/.calc-port is a serial port
Then run any of the Mac programs with  CALC_PORT=~/.calc-port  in front, for example:
  CALC_PORT=~/.calc-port python3 calcgpt/mac/calcgptd.py

Away from home: the board can reach this program through a Tailscale Funnel address (see AWAY.md); Funnel hands this program plain
connections on localhost, so nothing here changes. Because that address is public, logins are rate limited: after 5 wrong or missing
answers within a minute, new connections are dropped without even a hello until things go quiet.

A new valid login replaces an older connection at once (a board that restarted leaves a dead connection behind; this means the new one
is never kept waiting), and dead connections are noticed within about 30 seconds.

Security: anything that can connect here can act as the calculator (including typing on this Mac when --control is on), so the
bridge must prove it knows the secret: this program sends a random challenge and the bridge answers with HMAC-SHA256 of it, so
the secret itself never travels over Wi-Fi. The data after that is NOT encrypted: use it only on a network you trust (your home
Wi-Fi), never on a shared or public one. Keep ~/.calcnet-token private.

Wire format: after the handshake the connection is the same byte stream as the USB serial link, in both directions.
  server -> bridge:  "CALCNET1" + 16 random bytes        bridge -> server:  HMAC-SHA256(secret, challenge + "calc")  (32 bytes)
  server -> bridge:  "OK"  (then raw bytes)             a wrong answer just gets the connection closed."""
import argparse, hashlib, hmac, os, pty, secrets, select, socket, sys, time, tty

TOKEN_PATH = os.path.expanduser("~/.calcnet-token")
DEFAULT_LINK = os.path.expanduser("~/.calc-port")
HELLO = b"CALCNET1"


def load_token(path=TOKEN_PATH):
    """The shared secret. Made on first use with owner-only permissions."""
    if not os.path.exists(path):
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "w") as f:
            f.write(secrets.token_hex(16) + "\n")
    return open(path).read().strip().encode()


def answer_for(token, challenge):
    """What the bridge must reply: proof it knows the secret, without sending it."""
    return hmac.new(token, challenge + b"calc", hashlib.sha256).digest()


def recv_exact(conn, n, timeout):
    conn.settimeout(timeout)
    data = b""
    while len(data) < n:
        chunk = conn.recv(n - len(data))
        if not chunk:
            raise ConnectionError("closed")
        data += chunk
    return data


def handshake(conn, token, timeout=5.0):
    """True if the other end knows the secret."""
    challenge = secrets.token_bytes(16)
    try:
        conn.sendall(HELLO + challenge)
        reply = recv_exact(conn, 32, timeout)
    except (OSError, ConnectionError):
        return False
    if not hmac.compare_digest(reply, answer_for(token, challenge)):
        return False
    conn.sendall(b"OK")
    return True


class Throttle:
    """Too many wrong logins in a short time and new connections are dropped, so guessing the secret over the internet is hopeless."""
    def __init__(self, limit=5, window=60.0):
        self.limit, self.window, self.fails = limit, window, []

    def blocked(self, now=None):
        now = time.time() if now is None else now
        self.fails = [t for t in self.fails if now - t < self.window]
        return len(self.fails) >= self.limit

    def fail(self, now=None):
        self.fails.append(time.time() if now is None else now)


def tune(conn):
    """Quick delivery, and a dead peer noticed within about 30 s even when nothing is being sent."""
    conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    conn.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    for name, value in (("TCP_KEEPALIVE", 10), ("TCP_KEEPIDLE", 10), ("TCP_KEEPINTVL", 5), ("TCP_KEEPCNT", 4)):
        if hasattr(socket, name):
            try:
                conn.setsockopt(socket.IPPROTO_TCP, getattr(socket, name), value)
            except OSError:
                pass


def login(conn, addr, token, throttle):
    """Checks one new connection. True if it is the bridge; otherwise it is closed here."""
    if throttle.blocked():
        conn.close()
        return False
    tune(conn)
    if not handshake(conn, token):
        throttle.fail()
        print(f"refused a connection from {addr[0]} (wrong or missing secret)", flush=True)
        conn.close()
        return False
    return True


def make_link(link):
    """A virtual serial port, reachable at `link`. Never replaces anything that is not a link made by this program."""
    if os.path.lexists(link) and not os.path.islink(link):
        raise SystemExit(f"{link} exists and is not a link; choose another with --link")
    master, slave = pty.openpty()
    tty.setraw(master)
    tty.setraw(slave)
    if os.path.islink(link):
        os.remove(link)
    os.symlink(os.ttyname(slave), link)
    return master, slave


def drop_link(link, master, slave):
    if os.path.islink(link):
        os.remove(link)
    os.close(master)
    os.close(slave)  # a program reading the port now gets an error, the way it does when the cable is pulled


def pump(conn, master, server=None, token=None, throttle=None):
    """Copy bytes both ways until the Wi-Fi connection ends. If a new bridge logs in meanwhile, returns it (conn, addr) to take over."""
    conn.settimeout(None)
    while True:
        r, _, _ = select.select([conn, master] + ([server] if server else []), [], [], 1.0)
        if server in r:
            new, addr = server.accept()
            if login(new, addr, token, throttle):
                return new, addr
        if conn in r:
            data = conn.recv(65536)
            if not data:
                return None
            view = memoryview(data)
            while view:
                view = view[os.write(master, view):]
        if master in r:
            try:
                data = os.read(master, 65536)
            except OSError:
                return None
            conn.sendall(data)


def serve(host, port, link, token, once=False, ready=None):
    server = socket.socket()
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((host, port))
    server.listen(4)
    if ready:
        ready(server.getsockname()[1])
    throttle, current = Throttle(), None
    while True:
        while current is None:
            conn, addr = server.accept()
            if login(conn, addr, token, throttle):
                current = (conn, addr)
        conn, addr = current
        master, slave = make_link(link)
        print(f"bridge connected from {addr[0]}: {link} is the calculator's port", flush=True)
        try:
            current = pump(conn, master, server, token, throttle)
        except OSError:
            current = None
        finally:
            drop_link(link, master, slave)
            conn.close()
            print("bridge disconnected" if current is None else "bridge replaced by a new login", flush=True)
        if once and current is None:
            return


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("token")
    ls = sub.add_parser("listen")
    ls.add_argument("--port", type=int, default=7878)
    ls.add_argument("--host", default="0.0.0.0", help="address to listen on (default: all, so the bridge can reach this Mac)")
    ls.add_argument("--link", default=DEFAULT_LINK)
    a = ap.parse_args()
    token = load_token()
    if a.cmd == "token":
        print(token.decode())
        return
    print(f"calcnet is waiting for the bridge on port {a.port}. Programs can then use:  CALC_PORT={a.link}")
    try:
        serve(a.host, a.port, os.path.expanduser(a.link), token)
    except KeyboardInterrupt:
        print("stopped")


if __name__ == "__main__":
    main()
