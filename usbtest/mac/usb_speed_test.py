#!/usr/bin/env python3
"""Sends data to the calculator (running USBTEST) as fast as it will take it, for a few seconds,
and prints the speed. Also prints any key letters the calculator sends back (U D L R E).

Usage:  python3 usb_speed_test.py [seconds per size]      (default 3)
Tries several write sizes in turn, because each write to the USB serial port may carry a fixed delay.
Needs no extra packages. Close TI Connect CE first, and run USBTEST on the calculator before this."""
import glob, os, select, sys, termios, time, tty

seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 3.0
ports = ([os.environ["CALC_PORT"]] if os.path.exists(os.environ["CALC_PORT"]) else []) if os.environ.get("CALC_PORT") else sorted(glob.glob("/dev/cu.usbmodem*"))
if not ports:
    sys.exit("No calculator found. Run USBTEST on it, plug in the cable, close TI Connect CE, then try again.")
fd = os.open(ports[0], os.O_RDWR | os.O_NOCTTY)
tty.setraw(fd)
attrs = termios.tcgetattr(fd)
attrs[4] = attrs[5] = termios.B115200  # the speed setting is ignored on a USB link
termios.tcsetattr(fd, termios.TCSANOW, attrs)
print("Connected to", ports[0], "- trying 4 write sizes,", seconds, "seconds each.")

best = 0
keys = ""
for size in (1024, 4096, 16384, 65536):
    chunk = bytes(range(256)) * (size // 256)
    sent, start = 0, time.time()
    while time.time() - start < seconds:
        sent += os.write(fd, chunk)
        r, _, _ = select.select([fd], [], [], 0)
        if r:
            keys += os.read(fd, 64).decode("latin-1")
    elapsed = time.time() - start
    rate = sent / elapsed
    best = max(best, rate)
    print(f"  writes of {size:6d} bytes: {rate:9,.0f} bytes/s ({rate / 1024:,.0f} KB/s)")
    time.sleep(0.5)  # let the calculator catch up before the next size
rate = best
print("Frames per second this speed could carry, if every frame were:")
for name, size in [("320x240, 256 colors", 76800), ("320x240, 16 colors", 38400),
                   ("160x120, 256 colors", 19200), ("160x120, 16 colors", 9600), ("320x240, black and white", 9600)]:
    print(f"  {name:28s} {size:6d} bytes -> {rate / size:5.1f} frames/s (before compression)")
print("Keys the calculator sent back:", keys if keys else "(none - press arrow keys or Enter on the calculator during the test)")
os.close(fd)
