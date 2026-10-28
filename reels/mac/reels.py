#!/usr/bin/env python3
"""Plays video clips on the calculator (running REELS) and the sound on the Mac.

Usage:  python3 reels.py [folder or video files...] [--fps 15] [--shuffle] [--no-audio] [--fill]
Default folder: ../clips next to this script. Needs ffmpeg (brew install ffmpeg). No Python packages.

Keys on the calculator:  Down = next reel   Up = previous reel   Left = restart   Right = next
                         Enter = pause / play   Clear = quit
Close TI Connect CE first and start REELS on the calculator before running this."""
import argparse, calckeys, glob, os, random, select, signal, subprocess, sys, tempfile, termios, threading, time, tty

W, H = 160, 120
RAW_FRAME = W * H          # one byte per pixel from ffmpeg
PAL_BYTES = 256 * 4        # ffmpeg adds the 256-entry palette (blue, green, red, alpha) after each frame
VIDEO_EXT = (".mp4", ".mov", ".m4v", ".mkv", ".webm", ".avi")


def find_clips(args):
    paths = []
    for a in args or [os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "clips")]:
        if os.path.isdir(a):
            paths += [os.path.join(a, f) for f in sorted(os.listdir(a)) if f.lower().endswith(VIDEO_EXT)]
        elif os.path.isfile(a):
            paths.append(a)
    return paths


def encode(path, fps, fill=False):
    """-> (palette bytes: 16 x RGB, list of 9600-byte frames, 4 bits per pixel)."""
    shape = (f"scale={W}:{H}:force_original_aspect_ratio=increase,crop={W}:{H}" if fill else
             f"scale={W}:{H}:force_original_aspect_ratio=decrease,pad={W}:{H}:(ow-iw)/2:(oh-ih)/2:black")
    vf = (f"fps={fps},{shape},split[a][b];"
          "[a]palettegen=max_colors=16:stats_mode=full[p];[b][p]paletteuse=dither=bayer:bayer_scale=3")
    out = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-an", "-vf", vf, "-f", "rawvideo", "-pix_fmt", "pal8", "-"],
                         capture_output=True)
    data = out.stdout
    size = RAW_FRAME + PAL_BYTES
    if out.returncode != 0 or len(data) < size:
        raise RuntimeError(f"ffmpeg could not read {path}: {out.stderr.decode(errors='replace')[:200]}")
    pal = bytearray()
    for i in range(16):  # entries are blue, green, red, alpha
        b, g, r = data[RAW_FRAME + i * 4], data[RAW_FRAME + i * 4 + 1], data[RAW_FRAME + i * 4 + 2]
        pal += bytes((r, g, b))
    frames = []
    for o in range(0, len(data) - size + 1, size):
        idx = data[o:o + RAW_FRAME]
        frames.append(bytes(((h & 15) << 4) | (l & 15) for h, l in zip(idx[0::2], idx[1::2])))
    return bytes(pal), frames


def extract_audio(path):
    wav = tempfile.NamedTemporaryFile(suffix=".wav", delete=False).name
    r = subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", path, "-vn", "-ac", "2", "-ar", "44100", wav], capture_output=True)
    return wav if r.returncode == 0 and os.path.getsize(wav) > 1000 else None


class Clip:
    def __init__(self, path, fps, audio, fill):
        self.path, self.fps, self.audio_wanted, self.fill = path, fps, audio, fill
        self.ready = threading.Event()
        self.error = None
        threading.Thread(target=self._load, daemon=True).start()

    def _load(self):
        try:
            self.pal, self.frames = encode(self.path, self.fps, self.fill)
            self.wav = extract_audio(self.path) if self.audio_wanted else None
        except Exception as e:  # shown when the clip is played
            self.error = e
        self.ready.set()


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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="*")
    ap.add_argument("--fps", type=int, default=15)
    ap.add_argument("--shuffle", action="store_true")
    ap.add_argument("--no-audio", action="store_true")
    ap.add_argument("--fill", action="store_true", help="fill the screen and crop the edges (default: show the whole picture)")
    a = ap.parse_args()
    paths = find_clips(a.paths)
    if not paths:
        sys.exit("No videos found. Put some .mp4 files in the clips folder, or give file names.")
    if a.shuffle:
        random.shuffle(paths)
    ports = ([os.environ["CALC_PORT"]] if os.path.exists(os.environ["CALC_PORT"]) else []) if os.environ.get("CALC_PORT") else sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("No calculator found. Run REELS on it, plug in the cable, close TI Connect CE, then try again.")
    fd = os.open(ports[0], os.O_RDWR | os.O_NOCTTY)
    tty.setraw(fd)
    print(f"Connected to {ports[0]}. {len(paths)} clip(s). Down = next, Up = previous, Enter = pause, Clear = quit.")

    router = calckeys.Router()  # the calculator reports its keys; here they only choose reels
    cache = {}

    def clip(i):
        i %= len(paths)
        if i not in cache:
            cache[i] = Clip(paths[i], a.fps, not a.no_audio, a.fill)
        return cache[i]

    audio = None

    def stop_audio():
        nonlocal audio
        if audio and audio.poll() is None:
            audio.terminate()
        audio = None

    cur, quit_, paused = 0, False, False
    try:
        while not quit_:
            c = clip(cur)
            clip(cur + 1)  # start preparing the neighbours so scrolling is quick
            clip(cur - 1)
            print(f"[{cur + 1}/{len(paths)}] {os.path.basename(paths[cur % len(paths)])} ...", end="", flush=True)
            c.ready.wait()
            if c.error:
                print(" skipped:", c.error)
                cur += 1
                if len(cache) and all(x.ready.is_set() and x.error for x in cache.values()) and len(cache) >= len(paths):
                    sys.exit("None of the clips could be read.")
                continue
            print(f" {len(c.frames)} frames")
            send(fd, b"P" + c.pal)
            move = 0
            while move == 0 and not quit_:
                stop_audio()
                if c.wav and not paused:
                    audio = subprocess.Popen(["afplay", c.wav])
                t0, i = time.time(), 0
                while move == 0 and not quit_:
                    r, _, _ = select.select([fd], [], [], 0)
                    for k in router.feed(read_calc(fd, 256)) if r else []:
                        if k in "DR":
                            move = 1
                        elif k == "U":
                            move = -1
                        elif k == "L":
                            i, t0 = 0, time.time()
                            stop_audio()
                            if c.wav and not paused:
                                audio = subprocess.Popen(["afplay", c.wav])
                        elif k == "E":
                            paused = not paused
                            if audio and audio.poll() is None:
                                audio.send_signal(signal.SIGSTOP if paused else signal.SIGCONT)
                            t0 = time.time() - i / a.fps
                        elif k == "Q":
                            quit_ = True
                    if move or quit_:
                        break
                    if paused:
                        time.sleep(0.02)
                        t0 = time.time() - i / a.fps
                        continue
                    due = t0 + i / a.fps
                    now = time.time()
                    if now < due:
                        time.sleep(min(due - now, 0.004))
                        continue
                    i = max(i, int((now - t0) * a.fps))  # if sending fell behind, skip ahead to stay in step with the sound
                    if i >= len(c.frames):
                        break  # the clip ended: play it again
                    send(fd, b"F" + c.frames[i])
                    i += 1
            cur += move
    except OSError:
        print("The calculator disconnected (it quit REELS or the cable came out).")
    finally:
        stop_audio()
        os.close(fd)
    print("Done.")


if __name__ == "__main__":
    main()
