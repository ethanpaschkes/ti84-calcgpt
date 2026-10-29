#!/usr/bin/env python3
"""CalcGPT cloud server: many calculators' Wi-Fi boards log in here over the internet and chat with a cloud model.

  CALCGPT (calculator) <--USB--> bridge board <--TLS over Wi-Fi--> this server <--HTTPS--> OpenAI

Usage:  python3 cloud/server.py [--port 7979] [--host 0.0.0.0] [--devices cloud/devices.json] [--cert C --key K] [--proxy-protocol]
Needs no Python packages. Add a board first with  python3 cloud/add_device.py NAME  (see README.md).

The protocol is wireless/PROTOCOL.md, sections 1 and 2 (login `CALCNET2`):
  server -> board:  "CALCNET2" + 16 random bytes (the challenge)
  board -> server:  device id (16 ASCII bytes, padded with zero bytes) + HMAC-SHA256(key, challenge + "calc") (32 bytes)
                    where key = the 32 bytes the device secret's 64 hex characters stand for (as the board's firmware does)
  server -> board:  "OK" and the model list, then CalcGPT messages both ways; a failed login just gets the connection closed.

Each answer comes from the blocking OpenAI stream in calcgpt/mac/calcgptd.py, run in a worker thread, so many boards are served
at once. Only cloud (openai) models are offered: an "ollama" entry in models.json is skipped, so no local model is ever started.

Settings:
  OPENAI_API_KEY      from the environment, or cloud/.env (never commit it)
  CALCGPT_DEVICES     devices file (default cloud/devices.json); daily use is kept in usage.json next to it
  models              cloud/models.json if it exists, else calcgpt/mac/models.json

TLS: in production the host's proxy (Fly.io) does TLS and hands plain TCP to this program. --cert/--key serve TLS here instead.
--proxy-protocol: the proxy puts a PROXY protocol header (v1 or v2) in front of each connection, so the login limits apply to
the real board's address and not to the proxy's.

Logs one line per login and per question (the device's name, never the question text: students' questions stay private)."""
import argparse, asyncio, concurrent.futures, datetime, hashlib, hmac, ipaddress, json, os, secrets, socket, ssl, struct, sys, threading, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "calcgpt", "mac"))
import calcgptd as G  # noqa: E402  (the Mac-side CalcGPT: chat, cleaning, the OpenAI stream, the message framing)

HELLO = b"CALCNET2"
ID_LEN = 16
DEFAULT_PORT = 7979
DEFAULT_LIMIT = 200
DEFAULT_DEVICES = os.environ.get("CALCGPT_DEVICES") or os.path.join(HERE, "devices.json")
LOGIN_TIMEOUT = 5.0
WORKERS = 64            # answers being written at the same time (each waits on the network, so threads are cheap)
PROXY_V2 = b"\r\n\r\n\x00\r\nQUIT\n"


def answer_for(secret, challenge):
    """What the board must reply after its id: proof it knows its secret, without sending it."""
    return hmac.new(bytes.fromhex(secret), challenge + b"calc", hashlib.sha256).digest()


def log(*parts):
    print(time.strftime("%Y-%m-%d %H:%M:%S"), *parts, flush=True)


class Throttle:
    """Too many wrong logins from one address in a short time and its new connections are dropped without a hello
    (the same limits as calcnet: 5 within a minute). Other addresses are not affected."""
    def __init__(self, limit=5, window=60.0):
        self.limit, self.window, self.fails = limit, window, {}

    def _recent(self, addr, now):
        times = [t for t in self.fails.get(addr, []) if now - t < self.window]
        if times:
            self.fails[addr] = times
        else:
            self.fails.pop(addr, None)
        return times

    def blocked(self, addr, now=None):
        now = time.time() if now is None else now
        return len(self._recent(addr, now)) >= self.limit

    def fail(self, addr, now=None):
        now = time.time() if now is None else now
        if len(self.fails) > 10000:  # many addresses at once: forget the ones that went quiet
            for a in list(self.fails):
                self._recent(a, now)
        self.fails[addr] = self._recent(addr, now) + [now]


def write_private_json(path, data):
    """Saves JSON readable only by its owner, replacing the old file in one step (never half-written)."""
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    tmp = f"{path}.{os.getpid()}.tmp"
    fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w") as f:
        json.dump(data, f, indent=2)
        f.write("\n")
    os.replace(tmp, path)


