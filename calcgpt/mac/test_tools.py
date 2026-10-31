"""Tests for tools.py (the tools ChatGPT can call) and for the tool-call loop in calcgptd.py, with a fake model: nothing is
sent anywhere.  Run: python3 test_tools.py"""
import json, os, queue, sys, threading, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import calcgptd as G
import tools as T
from wire_decode import decode

fails = 0


def check(what, got, want):
    global fails
    if got != want:
        print(f"FAIL {what}\n   got  {got!r}\n   want {want!r}")
        fails += 1


def near(what, got, want, tol=0.01):
    global fails
    if not isinstance(got, (int, float)) or abs(got - want) > tol:
        print(f"FAIL {what}\n   got  {got!r}\n   want {want!r}")
        fails += 1


# ---- the calculator
check("arithmetic", T.calculate("2 + 3 * 4")["value"], 14)
check("^ means power", T.calculate("2^10")["value"], 1024)
near("sqrt and brackets", T.calculate("sqrt(3^2 + 4^2)")["value"], 5)
near("trig in degrees", T.calculate("sin(30)")["value"], 0.5)
near("trig in radians", T.calculate("sin(pi/2)", "rad")["value"], 1)
near("asin gives degrees", T.calculate("asin(0.5)")["value"], 30)
check("factorial and nCr", (T.calculate("factorial(5)")["value"], T.calculate("nCr(5,2)")["value"]), (120, 10))
check("division by zero", "error" in T.calculate("1/0"), True)
check("no code allowed", "error" in T.calculate("__import__('os')"), True)
check("no names allowed", "error" in T.calculate("x + 1"), True)
check("broken expression", "error" in T.calculate("2 +"), True)

# ---- solvers
r = T.solve_triangle(a=3, b=4, c=5)
check("SSS 3 4 5", (r["count"], r["solutions"][0]["C"], r["solutions"][0]["area"]), (1, 90, 6))
near("SSS A", r["solutions"][0]["A"], 36.8699)
r = T.solve_triangle(a=6, b=8, A=20)
check("SSA gives two triangles", r["count"], 2)
near("first c", r["solutions"][0]["c"], 12.857)
near("second c", r["solutions"][1]["c"], 2.178)
check("impossible: reason", "can't meet" in T.solve_triangle(a=1, b=1, c=5)["error"], True)
check("too few", "three values" in T.solve_triangle(a=1, b=2)["error"], True)
check("angles only", "side" in T.solve_triangle(A=30, B=40, C=110)["error"], True)
r = T.solve_triangle(a=3, b=4, right=True)
check("right triangle legs", (r["solutions"][0]["c"], r["rule"]), (5, "Pythagoras and SOH CAH TOA"))
r = T.solve_triangle(c=10, A=30, right=True)
near("right: a = 5", r["solutions"][0]["a"], 5)
check("negative side", "more than 0" in T.solve_triangle(a=-3, b=4, c=5)["error"], True)
check("quadratic roots", T.solve_quadratic(1, -5, 6)["roots"], [2, 3])
check("quadratic complex", T.solve_quadratic(1, 0, 1)["complex_roots"], ["0 + 1i", "0 - 1i"])
check("quadratic double", T.solve_quadratic(1, -4, 4)["real_roots"], 1)
check("quadratic a=0", T.solve_quadratic(0, 2, -4)["roots"], [2])
check("linear", T.solve_linear(2, 3, 1, 7)["x"], 4)
check("linear none", T.solve_linear(2, 3, 2, 5)["solutions"], "none")
check("system", (T.solve_system(1, 1, 3, 1, -1, 1)["x"], T.solve_system(1, 1, 3, 1, -1, 1)["y"]), (2, 1))
check("system parallel", "none" in T.solve_system(1, 1, 3, 2, 2, 7)["solutions"], True)
s = T.stats([1, 3, 5, 7, 9])
check("stats", (s["mean"], s["median"], s["mode"]), (5, 5, "none (every value once)"))
near("stats sd", s["sample_sd"], 3.1623)
check("stats mode", T.stats([9, 2, 9, 4])["mode"], [9])
check("stats empty", "error" in T.stats([]), True)
t = T.trig_values(150)
check("trig exact", (t["sin_exact"], t["cos_exact"], t["quadrant"], t["reference_angle"], t["radians"]), ("1/2", "-sqrt(3)/2", 2, 30, "5pi/6"))
check("trig undefined", T.trig_values(90)["tan"], "undefined")
check("trig wraps", T.trig_values(390)["angle_deg"], 30)
check("trig radians in", T.trig_values(3.14159265, "rad")["radians"], "pi")
check("number facts", (T.number_facts(12)["prime_factors"], T.number_facts(13)["prime"], T.number_facts(36)["perfect_square"]), ([2, 2, 3], True, True))
check("number facts bad", "error" in T.number_facts(2.5), True)
check("fraction simplify", T.fraction(6, 8)["fraction"], "3/4")
check("fraction mixed", T.fraction(7, 2)["mixed"], "3 1/2")
check("decimal to fraction", T.fraction(decimal=0.375)["fraction"], "3/8")
check("fraction by zero", "error" in T.fraction(1, 0), True)
near("km to miles", T.convert_units(10, "km", "miles")["value"], 6.2137)
check("temperature", T.convert_units(100, "c", "f")["value"], 212)
check("unknown unit", "error" in T.convert_units(1, "km", "kg"), True)
p = T.points(0, 0, 3, 4)
check("points", (p["distance"], p["midpoint"], p["slope"], p["line"]), (5, [1.5, 2], 1.333333, "y = 1.333333x + 0"))
check("points vertical", T.points(1, 0, 1, 5)["slope"], "undefined (vertical)")
check("date", len(T.date_time()["date"]), 10)
check("bad timezone", "error" in T.date_time("Mars/Olympus"), True)
check("days between", T.days_between("2026-10-01", "2026-11-01")["days"], 31)

