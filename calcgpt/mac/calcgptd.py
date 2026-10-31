#!/usr/bin/env python3
"""CalcGPT: the Mac side of an AI chat on the TI-84 Plus CE. You type a question on the calculator (running CALCGPT); a model
answers, and the reply streams onto the calculator screen as it is written. It is a general assistant (school, maths, writing,
coding, everyday questions). The Trace key on the calculator picks the model.

Usage:  python3 calcgptd.py [--model NAME] [--keep-alive 10m] [--ignore-render] [--quiet]
        python3 calcgptd.py --list-cloud       shows the model names your cloud account offers
Needs no Python packages. Close TI Connect CE first, start CALCGPT on the calculator, then run this.

Models come from models.json next to this file (made with defaults if it is missing):
  {"label": "Local 27B", "provider": "ollama", "model": "qwen3.8:27b"}     runs on this Mac through Ollama; nothing leaves the Mac
  {"label": "GPT fast", "provider": "openai", "model": "gpt-5-mini"}        a cloud model: faster, but your questions are sent to the
                                                                            company, and it costs money per answer
  {"label": "GPT web", "provider": "openai", "model": "gpt-5-search-api"}             searches the web first (news, scores, facts)
A cloud entry needs a key: put  OPENAI_API_KEY=...  in a file called .env next to this one (never share or commit it). Optional
fields: "base_url" (any OpenAI-compatible service), "reasoning_effort" (e.g. "low", for models that take it), "max_tokens" (the
answer plus the model's hidden thinking, default 4000). Labels: 19 characters at most.

A local model that has been idle takes up to a minute for its first answer (it has to load into memory); --keep-alive keeps it loaded
longer between questions, at the cost of memory. A local model is not started while a Blender or Apex Sky render is running, because they
compete for memory and the graphics chip; --ignore-render skips that check.

Messages, Mac to calculator (every length is one byte):
  'A' len text     more of the assistant's answer (plain ASCII; the calculator joins the pieces and wraps the lines)
  'D'              the answer is complete
  'T' len text     a short status such as "Thinking..."; length 0 clears it
  'C'              clear the whole conversation
  'K'              the list of models is about to be sent again      'L' len text   one model's name, added to the list
  'U' index        which model in the list is in use (one byte, not text)
Messages, calculator to Mac:
  'Q' len text     a question          'N'  new conversation          'S'  stop the answer          'Z'  quit
  'R'              send me the list of models                         'M' index      use this model (one byte)
Tools (tools.py, PROTOCOL.md section 5): a 'T' whose text starts with byte 1 is a request for the calculator ("form|...", "triangle|...");
a 'Q' whose text starts with byte 1 is the calculator's reply ("form|a=3|b=4"), or "hello|tools=1" once, meaning it can show cards."""
import argparse, glob, inspect, json, os, queue, re, select, subprocess, sys, threading, time, tty, unicodedata, urllib.error, urllib.request
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tools as T  # noqa: E402  (the tools the model can call: exact maths here, cards on the calculator)

