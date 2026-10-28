#!/usr/bin/env python3
"""Shows your Mac's screen on the calculator (running REELS), live.

Usage:  python3 screenshare.py [--view follow|whole] [--mode color|gray|mono] [--fps N] [--dither] [--test-file video.mp4]
Needs ffmpeg (brew install ffmpeg) and no Python packages. Close TI Connect CE first, start REELS on the calculator,
then run this. The first time, macOS asks to let the Terminal record the screen (System Settings > Privacy & Security >
Screen Recording); allow it and run this again.

Views:
  follow  (default) a window of your screen around the mouse pointer, at the calculator's own resolution, so text can be
          read. The window stays still until the pointer nears its edge, then it scrolls to keep up.
  whole   the entire screen shrunk to fit (good for layout and video, text is too small to read).

Modes:
  color  160x120, 16 colours, up to 20 frames/s
  gray   320x240, 4 grays,    up to 12 frames/s
  mono   320x240, black and white, up to 20 frames/s
Only the rows of the picture that changed are sent, so text, typing and a moving pointer use very little of the cable;
video and other full-screen changes are limited by the cable (about 6 to 10 frames/s).

Keys on the calculator:  Left / Right = change mode
                         follow view: Up = zoom in, Down = zoom out     whole view: Up / Down = pick fresh colours
                         Enter = pause / play         Clear = quit
--dither makes pictures smoother but text blurrier (the follow view is crisp by default)."""
import argparse, calckeys, ctypes, ctypes.util, glob, os, re, select, struct, subprocess, sys, tempfile, threading, time, tty, zlib

MODES = ["color", "gray", "mono"]
DEFAULT_FPS = {"color": 20, "gray": 12, "mono": 20}
PAL_TRAILER = 256 * 4  # ffmpeg appends a 256-entry palette (blue, green, red, alpha) to every 8-bit frame


class CGPoint(ctypes.Structure):
    _fields_ = [("x", ctypes.c_double), ("y", ctypes.c_double)]


class CGSize(ctypes.Structure):
    _fields_ = [("width", ctypes.c_double), ("height", ctypes.c_double)]


class CGRect(ctypes.Structure):
    _fields_ = [("origin", CGPoint), ("size", CGSize)]


class Screen:
    """Reads the pointer position and grabs a rectangle of the main display straight from macOS (no ffmpeg needed)."""
    def __init__(self):
        cg = self.cg = ctypes.CDLL(ctypes.util.find_library("CoreGraphics"))
        cf = self.cf = ctypes.CDLL(ctypes.util.find_library("CoreFoundation"))
        cg.CGMainDisplayID.restype = ctypes.c_uint32
        cg.CGDisplayBounds.argtypes = [ctypes.c_uint32]
        cg.CGDisplayBounds.restype = CGRect
        cg.CGEventCreate.argtypes = [ctypes.c_void_p]
        cg.CGEventCreate.restype = ctypes.c_void_p
        cg.CGEventGetLocation.argtypes = [ctypes.c_void_p]
        cg.CGEventGetLocation.restype = CGPoint
        cg.CGDisplayCreateImageForRect.argtypes = [ctypes.c_uint32, CGRect]
        cg.CGDisplayCreateImageForRect.restype = ctypes.c_void_p
        for name in ("CGImageGetWidth", "CGImageGetHeight", "CGImageGetBytesPerRow"):
            fn = getattr(cg, name)
            fn.argtypes = [ctypes.c_void_p]
            fn.restype = ctypes.c_size_t
        cg.CGImageGetDataProvider.argtypes = [ctypes.c_void_p]
        cg.CGImageGetDataProvider.restype = ctypes.c_void_p
        cg.CGDataProviderCopyData.argtypes = [ctypes.c_void_p]
        cg.CGDataProviderCopyData.restype = ctypes.c_void_p
        cf.CFDataGetLength.argtypes = [ctypes.c_void_p]
        cf.CFDataGetLength.restype = ctypes.c_long
        cf.CFDataGetBytePtr.argtypes = [ctypes.c_void_p]
        cf.CFDataGetBytePtr.restype = ctypes.c_void_p
        cf.CFRelease.argtypes = [ctypes.c_void_p]
        self.display = cg.CGMainDisplayID()
        b = cg.CGDisplayBounds(self.display)
        self.width, self.height = b.size.width, b.size.height  # in points

    def pointer(self):
        e = self.cg.CGEventCreate(None)
        p = self.cg.CGEventGetLocation(e)
        self.cf.CFRelease(e)
        return p.x, p.y

    def grab(self, x, y, w, h):
        """-> (pixel width, pixel height, bytes) in BGRA order, or None if macOS refuses (no screen recording permission)."""
        img = self.cg.CGDisplayCreateImageForRect(self.display, CGRect(CGPoint(x, y), CGSize(w, h)))
        if not img:
            return None
        pw, ph = self.cg.CGImageGetWidth(img), self.cg.CGImageGetHeight(img)
        stride = self.cg.CGImageGetBytesPerRow(img)
        data = self.cg.CGDataProviderCopyData(self.cg.CGImageGetDataProvider(img))
        raw = ctypes.string_at(self.cf.CFDataGetBytePtr(data), self.cf.CFDataGetLength(data))
        self.cf.CFRelease(data)
        self.cf.CFRelease(img)
        if stride != pw * 4:  # rows are padded: cut the padding off
            raw = b"".join(raw[r * stride:r * stride + pw * 4] for r in range(ph))
        return pw, ph, raw


