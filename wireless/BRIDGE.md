# Making the calculator wireless

## The idea
The calculator's USB port is a *device* port (like a flash drive): it needs something acting as the *host* to talk to it. Today
that is the Mac. Without the cable, a small **bridge** next to the calculator acts as the host and passes the bytes over Wi-Fi to
the Mac:

    Calculator --USB--> bridge (host, battery powered) --Wi-Fi--> calcnet.py on the Mac --virtual serial port--> the Mac programs

The Mac programs need no changes: run them with `CALC_PORT=~/.calc-port` in front.

## What is built and tested (on the Mac, with a fake calculator)
- `calcnet.py` (Mac): waits for the bridge, checks a secret with a challenge-and-response login, then makes `~/.calc-port` behave
  like the calculator's serial port. Tests: `test_calcnet.py`.
- `bridge_linux.py` (for a small Linux computer): connects to the Mac, logs in, and copies bytes between the calculator's USB serial
  device and the Wi-Fi connection. Tests: `test_bridge.py` runs the whole path on one computer with a virtual port.
- All the Mac programs take `CALC_PORT`, and treat a dropped link like a pulled cable.

## Status
Working with a real TI-84 Plus CE since 2026-10-28 using the ESP32-S3 board (see **`ESP_SETUP.md`**): the calculator powers the
board through a USB OTG adapter and CalcGPT answers arrive over Wi-Fi. The Raspberry Pi route (**`PI_SETUP.md`**) is written up but
was not built.

## Hardware options
1. **Raspberry Pi Zero 2 W** (the safest): Linux already acts as a USB host and sees the calculator as `/dev/ttyACM0`, so
   `bridge_linux.py` should run as it is. Needs: the board, a microSD card with Raspberry Pi OS, a micro-USB OTG adapter plus a
   USB-A to mini-USB cable (or one OTG cable that ends in the calculator's mini-B plug), and a power bank. Roughly $25 or more in
   total (check current prices); the Pi draws a few hundred mA.
2. **An ESP32-S3 board** (cheaper, about $10-15 plus a cable, check prices): it has a USB host mode, but the firmware must be written
   (ESP-IDF's CDC-ACM host driver + Wi-Fi + the same login) and tested on the real board, and the board must supply 5 V on its USB
   host port (some need a solder jumper). Higher risk; I would write it once you have the board, so we can try it together.
3. **Not recommended:** plugging a Wi-Fi USB dongle into the calculator (no driver), or anything that tries to power itself from the
   calculator's port (its limit is unverified, a forum poster guessed 100 mA).

## Security, plainly
Anything that can connect to `calcnet.py` can act as the calculator, including typing on the Mac in control mode, so it requires the
secret in `~/.calcnet-token` (never sent over Wi-Fi; only a proof of it is). The data after login is not encrypted. Use it on your
home network only. The secret stays on your two computers.

## First test, when hardware arrives
1. Mac: `python3 calcnet.py token` and copy the secret to the bridge (`~/.calcnet-token` there).
2. Mac: `python3 calcnet.py listen`.
3. Bridge: `python3 bridge_linux.py --mac <your Mac's name>.local`.
4. Calculator: start CALCGPT (or another program) and plug it into the bridge.
5. Mac: `CALC_PORT=~/.calc-port python3 calcgpt/mac/calcgptd.py`.
