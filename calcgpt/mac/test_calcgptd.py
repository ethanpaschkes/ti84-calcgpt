"""Tests for calcgptd.py with a fake model: no Ollama and no calculator needed.  Run: python3 test_calcgptd.py"""
import http.server, json, os, sys, threading, urllib.error
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import calcgptd as G
from wire_decode import decode

fails = 0


def check(what, got, want):
    global fails
    if got != want:
        print(f"FAIL {what}\n   got  {got!r}\n   want {want!r}")
        fails += 1


def answer_of(msgs):
    return "".join(m[1] for m in decode(b"".join(msgs)) if m[0] == "A")


# ---- the cleaner
def clean(pieces):
    c = G.Cleaner()
    return "".join(c.feed(p) for p in pieces) + c.flush()

check("plain text passes", clean(["Hello ", "world"]), "Hello world")
check("thinking block removed", clean(["<think>secret plan</think>The answer is 4."]), "The answer is 4.")
check("thinking block split over pieces", clean(["<thi", "nk>abc</th", "ink>OK"]), "OK")
check("less-than sign survives", clean(["if x ", "< 5 then", " go"]), "if x < 5 then go")
check("a lone < at the very end is kept", clean(["a <"]), "a <")
check("markdown marks removed", clean(["**Bold** and `code`"]), "Bold and code")
check("heading and bullets", clean(["## Title\n* one\n+ two\n- three"]), "Title\n- one\n- two\n- three")
check("latex brackets removed", clean([r"Area is \( \frac{1}{2} \)"]), "Area is  \\frac{1}{2} ")
check("unicode becomes ascii", clean(["sin 30° = 1/2, π/6 ≤ √2 — café"]), "sin 30 deg = 1/2, pi/6 <= sqrt2 - cafe")
check("star as multiply stays", clean(["2*3 = 6"]), "2*3 = 6")
check("a thinking block that never ends shows nothing", clean(["<think>forever"]), "")
check("a citation is removed", clean(["Paris is the capital ([britannica.com](https://www.britannica.com/x?utm=1))."]), "Paris is the capital.")
check("a citation split over pieces", clean(["Rain today ([wea", "ther.com](https://wea", "ther.com/a)). Bring a coat."]), "Rain today. Bring a coat.")
check("a named link keeps its text", clean(["See [the rules](https://a.org/r) first"]), "See the rules first")
check("a bare url is removed", clean(["Read more at https://ex", "ample.com/page now"]), "Read more at now")
check("square brackets in maths stay", clean(["on [0", ",1] it rises"]), "on [0,1] it rises")
check("divider lines removed", clean(["One.\n\n---\n\nTwo.\n* * *\nThree."]), "One.\n\nTwo.\nThree.")
check("a minus sign line is kept", clean(["x = 3\n-2 is less"]), "x = 3\n-2 is less")
check("a word ending in h is not lost", clean(["The graph", " is a line. Math"]), "The graph is a line. Math")

# ---- one question
def fake(pieces, seen=None):
    def stream(body):
        if seen is not None:
            seen.append(body)
        for p in pieces:
            yield p
    return stream

seen = []
chat = G.Chat(stream=fake(["The ", "sine of 30 ", "degrees is 1/2."], seen), busy=lambda: False, loaded=lambda m: True)
msgs = list(chat.reply("sin 30?"))
d = decode(b"".join(msgs))
check("starts with a status, then clears it, then text, then done", [m[0] for m in d][:2] + [m[0] for m in d][-1:], ["T", "T", "D"])
check("the status says Thinking", d[0], ("T", "Thinking..."))
check("the status is cleared when text starts", d[1], ("T", ""))
check("the answer arrives whole", answer_of(msgs), "The sine of 30 degrees is 1/2.")
body = seen[0]
check("request: model, streaming, no thinking, local keep-alive", (body["model"], body["stream"], body["think"], body["keep_alive"]),
      (G.DEFAULT_MODEL, True, False, G.KEEP_ALIVE))
check("request: system prompt then the question", [m["role"] for m in body["messages"]], ["system", "user"])
check("request carries the question", body["messages"][-1]["content"], "sin 30?")
check("short answers asked for", "brief" in body["messages"][0]["content"].lower() and "ASCII" in body["messages"][0]["content"], True)
check("history keeps the exchange", [m["role"] for m in chat.history], ["user", "assistant"])

# ---- memory of earlier turns, limited
chat = G.Chat(stream=fake(["ok"], seen), busy=lambda: False, loaded=lambda m: True)
for i in range(10):
    list(chat.reply(f"question {i}"))
roles = [m["role"] for m in seen[-1]["messages"]]
check("only the last pairs are sent back", (len(roles), roles[0], roles[-1]), (1 + 2 * G.MAX_HISTORY + 1, "system", "user"))
check("the oldest question was dropped", "question 0" in json.dumps(seen[-1]), False)
chat.new()
check("a new chat forgets", chat.history, [])