ZOOMS = [160, 240, 320, 480, 640, 960]  # width in screen points of the window that is shown (always 4:3); smaller = more zoom
DEFAULT_ZOOM = 2


def place_window(x, y, w, h, px, py, sw, sh):
    """Moves the window only when the pointer gets within a fifth of an edge, so the picture stays still while you read."""
    mx, my = w * 0.2, h * 0.2
    if px < x + mx:
        x = px - mx
    elif px > x + w - mx:
        x = px - w + mx
    if py < y + my:
        y = py - my
    elif py > y + h - my:
        y = py - h + my
    return min(max(x, 0), sw - w), min(max(y, 0), sh - h)


def png_rgb(path, w, h, pixels):
    """Writes a tiny PNG; pixels is a list of (r, g, b) rows-first."""
    raw = b"".join(b"\x00" + bytes(c for px in pixels[y * w:(y + 1) * w] for c in px) for y in range(h))

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


PERMISSION_HELP = ("ffmpeg got no picture from the screen. macOS has probably not allowed screen recording for the app "
                   "this is running in.\nOpen System Settings > Privacy & Security > Screen & System Audio Recording, "
                   "turn on the app that holds this terminal\n(Terminal, or the Claude app if you run it from there), "
                   "quit and reopen that app, then run this again.")


def find_screen(index):
    out = subprocess.run(["ffmpeg", "-hide_banner", "-f", "avfoundation", "-list_devices", "true", "-i", ""],
                         capture_output=True, text=True).stderr
    screens = re.findall(r"\[(\d+)\] Capture screen (\d+)", out)
    if not screens:
        sys.exit("ffmpeg can not see a screen to capture. Check Screen Recording permission for the Terminal.")
    for dev, n in screens:
        if int(n) == index:
            return dev
    sys.exit(f"No screen number {index}. Screens found: {[n for _, n in screens]}")


def source_args(a, fps, seconds=None):
    """The ffmpeg input: the screen, or a video file played over and over. seconds limits how long it is read."""
    limit = ["-t", str(seconds)] if seconds else []
    if a.test_file:
        return ["-re", "-stream_loop", "-1"] + limit + ["-i", a.test_file]
    return ["-f", "avfoundation", "-pixel_format", "bgr0", "-framerate", str(max(fps, 5)), "-capture_cursor", "1"] + limit + ["-i", f"{find_screen(a.screen)}:none"]