def read_devices(path):
    """The devices file as a dict (empty if there is none yet)."""
    try:
        data = json.load(open(path, encoding="utf-8"))
    except FileNotFoundError:
        return {}
    if not isinstance(data, dict):
        raise ValueError(f"{path} must hold a JSON object of devices")
    return data


def utc_day(now=None):
    return datetime.datetime.fromtimestamp(time.time() if now is None else now, datetime.timezone.utc).strftime("%Y-%m-%d")


class Devices:
    """The boards allowed in, and how many questions each asked today (UTC). The devices file is read again whenever it
    changes, so add_device.py and --disable work without restarting the server. Usage is kept in usage.json next to it."""
    def __init__(self, path, usage_path=None):
        self.path = path
        self.usage_path = usage_path or os.path.join(os.path.dirname(os.path.abspath(path)), "usage.json")
        self.stamp, self.devices = None, {}
        try:
            self.usage = json.load(open(self.usage_path, encoding="utf-8"))
            assert isinstance(self.usage.get("counts"), dict)
        except (OSError, ValueError, AssertionError, AttributeError):
            self.usage = {"day": utc_day(), "counts": {}}

    def _fresh(self):
        try:
            st = os.stat(self.path)
            stamp = (st.st_mtime_ns, st.st_size)
        except FileNotFoundError:
            stamp = None
        if stamp != self.stamp:
            try:
                self.devices = read_devices(self.path)
            except ValueError as e:
                log("devices file problem, keeping the last good one:", e)
                return self.devices
            self.stamp = stamp
        return self.devices

    def find(self, device_id):
        """The entry for an enabled device with a usable secret, or None."""
        d = self._fresh().get(device_id)
        if not isinstance(d, dict) or not d.get("enabled", True) or not isinstance(d.get("secret"), str) or len(d["secret"]) < 32:
            return None
        try:
            bytes.fromhex(d["secret"])
        except ValueError:
            return None
        return d

    def limit(self, device_id):
        d = self._fresh().get(device_id) or {}
        return int(d.get("daily_limit", DEFAULT_LIMIT))

    def used(self, device_id, now=None):
        if self.usage.get("day") != utc_day(now):
            self.usage = {"day": utc_day(now), "counts": {}}
        return self.usage["counts"].get(device_id, 0)

    def take(self, device_id, now=None):
        """Counts one question. False (and nothing counted) when the device has reached its daily limit."""
        n = self.used(device_id, now)
        if n >= self.limit(device_id):
            return False
        self.usage["counts"][device_id] = n + 1
        try:
            write_private_json(self.usage_path, self.usage)
        except OSError as e:
            log("could not save usage:", e)
        return True


async def read_proxy_header(reader):
    """The client's real address from a PROXY protocol header (v1 text or v2 binary). None if there is no usable one."""
    first = await reader.readexactly(12)
    if first == PROXY_V2:
        ver_cmd, fam, length = struct.unpack("!BBH", await reader.readexactly(4))
        body = await reader.readexactly(length)
        if ver_cmd >> 4 != 2:
            return None
        if ver_cmd & 0xF == 0:      # LOCAL: the proxy's own check, no client
            return None
        if fam >> 4 == 1 and len(body) >= 12:
            return str(ipaddress.IPv4Address(body[:4]))
        if fam >> 4 == 2 and len(body) >= 36:
            return str(ipaddress.IPv6Address(body[:16]))
        return None
    if first.startswith(b"PROXY "):
        line = first + await reader.readuntil(b"\r\n")
        if len(line) > 108:
            return None
        parts = line.decode("ascii", "replace").split()
        return parts[2] if len(parts) >= 6 and parts[1] in ("TCP4", "TCP6") else None
    return None


def tune(writer):
    """Quick delivery, and a dead peer noticed within about 30 s even when nothing is being sent."""
    sock = writer.get_extra_info("socket")
    if sock is None:
        return
    try:
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    except OSError:
        return
    for name, value in (("TCP_KEEPALIVE", 10), ("TCP_KEEPIDLE", 10), ("TCP_KEEPINTVL", 5), ("TCP_KEEPCNT", 4)):
        if hasattr(socket, name):
            try:
                sock.setsockopt(socket.IPPROTO_TCP, getattr(socket, name), value)
            except OSError:
                pass


