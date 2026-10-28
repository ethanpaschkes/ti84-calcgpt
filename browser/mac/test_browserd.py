"""Tests for browserd.py with no internet and no calculator.  Run: python3 test_browserd.py"""
import http.server, os, sys, threading, urllib.request
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import browserd as B
from wire_decode import decode

fails = 0


def check(what, got, want):
    global fails
    if got != want:
        print(f"FAIL {what}\n   got  {got!r}\n   want {want!r}")
        fails += 1


def rows_of(data):
    return [(m[1], m[2], m[3]) for m in decode(data) if m[0] == "W"]


# ---- wrap
check("short line", B.wrap([("hello", 0), ("world", 0)]), [("hello world", [])])
check("link span", B.wrap([("see", 0), ("this", 5), ("page", 5), ("now", 0)]), [("see this page now", [(5, 4, 13)])])
rows = B.wrap([(w, 0) for w in ("word " * 30).split()])
check("every row fits", all(len(t) <= 38 for t, _ in rows), True)
check("long word is cut", B.wrap([("x" * 90, 0)]), [("x" * 38, []), ("x" * 38, []), ("x" * 14, [])])
many = B.wrap([("a", 1), ("b", 2), ("c", 3), ("d", 4), ("e", 5)])
check("at most 3 link spans in a row", [len(sp) for _, sp in many], [3, 2])
check("two links keep their order", B.wrap([("a", 1), ("b", 2)]), [("a b", [(1, 0, 1), (2, 2, 3)])])
t, sp = B.wrap([("alpha", 7), ("beta", 7), ("gamma", 0), ("delta", 7)])[0]
check("a link spans its inner space only", (t, sp), ("alpha beta gamma delta", [(7, 0, 10), (7, 17, 22)]))
check("link broken over two rows continues", [sp for _, sp in B.wrap([("w" * 30, 9), ("z" * 30, 9)])], [[(9, 0, 30)], [(9, 0, 30)]])

# ---- reader
HTML = """<html><head><title>Sine &amp; cosine</title><style>p{color:red}</style><script>var x = 1;</script></head>
<body><nav><a href="/menu">Menu</a></nav>
<h1>The Sine Rule</h1>
<p>It says &ldquo;a/sin A = b/sin B&rdquo; for any triangle. See <a href="/law-of-cosines">the cosine rule</a> and
<a href="https://other.example/page#top">another site</a>.</p>
<ul><li>First item</li><li>Second <b>bold</b> item</li></ul>
<table class="infobox"><tr><td>NOISE IN INFOBOX</td></tr></table>
<p>Fact<sup>[1]</sup> after a citation.</p>
<div class="navbox"><div><p>NOISE NAVBOX</p></div></div>
<h2>Next part</h2><p>Café — naïve π ≤ 4</p>
<a href="#local">jump</a> <a href="mailto:x@y.z">mail</a>
<footer>FOOTER NOISE</footer></body></html>"""
page = B.reader_page("https://example.org/a/b", HTML)
texts = [t for _, t, _ in page.rows]
check("title", page.title, "Sine & cosine")
check("headings styled", [(s, t) for s, t, _ in page.rows if s in (B.BIG, B.HEADING)], [(B.BIG, "The Sine Rule"), (B.HEADING, "Next part")])
joined = " ".join(texts)
for noise in ("NOISE", "FOOTER", "var x", "Menu", "[1]", "color:red"):
    check(f"left out: {noise}", noise in joined, False)
check("quotes become plain", 'says "a/sin A = b/sin B"' in joined, True)
check("list items marked", any(t.startswith("* First item") for t in texts) and any(t.startswith("* Second bold item") for t in texts), True)
check("ascii only", all(ord(ch) < 128 for ch in joined), True)
check("accents and symbols", "Cafe - naive pi <= 4" in joined, True)
check("links resolved", sorted(page.links.values()), ["https://example.org/law-of-cosines", "https://other.example/page"])
cos_row = [(t, sp) for _, t, sp in page.rows if "cosine rule" in t][0]
lid = [k for k, v in page.links.items() if v.endswith("law-of-cosines")][0]
start = cos_row[0].index("the cosine rule")
check("cosine link covers its words", [s for s in cos_row[1] if s[0] == lid][0][1:], (start, start + len("the cosine rule")))
check("anchors and mail links are not links", len(page.links), 2)
check("page rows all fit", all(len(t) <= 38 and len(sp) <= 3 for _, t, sp in page.rows), True)
long = B.reader_page("https://x.org/", "<body>" + "".join(f"<p>paragraph number {i} with several words</p>" for i in range(900)) + "</body>")
check("long page is cut at 400 rows with a note", (len(long.rows), long.rows[-1][0], "cut" in long.rows[-1][1]), (400, B.NOTE, True))
check("empty page says so", B.reader_page("https://x.org/", "<body><script>1</script></body>").rows[0][1], "(this page has no text to show)")
check("title falls back to the site", B.reader_page("https://x.org/p", "<body>hi</body>").title, "x.org")