def fit(w, h):
    return (f"scale={w}:{h}:flags=area:force_original_aspect_ratio=decrease,"
            f"pad={w}:{h}:(ow-iw)/2:(oh-ih)/2:black")


def make_color_palette(a, path):
    """Looks at the screen for a moment and keeps the 16 colours it uses most."""
    cmd = ["ffmpeg", "-v", "error", "-y"] + source_args(a, 10, 1) + ["-vf",
           fit(160, 120) + ",palettegen=max_colors=16:stats_mode=diff", "-update", "1", "-frames:v", "1", path]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
    except subprocess.TimeoutExpired:
        sys.exit(PERMISSION_HELP)
    if r.returncode != 0:
        sys.exit("Could not capture the screen: " + r.stderr[:300])


def gray_palette_image(path):
    """ffmpeg wants exactly 256 colours: the 4 grays first, then magenta that a gray picture never picks."""
    png_rgb(path, 16, 16, [(0, 0, 0), (85, 85, 85), (170, 170, 170), (255, 255, 255)] + [(255, 0, 255)] * 252)


def start_ffmpeg(mode, a, fps, palette_path):
    """Whole-screen view: ffmpeg captures the screen itself."""
    base = ["ffmpeg", "-v", "error"] + source_args(a, fps)
    if mode == "mono":
        vf = f"fps={fps}," + fit(320, 240) + ",format=gray,format=monob"
        return subprocess.Popen(base + ["-vf", vf, "-sws_dither", "ed", "-f", "rawvideo", "-pix_fmt", "monob", "-"],
                                stdout=subprocess.PIPE), 9600
    w, h = (160, 120) if mode == "color" else (320, 240)
    to_gray = ",format=gray,format=bgra" if mode == "gray" else ""  # a gray palette can only match gray pixels
    flt = f"[0:v]fps={fps},{fit(w, h)}{to_gray}[v];[v][1:v]paletteuse=dither=bayer:bayer_scale=4"
    return subprocess.Popen(base + ["-i", palette_path, "-filter_complex", flt, "-f", "rawvideo", "-pix_fmt", "pal8", "-"],
                            stdout=subprocess.PIPE), w * h + PAL_TRAILER


def draw_pointer(raw, pw, ph, cx, cy, size):
    """Marks the pointer (macOS leaves it out of screen grabs): a dark block with a light edge, in the BGRA bytes."""
    out = bytearray(raw)
    for dy in range(-size - 1, size + 2):
        for dx in range(-size - 1, size + 2):
            x, y = cx + dx, cy + dy
            if 0 <= x < pw and 0 <= y < ph:
                v = 0 if abs(dx) <= size and abs(dy) <= size else 255
                o = (y * pw + x) * 4
                out[o:o + 3] = bytes((v, v, v))
    return bytes(out)


