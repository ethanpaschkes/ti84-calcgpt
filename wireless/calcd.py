#!/usr/bin/env python3
"""calcd: starts the right Mac program by itself for whatever the calculator runs over the wireless bridge.

  calculator --Wi-Fi--> calcnet.py (makes ~/.calc-port) --> this program --> calcgptd / browserd / screenshare / reels

Every time a program on the calculator opens the USB link, the board logs in to calcnet and ~/.calc-port appears. This program
opens it and tells the programs apart by their first bytes:
  CALCGPT  sends "R" at once (it asks for the model list), repeated each second until the Mac answers; an older CALCGPT that
           asked only once, while the board was still starting, is known by its first question ("Q") instead -> calcgpt/mac/calcgptd.py
  REELS    sends its keyboard ("S" + 7 bytes) within a moment                  -> reels/mac/screenshare.py (or reels.py, --reels clips)
  BROWSER  sends nothing until the Mac draws its home page                     -> browser/mac/browserd.py
The Mac program gets its own virtual serial port. The bytes used to tell the programs apart are handed on once the Mac program
has written its first bytes (every Mac program clears its port when it opens it, so anything sent earlier would be lost).
When the calculator program quits (the link closes), the Mac program is stopped too, and this waits for the next one.

Usage:
  python3 calcd.py [--reels share|clips] [--control] [--link ~/.calc-port]
--control lets REELS control this Mac's mouse and keyboard (needs Accessibility permission, see reels/README.md)."""
import argparse, os, pty, select, signal, subprocess, sys, termios, time, tty

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QUIET_FOR_BROWSER = 2.0   # seconds of silence that mean BROWSER (REELS reports its keyboard several times a second)


def log(*parts):
    print(time.strftime("%H:%M:%S"), *parts, flush=True)


def identify(first):
    """Which calculator program sent these first bytes (empty: it stayed quiet). None if they are not from a known program."""
    if not first:
        return "browser"
    if first[:1] in (b"R", b"Q", b"N", b"M"):   # BROWSER cannot send a search before its Mac side has drawn the home page
        return "calcgpt"
    if first[:1] == b"S":
        return "reels"
    return None


def command_for(program, a):
    script = {"calcgpt": ["calcgpt/mac/calcgptd.py", "--keep-alive", "10m"],
              "browser": ["browser/mac/browserd.py"],
              "reels": ["reels/mac/screenshare.py"] + (["--control"] if a.control else []) if a.reels == "share" else ["reels/mac/reels.py"]}[program]
    return [sys.executable, os.path.join(ROOT, script[0])] + script[1:]


def read_first(fd):
    """The calculator's first bytes: as soon as there is a whole first message, or whatever came after a quiet spell."""
    data, end = b"", time.time() + QUIET_FOR_BROWSER
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if r:
            chunk = os.read(fd, 1024)
            if not chunk:
                raise OSError("the link closed")
            data += chunk
        if (data and data[:1] != b"S") or len(data) >= 8:
            break
    return data


def write_all(fd, data):
    view = memoryview(data)
    while view:
        view = view[os.write(fd, view):]


def run(program, calc, first, a):
    """Runs the Mac program on its own virtual port, copying bytes between it and the calculator until one of them stops."""
    master, slave = pty.openpty()
    tty.setraw(master)
    tty.setraw(slave)
    env = dict(os.environ, CALC_PORT=os.ttyname(slave), PYTHONUNBUFFERED="1")
    proc = subprocess.Popen(command_for(program, a), cwd=ROOT, env=env, start_new_session=True)
    why = "the Mac program stopped"
    try:
        while proc.poll() is None:
            r, _, _ = select.select([calc, master], [], [], 0.2)
            if calc in r:
                data = os.read(calc, 65536)
                if not data:
                    why = "the calculator link closed"
                    break
                write_all(master, data)
            if master in r:
                try:
                    data = os.read(master, 65536)
                except OSError:
                    continue
                write_all(calc, data)
                if first:
                    write_all(master, first)   # the Mac program is listening now
                    first = b""
    except OSError:
        why = "the calculator link closed"
    os.close(master)
    os.close(slave)
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        os.killpg(proc.pid, signal.SIGTERM)
        proc.wait()
    log(f"{program} ended ({why}, exit code {proc.returncode})")


def drain(fd):
    """Throws away what the calculator sends until the link closes, so the next program starts on a fresh link."""
    try:
        while os.read(fd, 65536):
            pass
    except OSError:
        pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--link", default="~/.calc-port")
    ap.add_argument("--reels", choices=["share", "clips"], default="share", help="what REELS shows: this Mac's screen, or video clips")
    ap.add_argument("--control", action="store_true", help="let REELS control this Mac's mouse and keyboard")
    a = ap.parse_args()
    link = os.path.expanduser(a.link)
    log(f"calcd is waiting for a calculator program on {link} (REELS shows {'the screen' if a.reels == 'share' else 'clips'})")
    while True:
        if not os.path.exists(link):
            time.sleep(0.2)
            continue
        try:
            calc = os.open(link, os.O_RDWR | os.O_NOCTTY)
        except OSError:
            time.sleep(0.2)
            continue
        try:
            tty.setraw(calc, termios.TCSANOW)   # not the default TCSAFLUSH: that would throw away the first bytes
            first = read_first(calc)
            program = identify(first)
            if program:
                log(f"the calculator is running {program.upper()}: starting its Mac program")
                run(program, calc, first, a)
            else:
                log(f"unknown first bytes from the calculator: {first[:16].hex()}; waiting for the link to close")
            drain(calc)
        except OSError:
            log("the link closed before a program could be recognised")
        finally:
            os.close(calc)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("stopped")