# ---- encoding round trip
msgs = decode(B.encode_page(page))
check("page starts with B and ends with E", (msgs[0][0], msgs[0][1], msgs[0][2], msgs[-1]), ("B", "P", "Sine & cosine", ("E",)))
check("rows survive encoding", rows_of(B.encode_page(page)), [(s, t, sp) for s, t, sp in page.rows])
big = B.Page("P", "x")
big.add(0, "y" * 99, [(300, 5, 99), (1, 0, 1), (2, 2, 3), (3, 4, 5)])
r = rows_of(B.encode_page(big))[0]
check("long text and span limits enforced", (len(r[1]), len(r[2])), (38, 3))
check("link numbers above 255", r[2][0], (300, 5, 38))
check("home message", decode(B.encode_home([("Wiki", "u"), ("Desmos", "v")])), [("H", ["Wiki", "Desmos"])])

# ---- safety checks on addresses
for bad in ("file:///etc/passwd", "ftp://example.com/x", "http://127.0.0.1/", "http://localhost:8000/", "http://192.168.1.1/",
            "http://10.0.0.5/admin", "http://169.254.169.254/latest/", "javascript:alert(1)", "http://[::1]/"):
    try:
        B.check_url(bad)
        check(f"refused {bad}", "allowed", "refused")
    except ValueError:
        pass
B.check_url("http://8.8.8.8/")
B.check_url("http://127.0.0.1/", allow_local=True)
try:
    B.CheckedRedirect(False).redirect_request(urllib.request.Request("http://8.8.8.8/"), None, 302, "x", {}, "http://127.0.0.1/secret")
    check("redirect to this computer refused", "allowed", "refused")
except ValueError:
    pass

# ---- fetching from a local test site
class Site(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        if self.path == "/page":
            body = "<title>Local</title><h1>Hi</h1><p>Hello <a href='/next'>next</a></p>".encode(); ctype = "text/html; charset=utf-8"
        elif self.path == "/latin":
            body = "<title>L</title><p>café</p>".encode("latin-1"); ctype = "text/html; charset=iso-8859-1"
        elif self.path == "/img":
            body = b"\x89PNG"; ctype = "image/png"
        elif self.path == "/redirect":
            self.send_response(302); self.send_header("Location", "/page"); self.end_headers(); return
        else:
            self.send_response(404); self.end_headers(); return
        self.send_response(200); self.send_header("Content-Type", ctype); self.send_header("Content-Length", str(len(body)))
        self.end_headers(); self.wfile.write(body)

server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Site)
threading.Thread(target=server.serve_forever, daemon=True).start()
base = f"http://127.0.0.1:{server.server_port}"
final, text = B.fetch(base + "/page", allow_local=True)
check("fetch works", "Hello" in text, True)
check("redirect followed", B.fetch(base + "/redirect", allow_local=True)[0], base + "/page")
check("charset honoured", "café" in B.fetch(base + "/latin", allow_local=True)[1], True)
for path, why in (("/img", "not a text page"), ("/missing", "answered 404")):
    try:
        B.fetch(base + path, allow_local=True)
        check(f"{path} refused", "ok", why)
    except ValueError as e:
        check(f"{path} reason", why in str(e), True)
try:
    B.fetch(base + "/page")
    check("a local address is refused by default", "ok", "refused")
except ValueError:
    pass
server.shutdown()

