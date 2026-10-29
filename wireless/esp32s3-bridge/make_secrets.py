#!/usr/bin/env python3
"""Writes main/secrets.h: the servers this board talks to, Wi-Fi networks it knows from the start, and its secrets (never printed).

For a board that uses the CalcGPT server (any network, Wi-Fi added later from CALCGPT's Wi-Fi screen):
    python3 make_secrets.py --cloud-host calcgpt.example.com --device-file board1.txt
  board1.txt holds the two lines cloud/add_device.py printed (DEVICE_ID=... and DEVICE_SECRET=...).

Also talking to calcnet on your Mac at home (for REELS, BROWSER and the other Mac programs):
    add  --mac-ip 192.168.1.50  (and the Wi-Fi file below; the Mac is used only on the first network in it)

Wi-Fi networks built in from the start come from a text file, one per line, "name | password" (optional; default
~/.calcnet-wifi.txt if it exists):
    My Home WiFi | my home password
    School Guest |              (nothing after the bar: an open network with no password)

Options:
  --mac-ip ADDRESS     the Mac's address on the first Wi-Fi network (calcnet, plain, shared secret from ~/.calcnet-token)
  --net-ip N=ADDRESS   on Wi-Fi network N (counting from 1) the Mac is at ADDRESS too (repeatable)
  --funnel-host NAME   the Mac's Tailscale Funnel name, used from any network over TLS (calcnet)
  --cloud-host NAME    the CalcGPT server, used from any network over TLS on port 443 (--cloud-port to change)
  --cloud-plain        talk to the CalcGPT server without TLS (only for testing on your own network)
  --device-file FILE   this board's DEVICE_ID / DEVICE_SECRET lines from cloud/add_device.py"""
import argparse, os, re

ap = argparse.ArgumentParser()
ap.add_argument("--wifi-file", default=os.path.expanduser("~/.calcnet-wifi.txt"))
ap.add_argument("--ssid")
ap.add_argument("--password")
ap.add_argument("--mac-ip", help="the Mac's address on the first Wi-Fi network, like 192.168.1.50")
ap.add_argument("--net-ip", action="append", default=[], help="N=ADDRESS: the Mac's address on Wi-Fi network N (from 1)")
ap.add_argument("--funnel-host", help="the Mac's Tailscale Funnel name, like my-mac.tailXXXX.ts.net")
ap.add_argument("--port", type=int, default=7878)
ap.add_argument("--funnel-port", type=int, default=10000)
ap.add_argument("--cloud-host", help="the CalcGPT server's name, like calcgpt.example.com")
ap.add_argument("--cloud-port", type=int)
ap.add_argument("--cloud-plain", action="store_true")
ap.add_argument("--device-file")
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
elif os.path.exists(a.wifi_file):
    networks = parse_networks(a.wifi_file)
else:
    networks = []
if not (a.mac_ip or a.net_ip or a.funnel_host or a.cloud_host):
    raise SystemExit("give at least one server: --cloud-host, --mac-ip or --funnel-host")
if (a.mac_ip or a.net_ip) and not networks:
    raise SystemExit("--mac-ip needs the Wi-Fi file: the Mac is reached on the first network in it")

secret = ""
if a.mac_ip or a.net_ip or a.funnel_host:
    secret = open(a.token_file).read().strip()
    if not secret or any(c in secret for c in '"\\\n'):
        raise SystemExit("the secret file looks wrong; run  python3 ../calcnet.py token  once to make it")

device_id = device_secret = ""
if a.cloud_host:
    if not a.device_file:
        raise SystemExit("--cloud-host needs --device-file (the two lines cloud/add_device.py printed for this board)")
    fields = dict(line.strip().split("=", 1) for line in open(a.device_file, encoding="utf-8") if "=" in line)
    device_id, device_secret = fields.get("DEVICE_ID", ""), fields.get("DEVICE_SECRET", "")
    if not re.fullmatch(r"[a-z0-9]{1,16}", device_id) or not re.fullmatch(r"[0-9a-f]{16,128}", device_secret):
        raise SystemExit(f"{a.device_file} needs DEVICE_ID=... (up to 16 letters/digits) and DEVICE_SECRET=... (hex)")


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


endpoints = []                                               # (host, port, tls, network name or "" for any network)
if a.mac_ip:
    endpoints.append((a.mac_ip, a.port, 0, networks[0][0]))
for spec in a.net_ip:
    n, _, ip = spec.partition("=")
    if not n.isdigit() or not 1 <= int(n) <= len(networks) or not ip:
        raise SystemExit(f"--net-ip {spec}: expected N=ADDRESS with N between 1 and {len(networks)}")
    endpoints.append((ip, a.port, 0, networks[int(n) - 1][0]))
if a.funnel_host:
    endpoints.append((a.funnel_host, a.funnel_port, 1, ""))
if a.cloud_host:
    endpoints.append((a.cloud_host, a.cloud_port or (7979 if a.cloud_plain else 443), 0 if a.cloud_plain else 1, ""))

path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "main", "secrets.h")
fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
with os.fdopen(fd, "w") as f:
    f.write("/* written by make_secrets.py: holds Wi-Fi passwords and secrets. Never commit or share. */\n")
    f.write("struct wifi_net { const char *ssid, *password; };\n")
    f.write("struct endpoint { const char *host; int port; int tls; const char *ssid; };   /* ssid \"\" = any network */\n")
    f.write("static const struct wifi_net WIFI_NETS[] = {\n" + "".join(f"    {{{c_string(n)}, {c_string(p)}}},\n" for n, p in networks)
            + ("" if networks else '    {"", ""},\n') + "};\n")
    f.write(f"#define WIFI_NET_COUNT {len(networks)}\n")
    f.write("static const struct endpoint ENDPOINTS[] = {\n" + "".join(f"    {{{c_string(h)}, {p}, {t}, {c_string(s)}}},\n" for h, p, t, s in endpoints)
            + "};\n")
    f.write("#define ENDPOINT_COUNT (sizeof(ENDPOINTS) / sizeof(ENDPOINTS[0]))\n")
    f.write(f"#define CALC_SECRET {c_string(secret)}\n")
    f.write(f'#define DEVICE_ID_PADDED "{device_id}' + "\\0" * (16 - len(device_id)) + '"\n')
    f.write(f'#define DEVICE_SECRET "{device_secret}"\n')
print(f"wrote {path} (owner-only permissions): {len(networks)} built-in Wi-Fi network(s), {len(endpoints)} server(s)"
      + (f", device {device_id}" if device_id else ""))