# ---- long answers are cut into pieces of at most 200 characters
long_text = "word " * 150
msgs = list(G.Chat(stream=fake([long_text]), busy=lambda: False, loaded=lambda m: True).reply("long"))
check("pieces fit one byte", all(len(m) <= 2 + 255 for m in msgs), True)
check("long answer arrives complete", answer_of(msgs), long_text)

# ---- stopping
state = {"n": 0}
def counting_stream(body):
    for i in range(100):
        state["n"] = i
        yield f"piece{i} " * 4
def stop_after_three():
    return state["n"] >= 3
msgs = list(G.Chat(stream=counting_stream, busy=lambda: False, loaded=lambda m: True).reply("count", stop_after_three))
check("stopping ends the reply with D", decode(b"".join(msgs))[-1], ("D",))
check("stopping really stops early", state["n"] < 10, True)

# ---- render guard
ran = []
chat = G.Chat(stream=fake(["never"], ran), busy=lambda: True, loaded=lambda m: True)
msgs = list(chat.reply("hi"))
check("render running: no model call", ran, [])
check("render running: told on screen", [m for m in decode(b"".join(msgs))], [("T", "A render is running: AI paused."), ("D",)])
chat = G.Chat(stream=fake(["ok"], ran), busy=lambda: True, ignore_render=True, loaded=lambda m: True)
check("--ignore-render runs anyway", answer_of(list(chat.reply("hi"))), "ok")

# ---- a model that is not in memory yet
first = decode(next(iter(G.Chat(stream=fake(["ok"]), busy=lambda: False, loaded=lambda m: False).reply("hi"))))[0]
check("a cold model says it is loading", first, ("T", "Loading the model (about a minute)..."))
first = decode(next(iter(G.Chat(stream=fake(["ok"]), busy=lambda: False, loaded=lambda m: None).reply("hi"))))[0]
check("unknown state just says Thinking", first, ("T", "Thinking..."))
seen2 = []
list(G.Chat(stream=fake(["ok"], seen2), busy=lambda: False, loaded=lambda m: True, keep_alive="10m").reply("hi"))
check("keep-alive is passed on", seen2[0]["keep_alive"], "10m")

# ---- errors
def broken(body):
    raise urllib.error.URLError("refused")
    yield
msgs = list(G.Chat(stream=broken, busy=lambda: False, loaded=lambda m: True).reply("hi"))
check("Ollama not running: clear message", "Ollama" in answer_of(msgs) and decode(b"".join(msgs))[-1] == ("D",), True)
def odd(body):
    yield "partial "
    raise OSError("model crashed")
chat = G.Chat(stream=odd, busy=lambda: False, loaded=lambda m: True)
msgs = list(chat.reply("hi"))
check("error after some text keeps the text", answer_of(msgs).startswith("partial \nModel problem: model crashed"), True)
check("a failed answer is remembered as the partial text only", chat.history[-1]["content"], "partial")

# ---- the calculator's requests
cs = G.CalcStream()
data = b"Q" + bytes([5]) + b"hello" + b"S" + b"N" + b"Z"
check("requests", cs.feed(data), [("Q", "hello"), ("S", None), ("N", None), ("Z", None)])
cs = G.CalcStream()
got = []
for i in range(len(data)):
    got += cs.feed(data[i:i + 1])
check("requests split byte by byte", got, [("Q", "hello"), ("S", None), ("N", None), ("Z", None)])

# ---- the calculator's model requests
cs = G.CalcStream()
data = b"R" + b"M" + bytes([2]) + b"Q" + bytes([2]) + b"hi"
check("model requests", cs.feed(data), [("R", None), ("M", 2), ("Q", "hi")])
cs = G.CalcStream()
got = []
for i in range(len(data)):
    got += cs.feed(data[i:i + 1])
check("model requests split byte by byte", got, [("R", None), ("M", 2), ("Q", "hi")])

# ---- models.json
import tempfile
tmpdir = tempfile.mkdtemp()
def write(name, text):
    p = os.path.join(tmpdir, name)
    open(p, "w").write(text)
    return p
ms, probs = G.load_models(os.path.join(tmpdir, "missing.json"), local_model="tiny:1b")
check("no file gives the defaults", ([m["provider"] for m in ms], ms[0]["model"], probs), (["ollama", "openai"], "tiny:1b", []))
ms, probs = G.load_models(write("ok.json", json.dumps([{"label": "A very long label that will not fit", "provider": "ollama", "model": "x"},
                                                        {"label": "Cloud caf\u00e9", "provider": "openai", "model": "y", "reasoning_effort": "low"}])))
