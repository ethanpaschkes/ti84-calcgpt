#!/usr/bin/env python3
"""Adds a bridge board to the CalcGPT cloud server, or switches one off or on again.

Usage:
  python3 cloud/add_device.py NAME [--limit 200]     makes a new device; prints the two lines for the board's secrets.h
  python3 cloud/add_device.py --disable ID           switches a device off (a leaked board stops working at its next question)
  python3 cloud/add_device.py --enable ID            switches it on again
  python3 cloud/add_device.py --list                 names, ids, limits and on/off (never the secrets)
Option --devices PATH picks the devices file (default: env CALCGPT_DEVICES, else cloud/devices.json).

The devices file holds every board's secret: it is readable only by you, and it must never be committed or shared.
The running server notices changes to it by itself; no restart needed."""
import argparse, os, secrets, string, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from server import DEFAULT_DEVICES, DEFAULT_LIMIT, ID_LEN, read_devices, write_private_json  # noqa: E402

ID_CHARS = string.ascii_lowercase + string.digits


def new_device(path, name, limit=DEFAULT_LIMIT):
    """Saves a new device and returns (id, secret)."""
    devices = read_devices(path)
    while True:
        device_id = "".join(secrets.choice(ID_CHARS) for _ in range(ID_LEN))
        if device_id not in devices:
            break
    secret = secrets.token_hex(32)
    devices[device_id] = {"secret": secret, "name": name, "enabled": True, "daily_limit": limit}
    write_private_json(path, devices)
    return device_id, secret


def set_enabled(path, device_id, on):
    devices = read_devices(path)
    if device_id not in devices:
        sys.exit(f"No device {device_id!r} in {path}. See them with --list.")
    devices[device_id]["enabled"] = on
    write_private_json(path, devices)
    return devices[device_id].get("name", device_id)


def main():
    ap = argparse.ArgumentParser(description="Add a CalcGPT board, or switch one off or on.")
    ap.add_argument("name", nargs="?", help="a name for the new board, e.g. the student's name")
    ap.add_argument("--limit", type=int, default=DEFAULT_LIMIT, help="questions per day (UTC), default %(default)s")
    ap.add_argument("--disable", metavar="ID")
    ap.add_argument("--enable", metavar="ID")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--devices", default=DEFAULT_DEVICES)
    a = ap.parse_args()
    if a.disable or a.enable:
        on = bool(a.enable)
        name = set_enabled(a.devices, a.enable or a.disable, on)
        print(f"{name}: switched {'on' if on else 'off'}")
    elif a.list:
        for device_id, d in read_devices(a.devices).items():
            print(f"{device_id}  {'on ' if d.get('enabled', True) else 'off'}  limit {d.get('daily_limit', DEFAULT_LIMIT):>4}  {d.get('name', '')}")
    elif a.name:
        if a.limit < 1:
            sys.exit("--limit must be at least 1.")
        device_id, secret = new_device(a.devices, a.name.strip(), a.limit)
        print(f"DEVICE_ID={device_id}")
        print(f"DEVICE_SECRET={secret}")
        print("Keep these two lines secret: put them only into this board's secrets.h, never in Git or a chat.", file=sys.stderr)
    else:
        ap.error("give a NAME for a new board, or --disable ID, --enable ID or --list")


if __name__ == "__main__":
    main()