class FollowSource:
    """A window of the screen that follows the mouse pointer, fed to ffmpeg as raw pictures."""
    def __init__(self, screen, mode, zoom, fps, crisp, tmp, gray_path):
        self.screen, self.mode, self.zoom, self.fps, self.crisp = screen, mode, zoom, fps, crisp
        self.tmp, self.gray_path = tmp, gray_path
        self.stop_flag, self.error, self.proc = False, None, None

    def start(self):
        sc = self.screen
        self.rw = float(ZOOMS[self.zoom])
        self.rh = self.rw * 3 / 4
        px, py = sc.pointer()
        self.x = min(max(px - self.rw / 2, 0), sc.width - self.rw)
        self.y = min(max(py - self.rh / 2, 0), sc.height - self.rh)
        first = sc.grab(self.x, self.y, self.rw, self.rh)
        if first is None:
            sys.exit(PERMISSION_HELP)
        self.pw, self.ph, raw = first
        self.scale = self.pw / self.rw
        in_args = ["-f", "rawvideo", "-pix_fmt", "bgra", "-s", f"{self.pw}x{self.ph}", "-framerate", str(self.fps), "-i", "-"]
        base = ["ffmpeg", "-v", "error"] + in_args
        w, h = (160, 120) if self.mode == "color" else (320, 240)
        resize = f"scale={w}:{h}:flags=area"
        if self.mode == "mono":
            threshold = ",lut=y='if(gt(val,128),255,0)'" if self.crisp else ""
            cmd = base + ["-vf", resize + ",format=gray" + threshold + ",format=monob"] + \
                  ([] if self.crisp else ["-sws_dither", "ed"]) + ["-f", "rawvideo", "-pix_fmt", "monob", "-"]
            self.size = 9600
        else:
            dither = "none" if self.crisp else "bayer:bayer_scale=4"
            if self.mode == "color":
                pal = os.path.join(self.tmp, "follow_color.png")
                rawfile = os.path.join(self.tmp, "sample.bgra")
                with open(rawfile, "wb") as f:
                    f.write(raw)
                r = subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "bgra", "-s", f"{self.pw}x{self.ph}",
                                    "-i", rawfile, "-vf", resize + ",palettegen=max_colors=16:stats_mode=full",
                                    "-update", "1", "-frames:v", "1", pal], capture_output=True, text=True)
                if r.returncode != 0:
                    sys.exit("Could not make the colour palette: " + r.stderr[:300])
            else:
                pal = self.gray_path
            to_gray = ",format=gray,format=bgra" if self.mode == "gray" else ""
            cmd = base + ["-i", pal, "-filter_complex", f"[0:v]{resize}{to_gray}[v];[v][1:v]paletteuse=dither={dither}",
                          "-f", "rawvideo", "-pix_fmt", "pal8", "-"]
            self.size = w * h + PAL_TRAILER
        self.proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        threading.Thread(target=self._capture, args=(raw,), daemon=True).start()
        return self.proc, self.size

    def _capture(self, first_raw):
        sc, period = self.screen, 1.0 / self.fps
        nxt, raw = time.time(), first_raw
        try:
            while not self.stop_flag:
                px, py = sc.pointer()
                if raw is None:
                    self.x, self.y = place_window(self.x, self.y, self.rw, self.rh, px, py, sc.width, sc.height)
                    g = sc.grab(self.x, self.y, self.rw, self.rh)
                    if g is None:
                        self.error = PERMISSION_HELP
                        return
                    if (g[0], g[1]) != (self.pw, self.ph):
                        continue  # the display changed size under us: skip this one
                    raw = g[2]
                cx, cy = int((px - self.x) * self.scale), int((py - self.y) * self.scale)
                self.proc.stdin.write(draw_pointer(raw, self.pw, self.ph, cx, cy, max(1, int(self.scale * 2))))
                self.proc.stdin.flush()
                raw = None
                nxt += period
                time.sleep(max(0.0, nxt - time.time()))
        except (BrokenPipeError, ValueError, OSError):
            pass

    def stop(self):
        self.stop_flag = True
        self.proc.kill()  # first, so ffmpeg is gone before the input closes and never sees a half-sent picture
        try:
            self.proc.stdin.close()
        except OSError:
            pass
        self.proc.wait()


GRAY_PALETTE = bytes([0, 0, 0, 85, 85, 85, 170, 170, 170, 255, 255, 255] + [0] * 36)
ROW_BYTES = {"mono": 40, "gray": 80, "color": 80}   # bytes in one packed row of the picture
ROW_COUNT = {"mono": 240, "gray": 240, "color": 120}
FULL_LETTER = {"mono": b"M", "gray": b"G", "color": b"F"}
ROWS_LETTER = {"mono": b"m", "gray": b"g", "color": b"f"}