# ---- the tool list and running by name
names = [s["function"]["name"] for s in T.schemas(calc=True)]
check("every tool is listed once", len(names), len(set(names)))
check("calculator tools only when the calculator can", "ask_form" in names and "ask_form" not in [s["function"]["name"] for s in T.schemas(calc=False)], True)
check("a schema has no leftover optional flags", any("optional" in v for s in T.schemas() for v in s["function"]["parameters"]["properties"].values()), False)
check("required lists the non-optional ones", T.schemas()[1]["function"]["parameters"]["required"], [])
check("run by name", T.run_server_tool("solve_linear", {"a": 2, "b": 3})["x"], -1.5)
check("bad arguments are an error, not a crash", "error" in T.run_server_tool("solve_linear", {"q": 1}), True)

# ---- the calculator messages
check("form payload", T.calc_payload("ask_form", {"title": "Right triangle", "hint": "Fill 2", "fields": ["a", "b", "c"]}), "form|Right triangle|Fill 2|a|b|c")
check("choice payload", T.calc_payload("ask_choice", {"title": "What kind?", "options": ["Right", "Any"]}), "choice|What kind?|Right|Any")
check("triangle payload", T.calc_payload("draw_triangle", {"a": 3, "b": 4, "c": 5.0, "A": 36.8699, "B": 53.1301, "C": 90}), "triangle|3|4|5|36.8699|53.1301|90")
check("a | in text becomes a space, unicode is dropped", T.calc_payload("show_answers", {"title": "A|B", "rule": "pi π", "lines": ["x = 1"]}), "answers|A B|pi|x = 1")
long = T.calc_payload("show_steps", {"title": "T", "lines": ["x" * 60] * 10})
check("a long request is cut to fit one message", len(long) <= T.PAYLOAD_MAX and long.startswith("steps|T|"), True)
check("unknown calculator tool", T.calc_payload("fly", {}), None)
check("wire message", T.tool_message("status"), b"T\x07\x01status")
check("form reply", T.parse_calc_reply("form|a=3|b=4.5"), {"values": {"a": 3, "b": 4.5}})
check("form cancel", T.parse_calc_reply("form|cancel"), {"result": "the user cancelled the form"})
check("choice reply", T.parse_calc_reply("choice|Any triangle"), {"chosen": "Any triangle"})
check("status reply", T.parse_calc_reply("status|time=9:05|battery=3 of 4"), {"time": "9:05", "battery": "3 of 4"})
check("error reply", T.parse_calc_reply("error|unknown tool"), {"error": "unknown tool"})
check("ok reply", T.parse_calc_reply("ok"), {"result": "ok"})

# ---- the tool-call loop in Chat.reply, with a fake cloud stream that takes tools
two = [{"label": "Local", "provider": "ollama", "model": "loc"}, {"label": "Cloud", "provider": "openai", "model": "gpt-x"}]
calls = []


def scripted(script):
    """A fake model: each round yields the next item of the script (text pieces, or ("tool_calls", [...]))."""
    rounds = list(script)

    def stream(entry, key, messages, tools=None):
        calls.append((json.loads(json.dumps(messages)), tools))
        items = rounds.pop(0)
        for it in items:
            yield it
    return stream


def answer_of(msgs):
    return "".join(m[1] for m in decode(b"".join(msgs)) if m[0] == "A")


