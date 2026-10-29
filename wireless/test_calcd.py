"""Tests for calcd.py: it recognises each calculator program by its first bytes and starts that program's Mac side on a fake
calcnet link (the real Mac programs run; nothing goes to the internet).  Run: python3 test_calcd.py"""
import os, select, subprocess, sys, tempfile, time
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import calcd as D
import calcnet as N

fails = 0


def check(what, got, want):
    global fails
    if got != want:
        print(f"FAIL {what}\n   got  {got!r}\n   want {want!r}")
        fails += 1


check("silence is BROWSER", D.identify(b""), "browser")
check("R is CALCGPT", D.identify(b"R"), "calcgpt")
check("a question first is an older CALCGPT", D.identify(b"Q\x02hi"), "calcgpt")
check("a keyboard report is REELS", D.identify(b"S" + bytes(7)), "reels")
check("anything else is unknown", D.identify(b"\x00\x01"), None)


def read_for(fd, seconds):
    data, end = b"", time.time() + seconds
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.1)
        if r:
            try:
                data += os.read(fd, 65536)
            except OSError:
                break
    return data


def wait_for_line(out, text, seconds=10):
    end, seen = time.time() + seconds, ""
    while time.time() < end:
        r, _, _ = select.select([out], [], [], 0.2)
        if r:
            line = out.readline()
            seen += line
            if text in line:
                return True
    print("   calcd said:", seen.strip() or "(nothing)")
    return False


link = os.path.join(tempfile.mkdtemp(), "calc-port")
calcd = subprocess.Popen([sys.executable, os.path.join(HERE, "calcd.py"), "--link", link], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
try:
    check("calcd starts", wait_for_line(calcd.stdout, "waiting for a calculator program"), True)
    for first, name in ((b"R", "CALCGPT"), (b"", "BROWSER")):
        master, slave = N.make_link(link)     # what calcnet does when the board logs in
        os.write(master, first)
        check(f"{name} recognised", wait_for_line(calcd.stdout, f"running {name}"), True)
        check(f"{name}'s Mac program answers the calculator", len(read_for(master, 4)) > 0, True)
        N.drop_link(link, master, slave)      # the calculator program quit
        check(f"{name}'s Mac program stops with the link", wait_for_line(calcd.stdout, "ended (the calculator link closed"), True)
    master, slave = N.make_link(link)
    os.write(master, b"\x07\x07")
    check("unknown bytes are logged", wait_for_line(calcd.stdout, "unknown first bytes"), True)
    N.drop_link(link, master, slave)
finally:
    calcd.terminate()
print("all calcd tests passed" if not fails else f"{fails} failed")
sys.exit(1 if fails else 0)