check("labels are cut to fit and made ASCII", ([m["label"] for m in ms], len(ms[0]["label"])), (["A very long label t", "Cloud cafe"], 19))
check("extra fields are kept", ms[1]["reasoning_effort"], "low")
ms, probs = G.load_models(write("bad.json", json.dumps([{"label": "ok", "provider": "ollama", "model": "x"}, {"label": "no model", "provider": "openai"},
                                                         {"label": "weird", "provider": "carrier-pigeon", "model": "z"}, "text"])))
check("bad entries are skipped and named", (len(ms), len(probs)), (1, 3))
ms, probs = G.load_models(write("broken.json", "{not json"))
check("broken JSON falls back to the defaults", (len(ms), len(probs)), (2, 1))
ms, probs = G.load_models(write("many.json", json.dumps([{"label": f"m{i}", "provider": "ollama", "model": "x"} for i in range(12)])))
check("no more models than the calculator can show", len(ms), G.MAX_MODELS)
ms, probs = G.load_models(write("none.json", "[]"))
check("an empty list falls back to one local model", [m["provider"] for m in ms], ["ollama"])

# ---- .env
check("env file", G.load_env(write("env", "# key\nOPENAI_API_KEY = \"sk-test\"\n\nOTHER=1\n")), {"OPENAI_API_KEY": "sk-test", "OTHER": "1"})
check("no env file", G.load_env(os.path.join(tmpdir, "nope")), {})

# ---- saving the key
import setkey, stat
envp = os.path.join(tmpdir, "saved.env")
open(envp, "w").write("OTHER=1\nOPENAI_API_KEY=old\n")
setkey.write_key(envp, "sk-new")
check("the key replaces the old one and other lines stay", G.load_env(envp), {"OTHER": "1", "OPENAI_API_KEY": "sk-new"})
check("the key file is private", stat.S_IMODE(os.stat(envp).st_mode), 0o600)
envp2 = os.path.join(tmpdir, "fresh.env")
setkey.write_key(envp2, "sk-1")
check("a new key file is made private", (G.load_env(envp2), stat.S_IMODE(os.stat(envp2).st_mode)), ({"OPENAI_API_KEY": "sk-1"}, 0o600))

# ---- choosing a model
two = [{"label": "Local", "provider": "ollama", "model": "loc"}, {"label": "Cloud", "provider": "openai", "model": "gpt-x"}]
chat = G.Chat(models=two, stream=fake(["local answer"]), busy=lambda: False, loaded=lambda m: True, cloud_stream=lambda e, k, m: iter(["cloud ", "answer"]), api_key="sk-test")
check("the list for the calculator", decode(b"".join(chat.listing())), [("K",), ("L", "Local"), ("L", "Cloud"), ("U", 0)])
chat.select(1)
check("the choice is reported", decode(b"".join(chat.listing()))[-1], ("U", 1))
chat.select(7)
check("a choice that does not exist is ignored", chat.selected, 1)

# ---- a cloud answer
calls = []
def cloud_fake(entry, key, messages):
    calls.append((entry["model"], key, [m["role"] for m in messages]))
    yield "Paris is "
    yield "the capital."
chat = G.Chat(models=two, stream=fake(["never"], calls), busy=lambda: True, loaded=lambda m: False, cloud_stream=cloud_fake, api_key="sk-test")
chat.select(1)
msgs = list(chat.reply("capital of France?"))
d = decode(b"".join(msgs))
check("cloud: just Thinking while it waits (no model loading)", d[0], ("T", "Thinking..."))
check("cloud: the answer arrives", answer_of(msgs), "Paris is the capital.")
check("cloud: the right model, key and messages", calls, [("gpt-x", "sk-test", ["system", "user"])])
check("cloud: a running render does not block it", d[-1], ("D",))
check("cloud: remembered", [m["role"] for m in chat.history], ["user", "assistant"])
chat = G.Chat(models=two, stream=fake(["never"]), cloud_stream=cloud_fake, api_key=None)
chat.select(1)
msgs = list(chat.reply("hi"))
check("cloud without a key: says how to fix it", ("OPENAI_API_KEY" in answer_of(msgs), decode(b"".join(msgs))[-1]), (True, ("D",)))
def cloud_down(entry, key, messages):
    raise urllib.error.URLError("no route")
    yield
chat = G.Chat(models=two, stream=fake(["never"]), cloud_stream=cloud_down, api_key="k")
chat.select(1)
check("cloud unreachable: clear message", "Could not reach the cloud" in answer_of(list(chat.reply("hi"))), True)
def cloud_bad(entry, key, messages):
    raise OSError("cloud says: The model does not exist")
    yield