def chat_with(script, calc=False):
    c = G.Chat(models=two, cloud_stream=scripted(script), api_key="k")
    c.select(1)
    c.calc_tools = calc
    return c


# a server tool: the model asks for a calculation, gets the exact result, then answers
calls.clear()
c = chat_with([[("tool_calls", [{"id": "c1", "name": "calculate", "arguments": json.dumps({"expression": "sqrt(3^2+4^2)"})}])],
               ["The hypotenuse is ", "5."]])
msgs = list(c.reply("hypotenuse of 3 and 4?"))
check("server tool: the answer arrives", answer_of(msgs), "The hypotenuse is 5.")
check("server tool: two rounds", len(calls), 2)
check("server tool: tools were offered", any(s["function"]["name"] == "calculate" for s in calls[0][1]), True)
check("server tool: no calculator tools without the hello", any(s["function"]["name"] == "ask_form" for s in calls[0][1]), False)
second = calls[1][0]
check("server tool: the assistant's call and the tool result went back", ([m["role"] for m in second[-2:]], json.loads(second[-1]["content"])["value"]), (["assistant", "tool"], 5))
check("server tool: the call id is kept", second[-1]["tool_call_id"], "c1")
check("server tool: history keeps only the words", [m["role"] for m in c.history], ["user", "assistant"])
check("the prompt mentions the tools", "TOOLS" in second[0]["content"], True)

# a calculator tool: the request goes to the calculator as a marked status, the reply comes back through the queue
calls.clear()
c = chat_with([[("tool_calls", [{"id": "f1", "name": "ask_form", "arguments": json.dumps({"title": "Right triangle", "hint": "Fill 2", "fields": ["a", "b"]})}])],
               [("tool_calls", [{"id": "d1", "name": "draw_triangle", "arguments": json.dumps({"a": 3, "b": 4, "c": 5, "A": 36.87, "B": 53.13, "C": 90})}])],
               ["c is 5."]], calc=True)
got = []
gen = c.reply("triangle problem")
for m in gen:
    got.append(m)
    if m.startswith(b"T") and m[2:3] == b"\x01" and m[3:].startswith(b"form"):
        c.calc_reply("form|a=3|b=4")       # the calculator answers (in real life from the reader thread)
    if m.startswith(b"T") and m[2:3] == b"\x01" and m[3:].startswith(b"triangle"):
        c.calc_reply("ok")
d = decode(b"".join(got))
check("calc tool: the form request went out", ("T", "\x01form|Right triangle|Fill 2|a|b") in d, True)
check("calc tool: the drawing request went out", ("T", "\x01triangle|3|4|5|36.87|53.13|90") in d, True)
check("calc tool: the values reached the model", json.loads(calls[1][0][-1]["content"]), {"values": {"a": 3, "b": 4}})
check("calc tool: the final words", answer_of(got), "c is 5.")
check("calc tool: calculator tools offered after the hello", any(s["function"]["name"] == "ask_form" for s in calls[0][1]), True)
check("calc tool: ends with D", d[-1], ("D",))

# the calculator does not answer: the model is told, nothing hangs
T.CALC_WAIT = 0.3
calls.clear()
c = chat_with([[("tool_calls", [{"id": "f1", "name": "ask_choice", "arguments": json.dumps({"title": "Kind?", "options": ["a", "b"]})}])], ["ok then"]], calc=True)
t0 = time.time()
msgs = list(c.reply("choose"))
check("no reply: the model hears it timed out", "no answer" in calls[1][0][-1]["content"], True)
check("no reply: it did not hang", time.time() - t0 < 3, True)
check("no reply: the chat still finishes", answer_of(msgs), "ok then")
T.CALC_WAIT = 180.0

# the user cancels on the calculator while the model waits
c = chat_with([[("tool_calls", [{"id": "f1", "name": "ask_form", "arguments": json.dumps({"title": "T", "hint": "h", "fields": ["a"]})}])], ["fine"]], calc=True)
gen = c.reply("x")
got = []
for m in gen:
    got.append(m)
    if m[2:3] == b"\x01":
        c.calc_reply("form|cancel")
check("cancel on the calculator reaches the model", "cancelled" in calls[-1][0][-1]["content"], True)

# stopping (Y= on the calculator) while a tool waits
c = chat_with([[("tool_calls", [{"id": "f1", "name": "ask_form", "arguments": json.dumps({"title": "T", "hint": "h", "fields": ["a"]})}])], ["never"]], calc=True)
stopped = {"v": False}
got = []
for m in c.reply("x", lambda: stopped["v"]):
    got.append(m)
    stopped["v"] = True