OLLAMA = "http://localhost:11434/api/chat"
OLLAMA_PS = "http://localhost:11434/api/ps"
OPENAI_BASE = "https://api.openai.com/v1"
HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_MODEL = "qwen3.8:27b"
DEFAULT_MODELS = [
    {"label": "Local 27B (private)", "provider": "ollama", "model": DEFAULT_MODEL},
    {"label": "GPT fast (cloud)", "provider": "openai", "model": "gpt-5-mini", "reasoning_effort": "low"},
]
CLOUD_MAX_TOKENS = 4000  # a reasoning model's hidden thinking counts against this too, so it is far more than the answer needs
MAX_LABEL = 19
MAX_MODELS = 8
KEEP_ALIVE = "2m"       # the model leaves memory two minutes after the last answer
MAX_HISTORY = 6         # earlier question-and-answer pairs that are remembered
RENDER_PATTERNS = ("Blender -b", "Blender.app/Contents/MacOS/Blender", "pov-overnight.sh", "render_orbit.py")
SYSTEM = (
    "You are CalcGPT, a general-purpose assistant running on a TI-84 Plus CE graphing calculator. The screen is 38 characters "
    "wide and about 10 lines tall, so every word counts. It is now {now}. The user is a high school student.\n"
    "Help with anything: school subjects (maths, physics, chemistry, biology, history, English and more), writing, coding "
    "(including TI-BASIC and Python on this calculator), and everyday questions.\n"
    "FORMAT: plain ASCII text only. No markdown, no asterisks, no # headings, no tables, no emoji, no links or URLs, no source "
    "lists. Short lines. For steps, put each step on its own line: 1) 2) 3).\n"
    "LENGTH: be brief, usually 1 to 4 short sentences, under about 60 words. Go longer only when asked, when the user types MORE (then "
    "continue or expand your last answer), or when a problem needs its steps.\n"
    "MATHS: result first, then the key steps. Write it the way it is typed on the calculator: x^2, sqrt(3)/2, sin(30)=1/2, pi/6, "
    "e^(2x), ln(x), log(x), 3E8 for 3x10^8, (a+b)/(c+d). Keep units. Say when a value is rounded. Check the arithmetic before answering.\n"
    "CALCULATOR HELP: when asked how to do something on the TI-84 Plus CE, give the exact key presses using the names printed on "
    "the keys and menus, for example: 2nd TRACE (CALC), 4:maximum. Or: STAT, 1:Edit. Or: MATH, 1:>Frac.\n"
    "HOMEWORK: solve it, but show the method in a way the user can repeat on a test. To check an answer, say right or wrong first, then why.\n"
    "HONESTY: if you are not sure, or the question needs current information you do not have, say so in one short sentence. "
    "Never invent facts, numbers, dates or quotes.")


def system_prompt(now=None):
    """The system prompt with the current date and time filled in (so 'what day is it' and 'how long until' work)."""
    return SYSTEM.format(now=time.strftime("%A %d %B %Y, %H:%M", time.localtime(now)))

PUNCT = str.maketrans({"‘": "'", "’": "'", "“": '"', "”": '"', "–": "-", "—": "-", "−": "-",
                       "•": "-", "…": "...", " ": " ", "×": "x", "÷": "/", "≤": "<=", "≥": ">=",
                       "≠": "!=", "√": "sqrt", "π": "pi", "°": " deg", "→": "->", "←": "<-",
                       "²": "^2", "³": "^3", "θ": "theta", "α": "alpha", "β": "beta", "γ": "gamma",
                       "∞": "infinity", "≈": "~", "±": "+/-"})


def ascii_text(s):
    s = s.translate(PUNCT)
    return unicodedata.normalize("NFKD", s).encode("ascii", "ignore").decode("ascii")


LINK = re.compile(r" ?\(\[([^\]\n]*)\]\([^)\s]*\)\)|\[([^\]\n]*)\]\([^)\s]*\)")   # ([source](url)) citations, then [text](url)
URL = re.compile(r" ?\(?https?://[^\s)]+\)?")
DOMAIN = re.compile(r"[\w.-]+\.[a-z]{2,}", re.I)
UNFINISHED_LINK = re.compile(r" ?\(?\[[^\]\n]{0,200}(\]\([^)\s]*\)?)?$| ?\(?h(t(t(p(s?(:(/(/[^\s)]*)?)?)?)?)?)?)?$")


def drop_links(s):
    """Markdown links become their text; citations like ([apnews.com](https://...)) and bare URLs are removed (useless on a calculator)."""
    def link(m):
        text = m.group(1) if m.group(1) is not None else m.group(2)
        if DOMAIN.fullmatch(text.strip()):
            return ""
        return f" ({text})" if m.group(1) is not None else text
    return URL.sub("", LINK.sub(link, s))