def pack_frame(mode, raw):
    """-> (the picture packed for the calculator, 48-byte palette or None)"""
    if mode == "mono":
        return raw, None
    w, h = (160, 120) if mode == "color" else (320, 240)
    idx = raw[:w * h]
    if mode == "gray":
        # ffmpeg numbers the palette its own way, so go by the colour each index really is: 0 to 3 for black to white
        table = bytes(min(3, (raw[w * h + i * 4] + 42) // 85) for i in range(256))
        idx = idx.translate(table)
        return bytes(((p & 3) << 6) | ((q & 3) << 4) | ((r & 3) << 2) | (s & 3)
                     for p, q, r, s in zip(idx[0::4], idx[1::4], idx[2::4], idx[3::4])), GRAY_PALETTE
    pal = bytearray()
    for i in range(16):
        o = w * h + i * 4
        pal += bytes((raw[o + 2], raw[o + 1], raw[o]))  # stored blue, green, red -> red, green, blue
    return bytes(((x & 15) << 4) | (y & 15) for x, y in zip(idx[0::2], idx[1::2])), bytes(pal)


def picture_messages(mode, prev, cur):
    """The bytes that make the calculator show `cur`: only the rows that changed since `prev`, or the whole picture when
    that is not shorter (or when there is no previous picture). Nothing at all if nothing changed."""
    rb, n = ROW_BYTES[mode], ROW_COUNT[mode]
    full = FULL_LETTER[mode] + cur
    if prev is None:
        return full
    changed = [cur[y * rb:(y + 1) * rb] != prev[y * rb:(y + 1) * rb] for y in range(n)]
    if not any(changed):
        return b""
    out, y = [], 0
    while y < n:
        if not changed[y]:
            y += 1
            continue
        start = y
        while y < n and changed[y]:
            y += 1
        out.append(ROWS_LETTER[mode] + bytes((start, y - start)) + cur[start * rb:y * rb])
    msg = b"".join(out)
    return msg if len(msg) < len(full) else full


def read_calc(fd, n):
    """Reads from the calculator link. A closed link reads as empty bytes (a pulled cable raises an error instead): both mean gone."""
    data = os.read(fd, n)
    if not data:
        raise OSError("the calculator disconnected")
    return data


def send(fd, data):
    view = memoryview(data)
    while view:
        view = view[os.write(fd, view):]


class Reader(threading.Thread):
    """Reads frames from ffmpeg as fast as they come and keeps only the newest, so the picture stays live."""
    def __init__(self, proc, size):
        super().__init__(daemon=True)
        self.proc, self.size, self.latest, self.n, self.alive = proc, size, None, 0, True

    def run(self):
        while True:
            data = self.proc.stdout.read(self.size)
            if len(data) < self.size:
                self.alive = False
                return
            self.latest, self.n = data, self.n + 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--view", choices=["follow", "whole"], default="follow")
    ap.add_argument("--mode", choices=MODES)
    ap.add_argument("--fps", type=int)
    ap.add_argument("--dither", action="store_true", help="smoother pictures, blurrier text")
    ap.add_argument("--zoom", type=int, default=DEFAULT_ZOOM, choices=range(len(ZOOMS)), help="starting zoom, 0 (most zoomed in) to 5")
    ap.add_argument("--screen", type=int, default=0, help="whole view only: which screen")
    ap.add_argument("--control", action="store_true",
                    help="let the calculator control this Mac's mouse and keyboard (Graph key: View, Mouse, Type)")
    ap.add_argument("--test-file", help="whole view: stream a video file instead of the screen (for testing)")
    a = ap.parse_args()
    follow = a.view == "follow"
    ports = ([os.environ["CALC_PORT"]] if os.path.exists(os.environ["CALC_PORT"]) else []) if os.environ.get("CALC_PORT") else sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("No calculator found. Run REELS on it, plug in the cable, close TI Connect CE, then try again.")
    fd = os.open(ports[0], os.O_RDWR | os.O_NOCTTY)
    tty.setraw(fd)
    tmp = tempfile.mkdtemp()
    pal_path, gray_path = os.path.join(tmp, "color.png"), os.path.join(tmp, "gray.png")
    gray_palette_image(gray_path)
    screen = Screen() if follow or a.control else None
    injector = None
    if a.control:
        injector = calckeys.Injector(screen)
        if not injector.trusted():
            sys.exit("macOS has not allowed this app to control the computer. Open System Settings > Privacy & Security > "
                     "Accessibility,\nturn on the app you run this in (Terminal, or the Claude app), quit and reopen it, then try again.")
        print("CONTROL IS ON. On the calculator, Graph cycles VIEW, MOUSE, TYPE. Only MOUSE and TYPE act on this Mac. Clear quits.")

    def send_badge(text):
        send(fd, b"I" + bytes([len(text)]) + text.encode("ascii"))

    router = calckeys.Router(injector, send_badge)
    mode = a.mode or ("mono" if follow else "color")
    zoom, paused, quit_ = a.zoom, False, False
    print(f"Connected to {ports[0]}. View {a.view}, mode {mode}. Left/Right = mode, "
          + ("Up/Down = zoom, " if follow else "Up/Down = new colours, ") + "Enter = pause, Clear = quit.")
    try:
        while not quit_:
            fps = a.fps or DEFAULT_FPS[mode]
            if follow:
                source = FollowSource(screen, mode, zoom, fps, not a.dither, tmp, gray_path)
                proc, size = source.start()
                print(f"  window {ZOOMS[zoom]} x {ZOOMS[zoom] * 3 // 4} screen points")
            else:
                if mode == "color":
                    make_color_palette(a, pal_path)
                proc, size = start_ffmpeg(mode, a, fps, pal_path if mode == "color" else gray_path)
            reader = Reader(proc, size)
            reader.start()
            if mode == "mono":
                send(fd, b"P" + bytes([0, 0, 0, 255, 255, 255] + [0] * 42))
            last_n, last_packed, palette_sent, restart = 0, None, mode == "mono", False
            t_stat, sent_frames, t_begin = time.time(), 0, time.time()
            while not quit_ and not restart and reader.alive:
                if reader.n == 0 and time.time() - t_begin > 10:
                    proc.kill()
                    sys.exit(PERMISSION_HELP)
                r, _, _ = select.select([fd], [], [], 0.01)
                commands = router.feed(read_calc(fd, 256)) if r else []
                router.tick()
                for k in commands:
                    if k in "LR":
                        mode = MODES[(MODES.index(mode) + (1 if k == "R" else -1)) % 3]
                        restart = True
                    elif k in "UD" and follow:
                        new = max(0, zoom - 1) if k == "U" else min(len(ZOOMS) - 1, zoom + 1)
                        restart = restart or new != zoom
                        zoom = new
                    elif k in "UD" and mode == "color":
                        restart = True
                    elif k == "E":
                        paused = not paused
                    elif k == "Q":
                        quit_ = True
                if restart or quit_ or paused or reader.n == last_n:
                    continue
                last_n = reader.n
                packed, pal = pack_frame(mode, reader.latest)
                if pal is not None and not palette_sent:
                    send(fd, b"P" + pal)
                    palette_sent = True
                update = picture_messages(mode, last_packed, packed)
                if not update:
                    continue  # nothing changed on screen
                send(fd, update)
                last_packed = packed
                sent_frames += 1
                if time.time() - t_stat > 5:
                    print(f"  {mode}: {sent_frames / (time.time() - t_stat):.1f} frames/s sent")
                    t_stat, sent_frames = time.time(), 0
            if follow:
                if source.error:
                    sys.exit(source.error)
                source.stop()
            else:
                proc.kill()
                proc.wait()
            if not reader.alive and not restart and not quit_:
                sys.exit("ffmpeg stopped. If this is the first run, allow screen recording for the app you run this in.")
    except OSError:
        print("The calculator disconnected (it quit REELS or the cable came out).")
    finally:
        router.close()
        os.close(fd)
    print("Done.")


if __name__ == "__main__":
    main()