check("stopping ends the reply", decode(b"".join(got))[-1], ("D",))
check("stopping: the model was not called again", answer_of(got), "")

# a calculator tool when the calculator has not said hello: the model is told it is unavailable
c = chat_with([[("tool_calls", [{"id": "f1", "name": "ask_form", "arguments": json.dumps({"title": "T", "hint": "h", "fields": ["a"]})}])], ["typed instead"]], calc=False)
msgs = list(c.reply("x"))
check("no calculator tools: the model is told", "not available" in calls[-1][0][-1]["content"], True)
check("no calculator tools: nothing marked went to the calculator", any(m[2:3] == b"\x01" for m in msgs), False)

# an unknown tool, bad JSON arguments, and the round limit
c = chat_with([[("tool_calls", [{"id": "u1", "name": "teleport", "arguments": "{}"}, {"id": "u2", "name": "calculate", "arguments": "{not json"}])], ["done"]])
msgs = list(c.reply("x"))
tool_msgs = [m for m in calls[-1][0] if m["role"] == "tool"]
check("unknown tool and bad arguments are errors the model reads", ("unknown tool" in tool_msgs[0]["content"], "arguments" in tool_msgs[1]["content"]), (True, True))
c = chat_with([[("tool_calls", [{"id": f"r{i}", "name": "calculate", "arguments": json.dumps({"expression": "1+1"})}])] for i in range(20)])
msgs = list(c.reply("loop"))
check("the round limit stops a model that never answers", decode(b"".join(msgs))[-1], ("D",))
check("the round limit says so", "too many" in answer_of(msgs).lower() or "tool" in answer_of(msgs).lower(), True)

# text before a tool call still reaches the calculator
c = chat_with([["Let me ask... ", ("tool_calls", [{"id": "c1", "name": "calculate", "arguments": json.dumps({"expression": "2*3"})}])], ["6."]])
check("text around a tool call", answer_of(list(c.reply("x"))), "Let me ask... 6.")

# the old 3-argument fake streams keep working (no tools passed)
def plain(entry, key, messages):
    yield "plain answer"
c = G.Chat(models=two, cloud_stream=plain, api_key="k")
c.select(1)
check("a stream without a tools parameter still works", answer_of(list(c.reply("x"))), "plain answer")

# the wire reader: a marked question is a tool reply, not a question
cs = G.CalcStream()
check("marked question decodes", cs.feed(b"Q\x09\x01form|a=3"), [("Q", "\x01form|a=3")])

# openai_stream assembles streamed tool calls
import http.server
class Cloud(http.server.BaseHTTPRequestHandler):
    seen = None
    def log_message(self, *a): pass
    def do_POST(self):
        Cloud.seen = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        self.send_response(200); self.send_header("Content-Type", "text/event-stream"); self.end_headers()
        def ev(delta, finish=None):
            self.wfile.write(("data: " + json.dumps({"choices": [{"delta": delta, "finish_reason": finish}]}) + "\n\n").encode())
        ev({"content": "Sure. "})
        ev({"tool_calls": [{"index": 0, "id": "call_1", "function": {"name": "calcu", "arguments": ""}}]})
        ev({"tool_calls": [{"index": 0, "function": {"name": "late", "arguments": "{\"expr"}}]})
        ev({"tool_calls": [{"index": 0, "function": {"arguments": "ession\": \"1+1\"}"}}]})
        ev({"tool_calls": [{"index": 1, "id": "call_2", "function": {"name": "date_time", "arguments": "{}"}}]})
        ev({}, "tool_calls")
        self.wfile.write(b"data: [DONE]\n\n")
srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Cloud)
threading.Thread(target=srv.serve_forever, daemon=True).start()
entry = {"model": "gpt-x", "base_url": f"http://127.0.0.1:{srv.server_port}/v1"}
items = list(G.openai_stream(entry, "k", [{"role": "user", "content": "hi"}], T.schemas(False)))
check("streamed text first", items[0], "Sure. ")
check("tool calls assembled from pieces", items[-1], ("tool_calls", [{"id": "call_1", "name": "calculate", "arguments": "{\"expression\": \"1+1\"}"},
                                                                    {"id": "call_2", "name": "date_time", "arguments": "{}"}]))
check("the request carried the tools", [t["function"]["name"] for t in Cloud.seen["tools"]][:2], ["calculate", "solve_triangle"])
check("tool_choice is left to the model", Cloud.seen.get("tool_choice", "auto"), "auto")
srv.shutdown()

print(f"{fails} FAILED" if fails else "tools: all passed")
sys.exit(1 if fails else 0)