class Cleaner:
    """Turns the model's streamed text into plain calculator text: drops thinking blocks, links and markdown marks, ASCII only."""
    TAGS = ("<think>", "</think>")

    def __init__(self):
        self.in_think, self.pending, self.link_hold = False, "", ""

    def feed(self, piece):
        text, out = self.pending + piece, ""
        self.pending = ""
        while text:
            if self.in_think:
                end = text.find("</think>")
                if end < 0:
                    cut = text.rfind("<")
                    if cut >= 0 and "</think>".startswith(text[cut:]):
                        self.pending = text[cut:]  # a closing tag split across two pieces
                    return self._plain(out)
                text, self.in_think = text[end + 8:], False
                continue
            start = text.find("<think>")
            if start >= 0:
                out += text[:start]
                text, self.in_think = text[start + 7:], True
                continue
            cut = text.rfind("<")
            if cut >= 0 and any(t.startswith(text[cut:]) for t in self.TAGS):
                self.pending, text = text[cut:], text[:cut]  # might be the start of a tag: wait for the next piece
            out += text
            text = ""
        return self._plain(out)

    def flush(self):
        """What is left at the end of the reply (a held-back piece that turned out not to be a tag or a link)."""
        rest, self.pending = self.pending, ""
        held, self.link_hold = self.link_hold, ""
        return "" if self.in_think else self._plain(drop_links(held + rest), links=False)

    def _plain(self, s, links=True):
        if links:  # a link can arrive in several pieces: hold back a possibly unfinished one until the next piece
            s = self.link_hold + s
            m = UNFINISHED_LINK.search(s)
            s, self.link_hold = (s[:m.start()], m.group(0)) if m and m.group(0) else (s, "")
            s = drop_links(s)
        s = ascii_text(s).replace("\r", "").replace("\t", "  ")
        s = re.sub(r"(?m)^[ \t]*([-*_])[ \t]*(\1[ \t]*){2,}\n?", "", s)   # divider lines like --- (a wasted screen line)
        s = re.sub(r"\n{3,}", "\n\n", s)
        s = s.replace("**", "").replace("__", "").replace("`", "")
        s = re.sub(r"(?m)^(\s*)#{1,6}\s*", r"\1", s)        # heading marks
        s = re.sub(r"(?m)^(\s*)[*+]\s+", r"\1- ", s)         # bullets
        s = re.sub(r"\\\(|\\\)|\\\[|\\\]", "", s)    # LaTeX brackets
        return s


def gpu_busy():
    return any(subprocess.run(["pgrep", "-f", p], capture_output=True).returncode == 0 for p in RENDER_PATTERNS)