class Server:
    """Logs boards in and runs one CalcGPT chat per connection. No sockets are opened here, so it can be tested."""
    def __init__(self, devices, models, api_key, cloud_stream=None, throttle=None, proxy_protocol=False,
                 login_timeout=LOGIN_TIMEOUT, workers=WORKERS):
        self.devices, self.models, self.api_key = devices, models, api_key
        self.cloud_stream = cloud_stream          # None: the real OpenAI stream
        self.throttle = throttle or Throttle()
        self.proxy_protocol, self.login_timeout = proxy_protocol, login_timeout
        self.pool = concurrent.futures.ThreadPoolExecutor(max_workers=workers, thread_name_prefix="answer")
        self.active = {}                          # device id -> writer of its current connection

    async def handle(self, reader, writer):
        addr = (writer.get_extra_info("peername") or ("?",))[0]
        try:
            if self.proxy_protocol:
                try:
                    real = await asyncio.wait_for(read_proxy_header(reader), self.login_timeout)
                except (asyncio.IncompleteReadError, asyncio.LimitOverrunError, asyncio.TimeoutError, ValueError, OSError):
                    return
                if real is None:
                    return
                addr = real
            if self.throttle.blocked(addr):
                return
            tune(writer)
            device_id = await self.login(reader, writer, addr)
            if device_id is None:
                return
            old = self.active.get(device_id)
            if old is not None:  # a board that restarted leaves a dead connection behind: the new login replaces it
                old.close()
            self.active[device_id] = writer
            name = self.devices.find(device_id).get("name", device_id)
            log(f"login: {name} from {addr}")
            try:
                await self.chat(reader, writer, device_id, name)
            finally:
                if self.active.get(device_id) is writer:
                    del self.active[device_id]
                log(f"logout: {name}")
        except (ConnectionError, OSError):
            pass
        finally:
            writer.close()

    async def login(self, reader, writer, addr):
        """The device id if the board proved it knows its secret; otherwise None (and the failure is counted)."""
        challenge = secrets.token_bytes(16)
        try:
            writer.write(HELLO + challenge)
            await writer.drain()
            reply = await asyncio.wait_for(reader.readexactly(ID_LEN + 32), self.login_timeout)
        except (asyncio.IncompleteReadError, asyncio.TimeoutError, OSError):
            self.throttle.fail(addr)
            log(f"refused a connection from {addr} (no login)")
            return None
        raw_id, proof = reply[:ID_LEN], reply[ID_LEN:]
        device_id = raw_id.rstrip(b"\0").decode("ascii", "replace")
        entry = self.devices.find(device_id)
        # compare against something even for an unknown id, so the time taken does not tell which ids exist
        expected = answer_for(entry["secret"] if entry else "0" * 64, challenge)
        if not hmac.compare_digest(proof, expected) or entry is None:
            self.throttle.fail(addr)
            log(f"refused a connection from {addr} (unknown or switched-off device, or wrong secret)")
            return None
        writer.write(b"OK")
        await writer.drain()
        return device_id

    async def chat(self, reader, writer, device_id, name):
        """One CalcGPT conversation, handled the way calcgptd.main() does it over the cable."""
        loop = asyncio.get_running_loop()
        chat = G.Chat(models=self.models, cloud_stream=self.cloud_stream, api_key=self.api_key)
        stream, stop, worker = G.CalcStream(), threading.Event(), None
        lock = asyncio.Lock()

        async def send(*msgs):
            async with lock:
                for m in msgs:
                    writer.write(m)
                await writer.drain()

        async def answer(question):
            queue = asyncio.Queue()

            def work():
                try:
                    for m in chat.reply(question, stop.is_set):
                        loop.call_soon_threadsafe(queue.put_nowait, m)
                except Exception as e:  # never leave the calculator waiting for its D
                    log(f"answer for {name} failed: {type(e).__name__}")
                    loop.call_soon_threadsafe(queue.put_nowait, G.encode_text(b"T", "Server problem, try again"))
                    loop.call_soon_threadsafe(queue.put_nowait, b"D")
                finally:
                    loop.call_soon_threadsafe(queue.put_nowait, None)

            job = loop.run_in_executor(self.pool, work)
            try:
                while (m := await queue.get()) is not None:
                    await send(m)
            except (ConnectionError, OSError):
                stop.set()
            await job

        def busy():
            return worker is not None and not worker.done()

        await send(*chat.listing())
        try:
            while True:
                data = await reader.read(1024)
                if not data:
                    return
                for cmd, arg in stream.feed(data):
                    if cmd == "Z":
                        return
                    if cmd == "S":
                        stop.set()
                    elif cmd == "N":
                        stop.set()
                        chat.new()
                    elif cmd == "R":
                        await send(*chat.listing())
                    elif cmd == "M":
                        if not busy():  # not while an answer is being written: the listing says what is still in use
                            chat.select(arg)
                        await send(*chat.listing())
                    elif cmd == "Q" and arg.strip() and not busy():
                        if self.devices.find(device_id) is None:  # switched off (or removed) while connected
                            log(f"{name} was switched off: closing")
                            return
                        if not self.devices.take(device_id):
                            log(f"question from {name}: daily limit reached")
                            await send(G.encode_text(b"T", "Daily limit reached"), b"D")
                            continue
                        log(f"question from {name} ({self.devices.used(device_id)}/{self.devices.limit(device_id)} today, "
                            f"{chat.models[chat.selected]['label']})")
                        stop.clear()
                        worker = asyncio.ensure_future(answer(arg.strip()))
        finally:
            stop.set()
            if worker is not None:
                try:
                    await worker
                except (ConnectionError, OSError):
                    pass


