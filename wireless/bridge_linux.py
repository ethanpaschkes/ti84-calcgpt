#!/usr/bin/env python3
"""The wireless bridge for a small Linux computer (a Raspberry Pi Zero 2 W, say) that sits next to the calculator.

  Calculator --USB cable--> this computer (the USB host) --Wi-Fi--> calcnet.py on the Mac

Run on the bridge:   python3 bridge_linux.py --mac Ethans-Mac-Mini.local --token-file ~/.calcnet-token
  (copy the secret from the Mac first:  python3 calcnet.py token   and save it in that file on the bridge)
Start CALCGPT, REELS or any other calculator program on the calculator, plug it into this computer, and on the Mac run
`python3 calcnet.py listen`; then use the Mac programs with CALC_PORT=~/.calc-port. It reconnects by itself when either end drops.

Uses only the Python standard library. See calcnet.py for the login handshake and the security notes (the data is not encrypted:
use a network you trust)."""
import argparse, glob, hashlib, hmac, os, select, socket, sys, termios, time, tty

HELLO = b"CALCNET1"


def answer_for(token, challenge):
    return hmac.new(token, challenge + b"calc", hashlib.sha256).digest()


def recv_exact(sock, n, timeout):
    sock.settimeout(timeout)
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("closed")
        data += chunk
    return data


def connect_to_mac(host, port, token):
    """A logged-in connection to calcnet on the Mac, or raises OSError."""
    sock = socket.create_connection((host, port), timeout=5)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    hello = recv_exact(sock, len(HELLO) + 16, 5)
    if hello[:len(HELLO)] != HELLO:
        raise OSError("that is not calcnet")
    sock.sendall(answer_for(token, hello[len(HELLO):]))
    if recv_exact(sock, 2, 5) != b"OK":
        raise OSError("the Mac did not accept the secret")
    return sock


def open_calculator(device):
    fd = os.open(device, os.O_RDWR | os.O_NOCTTY)
    tty.setraw(fd)
    attrs = termios.tcgetattr(fd)
    attrs[4] = attrs[5] = termios.B115200  # ignored by a USB link, harmless
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    return fd


def find_calculator(device):
    if device:
        return device if os.path.exists(device) else None
    found = sorted(glob.glob("/dev/ttyACM*"))
    return found[0] if found else None


def pump(sock, fd):
    """Copy bytes both ways until either end goes away."""
    sock.settimeout(None)
    while True:
        r, _, _ = select.select([sock, fd], [], [], 1.0)
        if sock in r:
            data = sock.recv(65536)
            if not data:
                return
            view = memoryview(data)
            while view:
                view = view[os.write(fd, view):]
        if fd in r:
            data = os.read(fd, 65536)
            if not data:
                return
            sock.sendall(data)


def run(host, port, token, device, log=print, stop=lambda: False):
    waiting = None
    while not stop():
        path = find_calculator(device)
        if not path:
            if waiting != "calculator":
                log("waiting for the calculator (plug it in and start a program on it)")
                waiting = "calculator"
            time.sleep(1)
            continue
        try:
            sock = connect_to_mac(host, port, token)
        except OSError as e:
            if waiting != "mac":
                log(f"waiting for the Mac ({e})")
                waiting = "mac"
            time.sleep(2)
            continue
        waiting = None
        try:
            fd = open_calculator(path)
        except OSError:
            sock.close()
            time.sleep(1)
            continue
        log(f"connected: {path} <-> {host}:{port}")
        try:
            pump(sock, fd)
        except OSError:
            pass
        finally:
            os.close(fd)
            sock.close()
        log("link ended; starting again")
        time.sleep(0.5)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mac", required=True, help="the Mac's name or address, e.g. Ethans-Mac-Mini.local")
    ap.add_argument("--port", type=int, default=7878)
    ap.add_argument("--token-file", default=os.path.expanduser("~/.calcnet-token"))
    ap.add_argument("--device", help="the calculator's serial device (default: the first /dev/ttyACM*)")
    a = ap.parse_args()
    try:
        token = open(a.token_file).read().strip().encode()
    except OSError:
        sys.exit(f"cannot read the secret from {a.token_file}: copy it from the Mac (python3 calcnet.py token)")
    try:
        run(a.mac, a.port, token, a.device)
    except KeyboardInterrupt:
        print("stopped")


if __name__ == "__main__":
    main()