chat = G.Chat(models=two, stream=fake(["never"]), cloud_stream=cloud_bad, api_key="k")
chat.select(1)
check("cloud error text reaches the screen", "The model does not exist" in answer_of(list(chat.reply("hi"))), True)
check("the system prompt is a general assistant", "general-purpose assistant" in G.SYSTEM and "school" in G.SYSTEM, True)
check("the system prompt knows the date", "Tuesday 27 October 2026" in G.system_prompt(1793110000), True)
check("the system prompt asks for key presses", "2nd TRACE" in G.system_prompt(), True)
check("every default model label fits", all(len(m["label"]) <= G.MAX_LABEL for m in G.DEFAULT_MODELS), True)

# ---- a fake cloud server over real HTTP (server-sent events, like the real service)
class Cloud(http.server.BaseHTTPRequestHandler):
    seen = None
    status = 200
    def log_message(self, *a): pass
    def do_POST(self):
        Cloud.seen = (self.path, self.headers.get("Authorization"), json.loads(self.rfile.read(int(self.headers["Content-Length"]))))
        if Cloud.status != 200:
            self.send_response(Cloud.status); self.end_headers()
            self.wfile.write(json.dumps({"error": {"message": "Incorrect API key provided."}}).encode()); return
        self.send_response(200); self.send_header("Content-Type", "text/event-stream"); self.end_headers()
        for piece in ("Hel", "lo"):
            self.wfile.write(("data: " + json.dumps({"choices": [{"delta": {"content": piece}}]}) + "\n\n").encode())
        self.wfile.write(("data: " + json.dumps({"choices": []}) + "\n\n").encode())
        self.wfile.write(b"data: [DONE]\n\n")
    def do_GET(self):
        self.send_response(200); self.end_headers()
        self.wfile.write(json.dumps({"data": [{"id": "b-model"}, {"id": "a-model"}]}).encode())
cloud_server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Cloud)
threading.Thread(target=cloud_server.serve_forever, daemon=True).start()
entry = {"model": "gpt-x", "base_url": f"http://127.0.0.1:{cloud_server.server_port}/v1", "reasoning_effort": "low"}
check("cloud streaming pieces", list(G.openai_stream(entry, "sk-real", [{"role": "user", "content": "hi"}])), ["Hel", "lo"])
path, auth, body = Cloud.seen
check("cloud request: path, key header, model, streaming, effort", (path, auth, body["model"], body["stream"], body["reasoning_effort"]),
      ("/v1/chat/completions", "Bearer sk-real", "gpt-x", True, "low"))
entry2 = dict(entry); del entry2["reasoning_effort"]
list(G.openai_stream(entry2, "k", []))
check("no effort is sent unless configured", "reasoning_effort" in Cloud.seen[2], False)
check("the model list is sorted", G.list_cloud(entry, "k"), ["a-model", "b-model"])
Cloud.status = 401
try:
    list(G.openai_stream(entry, "bad", []))
    check("an HTTP error is raised", "no error", "OSError")
except OSError as e:
    check("an HTTP error carries the service's reason and no key", ("Incorrect API key" in str(e), "bad" in str(e).replace("Incorrect", "")), (True, False))
cloud_server.shutdown()

# ---- a fake Ollama server over real HTTP
class Fake(http.server.BaseHTTPRequestHandler):
    last = None
    def log_message(self, *a): pass
    def do_POST(self):
        Fake.last = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        self.send_response(200); self.send_header("Content-Type", "application/x-ndjson"); self.end_headers()
        for piece in ("Hi ", "there"):
            self.wfile.write((json.dumps({"message": {"role": "assistant", "content": piece}, "done": False}) + "\n").encode()); self.wfile.flush()
        self.wfile.write((json.dumps({"message": {"role": "assistant", "content": ""}, "done": True}) + "\n").encode())
server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fake)
threading.Thread(target=server.serve_forever, daemon=True).start()
G.OLLAMA = f"http://127.0.0.1:{server.server_port}/api/chat"
check("real HTTP streaming", list(G.ollama_stream({"model": "m", "messages": []})), ["Hi ", "there"])
check("the request body was sent as given", Fake.last["model"], "m")
class Err(Fake):
    def do_POST(self):
        self.rfile.read(int(self.headers["Content-Length"]))
        self.send_response(200); self.end_headers(); self.wfile.write(b'{"error":"model not found"}\n')
server2 = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Err)
threading.Thread(target=server2.serve_forever, daemon=True).start()
G.OLLAMA = f"http://127.0.0.1:{server2.server_port}/api/chat"
try:
    list(G.ollama_stream({}))
    check("an error from Ollama is raised", "no error", "OSError")
except OSError as e:
    check("an error from Ollama is raised", "model not found" in str(e), True)
server.shutdown(); server2.shutdown()
print(f"{fails} FAILED" if fails else "calcgptd: all passed")
sys.exit(1 if fails else 0)