# ---- search results parsing
BRAVE = '{"web":{"results":[{"title":"Law of <strong>Sines</strong>","url":"https://www.mathsisfun.com/algebra/trig-sine-law.html","description":"Learn it &amp; use it"},{"title":"No link","url":"ftp://x","description":"x"}]}}'
seen = {}
def fake_brave(url, headers=None):
    seen.update(headers or {}); seen["url"] = url
    return url, BRAVE
res = B.search_brave("x y", "SECRETKEY", get=fake_brave)
check("brave results", res, [("Law of Sines", "https://www.mathsisfun.com/algebra/trig-sine-law.html", "Learn it & use it")])
check("brave key sent as a header, not in the address", (seen.get("X-Subscription-Token"), "SECRETKEY" in seen["url"], "q=x+y" in seen["url"]), ("SECRETKEY", False, True))
WIKI = '{"query":{"search":[{"title":"Law of sines","snippet":"the <span class=\\"searchmatch\\">law</span> of sines"}]}}'
check("wiki results", B.search_wiki("x", get_json=lambda url: (url, WIKI)),
      [("Law of sines", "https://en.wikipedia.org/wiki/Law_of_sines", "the law of sines")])
SAMPLE = [("Law of sines - Wikipedia", "https://en.wikipedia.org/wiki/Law_of_sines",
           "In trigonometry, the law of sines is an equation relating lengths."),
          ("Law of Sines", "https://www.mathsisfun.com/algebra/trig-sine-law.html", "Learn it & use it")]
rp = B.results_page("sine", SAMPLE)
check("results page layout", [(s, t[:20]) for s, t, _ in rp.rows][:4],
      [(B.RESULT_TITLE, "Law of sines - Wikip"), (B.RESULT_URL, "en.wikipedia.org/wik"), (B.SNIPPET, "In trigonometry, the"), (B.SNIPPET, "an equation relating")])
check("result link covers title and address", (rp.rows[0][2], rp.rows[1][2][0][0]), ([(1, 0, 24)], 1))
check("links table", rp.links, {1: "https://en.wikipedia.org/wiki/Law_of_sines", 2: "https://www.mathsisfun.com/algebra/trig-sine-law.html"})

# ---- a Wikipedia-style page: the page's own tags carry many classes and must not be thrown away
WIKI_PAGE = """<html class="client-nojs vector-feature-language-in-header-enabled"><head><title>Law of sines - Wikipedia</title></head>
<body class="skin-vector vector-toc-available"><div class="vector-header-container"><a href="/w">Log in</a></div>
<div class="mw-body"><h1>Law of sines</h1><div class="mw-parser-output"><p>In <a href="/wiki/Trigonometry">trigonometry</a>, the law of sines relates sides.</p>
<div class="toc"><p>TOC NOISE</p></div><table class="infobox vcard"><tr><td>INFOBOX NOISE</td></tr></table>
<p>A protocol is not a toc.</p><span class="mw-editsection">edit</span></div></div></body></html>"""
wp = B.reader_page("https://en.wikipedia.org/wiki/Law_of_sines", WIKI_PAGE)
wtext = " ".join(t for _, t, _ in wp.rows)
check("wikipedia-style page is kept", ("law of sines relates sides" in wtext, "A protocol is not a toc." in wtext), (True, True))
check("wikipedia-style noise left out", [n in wtext for n in ("TOC NOISE", "INFOBOX NOISE", "edit", "Log in")], [False, False, False, False])

# ---- a session, with no internet
PAGES = {"https://site.test/a": "<title>A</title><h1>Page A</h1><p><a href='/b'>go to B</a></p>",
         "https://site.test/b": "<title>B</title><p>Page B text</p>",
         "https://en.wikipedia.org/wiki/Main_Page": "<title>Wiki</title><p>Main page</p>"}
def fake_get(url):
    if url in PAGES:
        return url, PAGES[url]
    raise ValueError("could not connect")
s = B.Session("auto", get=fake_get, brave_key="k", search_web=lambda q: [("Site A", "https://site.test/a", "snippet a")],
              search_wikipedia=lambda q: [("Wiki hit", "https://en.wikipedia.org/wiki/Main_Page", "wiki snippet")])
