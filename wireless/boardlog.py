#!/usr/bin/env python3
"""Shows what the ESP32 bridge board says, sent over Wi-Fi (UDP port 9999), so no cable to the board is needed.
Usage: python3 boardlog.py [logfile]      lines are printed with a time and also appended to the file if one is given.
The first seconds after a restart are not sent (the Wi-Fi is not up yet)."""
import socket, sys, time

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 9999))
out = open(sys.argv[1], "a", buffering=1) if len(sys.argv) > 1 else None
print("listening for the board on UDP port 9999", flush=True)
while True:
    data, addr = sock.recvfrom(2048)
    text = data.decode("ascii", "replace").replace("\r", "").rstrip("\n")
    for line in text.split("\n"):
        stamped = f"{time.strftime('%H:%M:%S')} {addr[0]} {line}"
        print(stamped, flush=True)
        if out:
            out.write(stamped + "\n")