def ollama_stream(body):
    """Yields the pieces of text of a reply. Only talks to this Mac. Raises OSError if Ollama is not there."""
    req = urllib.request.Request(OLLAMA, json.dumps(body).encode(), {"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=600) as r:
        for line in r:
            if not line.strip():
                continue
            item = json.loads(line)
            if item.get("error"):
                raise OSError(item["error"])
            piece = item.get("message", {}).get("content", "")
            if piece:
                yield piece
            if item.get("done"):
                return


def ollama_loaded(model):
    """Is the model already in memory? True or False, or None if Ollama can not say."""
    try:
        with urllib.request.urlopen(OLLAMA_PS, timeout=3) as r:
            names = [m.get("name") or m.get("model") for m in json.load(r).get("models", [])]
        return model in names
    except (OSError, ValueError):
        return None


def encode_text(tag, text):
    b = ascii_text(text).encode("ascii", "replace")[:255]
    return tag + bytes([len(b)]) + b


def load_env(path=os.path.join(HERE, ".env")):
    """KEY=value lines from a .env file (comments and blank lines skipped). A missing file gives nothing."""
    out = {}
    try:
        for line in open(path, encoding="utf-8"):
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                k, v = line.split("=", 1)
                out[k.strip()] = v.strip().strip("\"'")
    except OSError:
        pass
    return out


def load_models(path=os.path.join(HERE, "models.json"), local_model=DEFAULT_MODEL):
    """The models to offer. A bad or missing file gives the defaults; a bad entry is skipped (and named in the second result)."""
    problems = []
    try:
        entries = json.load(open(path, encoding="utf-8"))
    except OSError:
        entries = None
    except ValueError as e:
        entries, problems = None, [f"models.json is not valid JSON ({e})"]
    if not isinstance(entries, list):
        entries = [dict(m) for m in DEFAULT_MODELS]
        entries[0]["model"] = local_model
    good = []
    for i, e in enumerate(entries):
        if (not isinstance(e, dict) or e.get("provider") not in ("ollama", "openai") or not e.get("model")
                or not str(e.get("label", "")).strip()):
            problems.append(f"models.json entry {i + 1} needs a label, a model and a provider of ollama or openai")
            continue
        good.append(dict(e, label=ascii_text(str(e["label"]))[:MAX_LABEL], model=str(e["model"])))
    if not good:
        good = [dict(DEFAULT_MODELS[0], model=local_model)]
    return good[:MAX_MODELS], problems


def cloud_error(e):
    """A short readable reason from a cloud HTTP error, never including the key."""
    try:
        msg = json.load(e).get("error", {}).get("message") or ""
    except (ValueError, OSError, AttributeError):
        msg = ""
    return ascii_text(msg or f"HTTP {e.code}").replace("\n", " ")[:150]


def openai_stream(entry, key, messages, tools=None):
    """Yields the pieces of text of a reply from an OpenAI-compatible service, and at the end, if the model called tools,
    one ("tool_calls", [{id, name, arguments}, ...]) item. Raises OSError with a readable reason."""
    body = {"model": entry["model"], "messages": messages, "stream": True, "max_completion_tokens": entry.get("max_tokens", CLOUD_MAX_TOKENS)}
    if entry.get("reasoning_effort"):
        body["reasoning_effort"] = entry["reasoning_effort"]
    if tools:
        body["tools"] = tools
    url = (entry.get("base_url") or OPENAI_BASE).rstrip("/") + "/chat/completions"
    req = urllib.request.Request(url, json.dumps(body).encode(), {"Content-Type": "application/json", "Authorization": "Bearer " + key})
    pending = {}   # tool calls arrive in pieces, by index
    try:
        with urllib.request.urlopen(req, timeout=120) as r:
            for raw in r:
                line = raw.decode("utf-8", "replace").strip()
                if not line.startswith("data:"):
                    continue
                data = line[5:].strip()
                if data == "[DONE]":
                    break
                choices = json.loads(data).get("choices") or []
                delta = choices[0].get("delta", {}) if choices else {}
                piece = delta.get("content")
                if piece:
                    yield piece
                for tc in delta.get("tool_calls") or []:
                    p = pending.setdefault(tc.get("index", 0), {"id": "", "name": "", "arguments": ""})
                    if tc.get("id"):
                        p["id"] = tc["id"]
                    fn = tc.get("function") or {}
                    p["name"] += fn.get("name") or ""
                    p["arguments"] += fn.get("arguments") or ""
    except urllib.error.HTTPError as e:
        raise OSError("cloud says: " + cloud_error(e))
    if pending:
        yield ("tool_calls", [pending[i] for i in sorted(pending)])


def takes_tools(stream):
    """Does this stream function accept a tools argument? (test fakes may not)"""
    try:
        return len(inspect.signature(stream).parameters) >= 4
    except (TypeError, ValueError):
        return False


def list_cloud(entry, key):
    """The model names the cloud account offers."""
    req = urllib.request.Request((entry.get("base_url") or OPENAI_BASE).rstrip("/") + "/models", headers={"Authorization": "Bearer " + key})
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            return sorted(m["id"] for m in json.load(r).get("data", []))
    except urllib.error.HTTPError as e:
        raise OSError(cloud_error(e))


class Chat:
    """The conversation and what to send for each question. No serial port in here, so it can be tested."""
    def __init__(self, model=DEFAULT_MODEL, stream=None, busy=None, ignore_render=False, loaded=None, keep_alive=KEEP_ALIVE,
                 models=None, cloud_stream=None, api_key=None):
        self.ignore_render, self.keep_alive = ignore_render, keep_alive
        self.models = models or [dict(DEFAULT_MODELS[0], model=model)]
        self.selected = 0
        self.loaded = loaded or ollama_loaded
        self.stream = stream or ollama_stream   # looked up when the chat is made, so tests can swap them
        self.cloud_stream = cloud_stream or openai_stream
        self.api_key = api_key
        self.busy = busy or gpu_busy
        self.history = []
        self.tools_on = takes_tools(self.cloud_stream)   # the model gets tools (cloud models only)
        self.calc_tools = False                          # the calculator said it can show tool cards
        self.calc_replies = queue.Queue()                # the calculator's answers to tool requests, from the reader thread

    def new(self):
        self.history = []

    def listing(self):
        """The messages that tell the calculator which models there are and which one is in use."""
        out = [b"K"] + [encode_text(b"L", m["label"]) for m in self.models]
        return out + [b"U" + bytes([self.selected])]

    def select(self, index):
        if 0 <= index < len(self.models):
            self.selected = index

    def calc_reply(self, text):
        """The calculator answered a tool request (the text after the marker)."""
        self.calc_replies.put(text)

    def _wait_calc(self, cancelled):
        """The calculator's answer to the tool request just sent, as a dict; None if the question was stopped meanwhile."""
        deadline = time.time() + T.CALC_WAIT
        while time.time() < deadline:
            if cancelled():
                return None
            try:
                return T.parse_calc_reply(self.calc_replies.get(timeout=0.2))
            except queue.Empty:
                pass
        return {"error": "no answer from the calculator (the user did not fill it in); continue without it"}

    def _cloud_rounds(self, entry, messages, cancelled):
        """The cloud model with tools: yields text pieces, and raw wire messages (bytes) for the calculator's tool cards; runs
        the tools the model calls and asks it again, up to T.MAX_ROUNDS times."""
        tools = T.schemas(calc=self.calc_tools)
        for _ in range(T.MAX_ROUNDS):
            text, calls = "", []
            for item in self.cloud_stream(entry, self.api_key, messages, tools):
                if isinstance(item, str):
                    text += item
                    yield item
                elif item and item[0] == "tool_calls":
                    calls = item[1]
                if cancelled():
                    return
            if not calls:
                return
            messages.append({"role": "assistant", "content": text or None, "tool_calls": [
                {"id": c["id"], "type": "function", "function": {"name": c["name"], "arguments": c["arguments"]}} for c in calls]})
            for c in calls:
                if cancelled():
                    return
                try:
                    args = json.loads(c["arguments"] or "{}")
                    if not isinstance(args, dict):
                        raise ValueError("not an object")
                    result = None
                except ValueError:
                    result = {"error": "the arguments were not valid JSON"}
                if result is not None:
                    pass
                elif c["name"] in T.SERVER_TOOLS:
                    result = T.run_server_tool(c["name"], args)
                elif c["name"] in T.CALC_TOOLS:
                    if not self.calc_tools:
                        result = {"error": "calculator cards are not available in this session: answer in text"}
                    else:
                        while not self.calc_replies.empty():   # a stale answer from an earlier card
                            self.calc_replies.get_nowait()
                        yield T.tool_message(T.calc_payload(c["name"], args))
                        result = self._wait_calc(cancelled)
                        if result is None:
                            return
                else:
                    result = {"error": f"unknown tool {c['name']}"}
                messages.append({"role": "tool", "tool_call_id": c["id"], "content": json.dumps(result)})
        yield "\n(Too many tool steps: ask again more simply.)"

    def reply(self, question, cancelled=lambda: False):
        """Yields the messages to send for one question, in order."""
        entry = self.models[self.selected]
        cloud = entry["provider"] != "ollama"
        messages = ([{"role": "system", "content": system_prompt()}] + self.history[-2 * MAX_HISTORY:]
                    + [{"role": "user", "content": question}])
        if cloud and not self.api_key:
            yield encode_text(b"A", "This model is in the cloud and needs a key. Put OPENAI_API_KEY=... in the file .env next to calcgptd.py, then restart it.")
            yield b"D"
            return
        if not cloud and not self.ignore_render and self.busy():
            yield encode_text(b"T", "A render is running: AI paused.")
            yield b"D"
            return
        yield encode_text(b"T", "Loading the model (about a minute)..." if not cloud and self.loaded(entry["model"]) is False else "Thinking...")
        if cloud and self.tools_on:
            messages[0]["content"] += T.prompt_addition(self.calc_tools)
            source = self._cloud_rounds(entry, messages, cancelled)
        elif cloud:
            source = self.cloud_stream(entry, self.api_key, messages)
        else:
            source = self.stream({"model": entry["model"], "stream": True, "think": False, "keep_alive": self.keep_alive,
                                  "messages": messages, "options": {"temperature": 0.4, "num_ctx": 4096, "num_predict": 500}})
        cleaner, answer, first, buf, last_send = Cleaner(), "", True, "", time.time()
        try:
            for piece in source:
                if cancelled():
                    break
                if isinstance(piece, bytes):   # a tool card for the calculator: goes out as it is, between the text
                    for i in range(0, len(buf), 200):
                        yield encode_text(b"A", buf[i:i + 200])
                    buf = ""
                    yield piece
                    continue
                text = cleaner.feed(piece)
                if not text:
                    continue
                if first:
                    text, first = text.lstrip(), False
                    yield encode_text(b"T", "")
                answer, buf = answer + text, buf + text
                if len(buf) >= 16 or time.time() - last_send > 0.15:
                    for i in range(0, len(buf), 200):
                        yield encode_text(b"A", buf[i:i + 200])
                    buf, last_send = "", time.time()
        except (OSError, urllib.error.URLError, ValueError) as e:
            for i in range(0, len(buf), 200):  # text that was waiting to be sent still goes out before the error
                yield encode_text(b"A", buf[i:i + 200])
            yield encode_text(b"T", "")
            if isinstance(e, urllib.error.URLError):
                msg = "Could not reach the cloud. Is the internet on?" if cloud else "Could not reach the model. Is Ollama running on the Mac?"
            else:
                msg = f"Model problem: {e}"
            yield encode_text(b"A", ("\n" if answer else "") + msg)
        else:
            tail = cleaner.flush()
            answer, buf = answer + tail, buf + tail
            for i in range(0, len(buf), 200):
                yield encode_text(b"A", buf[i:i + 200])
        if first:
            yield encode_text(b"T", "")
        if answer.strip():
            self.history += [{"role": "user", "content": question}, {"role": "assistant", "content": answer.strip()[:1500]}]
        yield b"D"


class CalcStream:
    """Cuts the calculator's byte stream into requests."""
    def __init__(self):
        self.buf = b""

    def feed(self, data):
        self.buf += data
        out = []
        while self.buf:
            c = chr(self.buf[0])
            if c == "Q":
                if len(self.buf) < 2 or len(self.buf) < 2 + self.buf[1]:
                    break
                out.append(("Q", self.buf[2:2 + self.buf[1]].decode("ascii", "replace")))
                self.buf = self.buf[2 + self.buf[1]:]
            elif c in "NSZR":
                out.append((c, None))
                self.buf = self.buf[1:]
            elif c == "M":
                if len(self.buf) < 2:
                    break
                out.append(("M", self.buf[1]))
                self.buf = self.buf[2:]
            else:
                self.buf = self.buf[1:]
        return out


def read_calc(fd, n):
    """Reads from the calculator link. A closed link reads as empty bytes (a pulled cable raises an error instead): both mean gone."""
    data = os.read(fd, n)
    if not data:
        raise OSError("the calculator disconnected")
    return data


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default=DEFAULT_MODEL)
    ap.add_argument("--keep-alive", default=KEEP_ALIVE, help="how long the model stays in memory after an answer, e.g. 10m (default 2m)")
    ap.add_argument("--ignore-render", action="store_true", help="run the model even while a render is running")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--list-cloud", action="store_true", help="show the model names your cloud account offers, then stop")
    a = ap.parse_args()
    models, problems = load_models(local_model=a.model)
    key = os.environ.get("OPENAI_API_KEY") or load_env().get("OPENAI_API_KEY")
    if a.list_cloud:
        cloud = next((m for m in models if m["provider"] != "ollama"), {})
        if not key:
            sys.exit("No key: put OPENAI_API_KEY=... in the file .env next to calcgptd.py.")
        try:
            print("\n".join(list_cloud(cloud, key)))
        except OSError as e:
            sys.exit(f"Could not list the models: {e}")
        return
    for p in problems:
        print("note:", p)
    ports = ([os.environ["CALC_PORT"]] if os.path.exists(os.environ["CALC_PORT"]) else []) if os.environ.get("CALC_PORT") else sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("No calculator found. Run CALCGPT on it, plug in the cable, close TI Connect CE, then try again.")
    fd = os.open(ports[0], os.O_RDWR | os.O_NOCTTY)
    tty.setraw(fd)
    chat, stream, lock = Chat(ignore_render=a.ignore_render, keep_alive=a.keep_alive, models=models, api_key=key), CalcStream(), threading.Lock()
    stop = threading.Event()
    worker = [None]

    def send(data):
        with lock:
            view = memoryview(data)
            while view:
                view = view[os.write(fd, view):]

    def answer(question):
        try:
            for m in chat.reply(question, stop.is_set):
                send(m)
        except OSError:
            pass

    def send_listing():
        for m in chat.listing():
            send(m)

    if not a.quiet:
        print(f"CalcGPT is running on {ports[0]} with {len(models)} models ({', '.join(m['label'] for m in models)}); "
              f"{'cloud key found' if key else 'no cloud key'}. Clear on the calculator quits.")
    try:
        send_listing()
        while True:
            r, _, _ = select.select([fd], [], [], 0.2)
            if not r:
                continue
            for cmd, arg in stream.feed(read_calc(fd, 1024)):
                if cmd == "Z":
                    stop.set()
                    return
                if cmd == "S":
                    stop.set()
                elif cmd == "N":
                    stop.set()
                    chat.new()
                elif cmd == "R":
                    send_listing()
                elif cmd == "M":
                    if worker[0] and worker[0].is_alive():
                        send_listing()    # not while an answer is being written: tell the calculator what is still in use
                    else:
                        chat.select(arg)
                        send_listing()
                        if not a.quiet:
                            print("   model:", chat.models[chat.selected]["label"])
                elif cmd == "Q" and arg.startswith(T.MARK):   # the calculator's answer to a tool card, or its hello
                    if arg[1:].startswith("hello"):
                        chat.calc_tools = True
                    else:
                        chat.calc_reply(arg[1:])
                elif cmd == "Q" and arg.strip() and not (worker[0] and worker[0].is_alive()):
                    if not a.quiet:
                        print("   asked:", arg[:60])
                    stop.clear()
                    worker[0] = threading.Thread(target=answer, args=(arg.strip(),), daemon=True)
                    worker[0].start()
    except OSError:
        if not a.quiet:
            print("The calculator disconnected.")
    finally:
        stop.set()
        os.close(fd)


if __name__ == "__main__":
    main()
