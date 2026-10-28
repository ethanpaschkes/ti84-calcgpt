#!/usr/bin/env python3
"""Writes main/secrets.h from your Wi-Fi networks and the bridge secret in ~/.calcnet-token (the secret is never printed).

Wi-Fi networks come from a text file, one per line, "name | password" (the first is your home network; the board tries them in order):
    My Home WiFi | my home password
    My iPhone | my hotspot password
    School Guest |              (nothing after the bar: an open network with no password)
Usage: python3 make_secrets.py [--wifi-file ~/.calcnet-wifi.txt] [--mac-ip 192.168.1.50] [--net-ip 2=172.20.10.2] [--funnel-host NAME.ts.net]
  --mac-ip       the Mac's address on the home network (used only on the first Wi-Fi network, no encryption needed there)
  --net-ip       N=ADDRESS: on Wi-Fi network N (counting from 1) the Mac is at ADDRESS, plain like home (repeatable; for a
                 hotspot shared with a laptop that relays to calcnet)
  --funnel-host  the Mac's Tailscale Funnel name, used from any network over TLS (see ../AWAY.md); leave out to use the home address only
Old style still works for one network:  --ssid "Name" --password "pw" --mac-ip 192.168.1.50"""
import argparse, os

ap = argparse.ArgumentParser()
ap.add_argument("--wifi-file", default=os.path.expanduser("~/.calcnet-wifi.txt"))
ap.add_argument("--ssid")
ap.add_argument("--password")
ap.add_argument("--mac-ip", required=True, help="the Mac's address on the home network, like 192.168.1.50")
ap.add_argument("--net-ip", action="append", default=[], help="N=ADDRESS: the Mac's address on Wi-Fi network N (from 1)")
ap.add_argument("--funnel-host", help="the Mac's Tailscale Funnel name, like my-mac.tailXXXX.ts.net")
ap.add_argument("--port", type=int, default=7878)
ap.add_argument("--funnel-port", type=int, default=10000)
ap.add_argument("--token-file", default=os.path.expanduser("~/.calcnet-token"))
a = ap.parse_args()


def parse_networks(path):
    nets = []
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line.strip() or line.lstrip().startswith("#") or " |" not in line:
            continue
        name, _, password = line.partition(" |")
        if name.strip():
            nets.append((name.strip(), password.strip()))  # an empty password means an open network
    return nets


if a.ssid:
    networks = [(a.ssid, a.password or "")]
else:
    try:
        networks = parse_networks(a.wifi_file)
    except OSError:
        raise SystemExit(f"no Wi-Fi file at {a.wifi_file}; make one with lines like  Home name | password")
if not networks:
    raise SystemExit("no Wi-Fi networks found (lines look like:  Home name | password)")
secret = open(a.token_file).read().strip()
if not secret or any(c in secret for c in '"\\\n'):
    raise SystemExit("the secret file looks wrong; run  python3 ../calcnet.py token  once to make it")


def c_char(c):
    if c in '"\\':
        return "\\" + c
    if 32 <= ord(c) < 127:
        return c
    return "".join(f"\\{b:03o}" for b in c.encode())  # e.g. an iPhone's curly apostrophe, sent to the board as exact UTF-8 bytes


def c_string(s):
    if any(ord(c) < 32 or ord(c) == 127 for c in s):
        raise SystemExit("control characters are not allowed in Wi-Fi names, passwords and addresses")
    return '"' + "".join(c_char(c) for c in s) + '"'


endpoints = [(a.mac_ip, a.port, 0, 0)]                      # home network only, plain
for spec in a.net_ip:
    n, _, ip = spec.partition("=")
    if not n.isdigit() or not 1 <= int(n) <= len(networks) or not ip:
        raise SystemExit(f"--net-ip {spec}: expected N=ADDRESS with N between 1 and {len(networks)}")
    endpoints.append((ip, a.port, 0, int(n) - 1))           # that network only, plain
if a.funnel_host:
    endpoints.append((a.funnel_host, a.funnel_port, 1, -1))  # any network, encrypted
path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "main", "secrets.h")
fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
with os.fdopen(fd, "w") as f:
    f.write("/* written by make_secrets.py: holds your Wi-Fi passwords and the bridge secret. Never commit or share. */\n")
    f.write("struct wifi_net { const char *ssid, *password; };\nstruct endpoint { const char *host; int port; int tls; int wifi; };\n")
    f.write("static const struct wifi_net WIFI_NETS[] = {\n" + "".join(f"    {{{c_string(n)}, {c_string(p)}}},\n" for n, p in networks) + "};\n")
    f.write("#define WIFI_NET_COUNT (sizeof(WIFI_NETS) / sizeof(WIFI_NETS[0]))\n")
    f.write("static const struct endpoint ENDPOINTS[] = {\n" + "".join(f"    {{{c_string(h)}, {p}, {t}, {w}}},\n" for h, p, t, w in endpoints) + "};\n")
    f.write("#define ENDPOINT_COUNT (sizeof(ENDPOINTS) / sizeof(ENDPOINTS[0]))\n")
    f.write(f"#define CALC_SECRET {c_string(secret)}\n")
print(f"wrote {path} (owner-only permissions): {len(networks)} Wi-Fi network(s), {len(endpoints)} Mac address(es)")