check("home", decode(s.home()[0]), [("H", ["Wikipedia", "Desmos", "Khan Academy", "Hacker News", "Example page"])])
out = s.handle("Q", "law of sines")
check("search sends status then results", [decode(m)[0][0] for m in out], ["T", "B"])
check("results page kind", decode(out[1])[0][:2], ("B", "R"))
out = s.handle("K", 1)
check("following a result opens the page", [m[2] for m in decode(out[1]) if m[0] == "W"], ["Page A", "go to B"])
out = s.handle("K", 1)
check("following a link on the page", [m[2] for m in decode(out[1]) if m[0] == "W"], ["Page B text"])
out = s.handle("X", None)
check("back shows the previous page", [m[2] for m in decode(out[0]) if m[0] == "W"], ["Page A", "go to B"])
s.handle("X", None)
out = s.handle("X", None)
check("back from the first page is the home page", decode(out[0])[0][0], "H")
check("back at home stays at home", decode(s.handle("X", None)[0])[0][0], "H")
out = s.handle("K", 1)
check("a bookmark at home opens", [m[2] for m in decode(out[1]) if m[0] == "W"], ["Main page"])
out = s.handle("U", "site.test/b")
check("typed address without https", [m[2] for m in decode(out[1]) if m[0] == "W"], ["Page B text"])
out = s.handle("Q", "site.test/a")
check("a search that looks like an address opens it", decode(out[1])[0][2], "A")
out = s.handle("U", "https://missing.test/")
check("a failed page shows a message", (decode(out[1])[0][:2], "could not connect" in " ".join(m[2] for m in decode(out[1]) if m[0] == "W")), (("B", "M"), True))
check("empty search does nothing", s.handle("Q", "   "), [])
check("G goes to the home page from anywhere", decode(s.handle("G", None)[0])[0][0], "H")
check("after G the history is empty", (s.history, s.page), ([], None))
check("unknown link does nothing", s.handle("K", 999), [])
s2 = B.Session("auto", get=fake_get, brave_key="k", search_web=lambda q: [], search_wikipedia=lambda q: [("Wiki hit", "https://en.wikipedia.org/wiki/Main_Page", "w")])
check("web search with no results falls back to Wikipedia", [m[2] for m in decode(s2.handle("Q", "zzz")[1]) if m[0] == "W"][0], "Wiki hit")
def failing(q): raise ValueError("blocked")
s3 = B.Session("auto", get=fake_get, brave_key="k", search_web=failing, search_wikipedia=failing)
check("both searches failing gives a message", "not working" in " ".join(m[2] for m in decode(s3.handle("Q", "x")[1]) if m[0] == "W"), True)
s4 = B.Session("wiki", get=fake_get, brave_key="k", search_web=failing, search_wikipedia=lambda q: [("W", "https://en.wikipedia.org/wiki/Main_Page", "w")])
check("wiki mode never uses the web search", decode(s4.handle("Q", "x")[1])[0][:2], ("B", "R"))

# ---- the calculator's request stream
cs = B.CalcStream()
data = b"Q" + bytes([5]) + b"sines" + b"K" + bytes([44, 1]) + b"X" + b"U" + bytes([7]) + b"a.com/b" + b"G" + b"Z"
check("requests", cs.feed(data), [("Q", "sines"), ("K", 300), ("X", None), ("U", "a.com/b"), ("G", None), ("Z", None)])
cs = B.CalcStream()
got = []
for i in range(len(data)):
    got += cs.feed(data[i:i + 1])
check("requests split byte by byte", got, [("Q", "sines"), ("K", 300), ("X", None), ("U", "a.com/b"), ("G", None), ("Z", None)])
check("looks like an address", [B.looks_like_address(t) for t in ("example.com", "https://a.b/c d".split()[0], "law of sines", "what is 3.5", "a.co/x?y=1", "x.y z")],
      [True, True, False, False, True, False])
s5 = B.Session("auto", get=fake_get, search_web=failing, search_wikipedia=lambda q: [("W", "https://en.wikipedia.org/wiki/Main_Page", "w")])
check("no Brave key: the web search is never tried", decode(s5.handle("Q", "x")[1])[0][:2], ("B", "R"))
os.environ.pop("BRAVE_API_KEY", None)
check("key read from the environment", (os.environ.__setitem__("BRAVE_API_KEY", " abc ") or B.read_env_key()), "abc")
os.environ.pop("BRAVE_API_KEY", None)
print(f"{fails} FAILED" if fails else "browserd: all passed")
sys.exit(1 if fails else 0)