def cloud_models(path=None):
    """The cloud models to offer (an ollama entry is skipped: the server never runs a local model) and notes about skipped ones."""
    if path is None:
        own = os.path.join(HERE, "models.json")
        path = own if os.path.exists(own) else os.path.join(ROOT, "calcgpt", "mac", "models.json")
    models, problems = G.load_models(path)
    skipped = [m["label"] for m in models if m["provider"] != "openai"]
    return [m for m in models if m["provider"] == "openai"], problems + [f"skipped local model {s!r} (cloud only here)" for s in skipped]


def tls_context(cert, key):
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.minimum_version = ssl.TLSVersion.TLSv1_2
    ctx.load_cert_chain(cert, key)
    return ctx


async def serve(server, host, port, tls=None, ready=None):
    """Listens until cancelled. ready(port) is called once it is listening (port 0 picks a free one)."""
    listener = await asyncio.start_server(server.handle, host, port, ssl=tls, limit=4096)
    if ready:
        ready(listener.sockets[0].getsockname()[1])
    async with listener:
        await listener.serve_forever()


def main():
    ap = argparse.ArgumentParser(description="CalcGPT cloud server (CALCNET2 login, cloud models only)")
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=int(os.environ.get("PORT", DEFAULT_PORT)))
    ap.add_argument("--devices", default=DEFAULT_DEVICES, help="the devices file (default %(default)s)")
    ap.add_argument("--models", help="models file (default cloud/models.json, else calcgpt/mac/models.json)")
    ap.add_argument("--cert", help="serve TLS here with this certificate (PEM); needs --key")
    ap.add_argument("--key", help="the certificate's private key (PEM)")
    ap.add_argument("--proxy-protocol", action="store_true", default=os.environ.get("CALCGPT_PROXY_PROTOCOL") == "1",
                    help="expect a PROXY protocol header first (set by the host's proxy; also env CALCGPT_PROXY_PROTOCOL=1)")
    a = ap.parse_args()
    if bool(a.cert) != bool(a.key):
        sys.exit("--cert and --key go together.")
    key = os.environ.get("OPENAI_API_KEY") or G.load_env(os.path.join(HERE, ".env")).get("OPENAI_API_KEY")
    if not key:
        sys.exit("No OpenAI key: set OPENAI_API_KEY, or put OPENAI_API_KEY=... in cloud/.env (never commit it).")
    models, notes = cloud_models(a.models)
    for n in notes:
        print("note:", n)
    if not models:
        sys.exit("No cloud models to offer: add an openai entry to models.json.")
    devices = Devices(a.devices)
    count = len(devices._fresh())
    server = Server(devices, models, key, proxy_protocol=a.proxy_protocol)
    log(f"CalcGPT cloud server on port {a.port}{' (TLS)' if a.cert else ''}{' behind a PROXY-protocol proxy' if a.proxy_protocol else ''}; "
        f"{count} device{'s' if count != 1 else ''} in {a.devices}; models: {', '.join(m['label'] for m in models)}")
    if not count:
        print("note: no devices yet. Add one with:  python3 cloud/add_device.py NAME")
    try:
        asyncio.run(serve(server, a.host, a.port, tls_context(a.cert, a.key) if a.cert else None))
    except KeyboardInterrupt:
        print("stopped")


if __name__ == "__main__":
    main()
