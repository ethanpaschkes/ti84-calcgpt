#!/usr/bin/env python3
"""relay: lets the bridge board reach calcnet on another Mac through this one (for example a laptop at school, on the same Wi-Fi
as the board, passing everything to the home Mac over Tailscale).

  board --Wi-Fi--> this Mac (TCP 7878, UDP 9999) --Tailscale--> the Mac running calcnet.py and boardlog.py

The login and the secret pass through untouched: this program never sees the secret and cannot act as the calculator. The board's
log lines (UDP 9999) are printed here and passed on too, so /tmp/board.log on the home Mac stays complete.

Usage: python3 relay.py --to HOME-MAC        HOME-MAC = its Tailscale name or address (e.g. my-mac-mini)
The first run makes macOS ask whether Python may accept incoming connections: allow it."""
import argparse, select, socket, threading, time


def pipe(a, b):
    """Copies both ways between two connections until either closes."""
    try:
        while True:
            r, _, _ = select.select([a, b], [], [])
            for src in r:
                data = src.recv(65536)
                if not data:
                    return
                (b if src is a else a).sendall(data)
    except OSError:
        pass
    finally:
        a.close()
        b.close()


def serve_tcp(port, to):
    server = socket.socket()
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", port))
    server.listen(4)
    while True:
        conn, addr = server.accept()
        try:
            home = socket.create_connection((to, port), timeout=5)
        except OSError as e:
            print(f"{time.strftime('%H:%M:%S')} the board connected from {addr[0]} but {to}:{port} did not answer ({e})", flush=True)
            conn.close()
            continue
        home.settimeout(None)
        for s in (conn, home):
            s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        print(f"{time.strftime('%H:%M:%S')} the board connected from {addr[0]}: passing it on to {to}", flush=True)
        threading.Thread(target=pipe, args=(conn, home), daemon=True).start()


def serve_logs(port, to):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", port))
    while True:
        data, addr = sock.recvfrom(2048)
        print(f"{time.strftime('%H:%M:%S')} {addr[0]} {data.decode('ascii', 'replace').rstrip()}", flush=True)
        try:
            sock.sendto(data, (to, port))
        except OSError:
            pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--to", required=True, help="the Mac running calcnet.py (Tailscale name or address)")
    ap.add_argument("--port", type=int, default=7878)
    ap.add_argument("--log-port", type=int, default=9999)
    a = ap.parse_args()
    print(f"relay: the board's port {a.port} on this Mac -> {a.to}. This Mac's Wi-Fi address is the one for make_secrets.py --net-ip. "
          "Ctrl+C stops.", flush=True)
    threading.Thread(target=serve_logs, args=(a.log_port, a.to), daemon=True).start()
    try:
        serve_tcp(a.port, a.to)
    except KeyboardInterrupt:
        print("stopped")


if __name__ == "__main__":
    main()
