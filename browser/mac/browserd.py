#!/usr/bin/env python3
"""CalcSearch: the Mac side of the calculator web browser. It has no window: it fetches web pages, cuts them down to
plain text rows and sends only those rows to the calculator (running BROWSER) over the USB cable.

Usage:  python3 browserd.py [--search auto|brave|wiki] [--bookmarks FILE] [--quiet]
Needs no Python packages. Close TI Connect CE first, start BROWSER on the calculator, then run this.

Search uses official search services only: Wikipedia's search API (free, no key), or Brave Search's API for the whole web if you
make a free key and put it in a file called .env next to this script as a line  BRAVE_API_KEY=your-key  (never share that file).
"auto" uses Brave when the key is there and Wikipedia otherwise. Search pages that block programs (Google, DuckDuckGo) are not
scraped. Only http and https addresses are opened, and never addresses on this computer or the local network.

Messages, Mac to calculator (all lengths are one byte; a link number is two bytes, low byte first):
  'H' n  n x (len text)                      home page: n bookmark names
  'B' kind len title                         a page starts: kind R (results), P (page) or M (message)
  'W' style nspans len text  nspans x (id id start end)
                                             one row of text (at most 38 characters) with up to 3 link spans, each a link
                                             number and the columns it covers (start up to but not including end)
  'E'                                        the page is complete
  'T' len text                               a short status such as "Loading..."
Messages, calculator to Mac:
  'Q' len text   a search (or an address if it looks like one)      'U' len text   an address
  'K' id id      follow link number     'X'  back     'G'  go to the home page     'Z'  quit
Row styles: 0 normal, 1 big heading, 2 heading, 3 result title, 4 result address, 5 result snippet, 6 note."""
import argparse, glob, html, ipaddress, os, re, select, socket, sys, tty, unicodedata, urllib.error, urllib.parse, urllib.request
from html.parser import HTMLParser

COLS = 38            # characters across the calculator screen
MAX_ROWS = 400       # rows the calculator can hold
MAX_SPANS = 3
MAX_BYTES = 1_500_000
USER_AGENT = "CalcSearch/0.1 (a student's calculator project; reads pages as plain text)"

NORMAL, BIG, HEADING, RESULT_TITLE, RESULT_URL, SNIPPET, NOTE = range(7)

SKIP_TAGS = {"script", "style", "noscript", "svg", "template", "iframe", "nav", "footer", "aside", "form", "button",
             "select", "option", "sup", "canvas", "audio", "video"}
SKIP_CLASS_WORDS = {"navbox", "infobox", "sidebar", "mw-editsection", "reference", "reflist", "toc", "hatnote", "metadata",
                    "noprint", "mw-jump-link", "printfooter", "catlinks", "navbar", "vector-toc", "mw-empty-elt"}
SKIP_CLASS_PREFIXES = ("navbox", "infobox", "sidebar", "vector-menu", "vector-header", "vector-page-toolbar", "vector-sticky",
                       "vector-column", "vector-user", "vector-dropdown", "mw-indicators")
BLOCK_TAGS = {"p", "div", "section", "article", "main", "li", "ul", "ol", "tr", "table", "br", "h1", "h2", "h3", "h4", "h5",
              "h6", "blockquote", "pre", "dd", "dt", "dl", "figure", "figcaption", "hr", "header", "thead", "tbody"}
VOID_TAGS = {"br", "img", "hr", "meta", "link", "input", "area", "base", "col", "embed", "source", "track", "wbr"}
PUNCT = str.maketrans({"‘": "'", "’": "'", "“": '"', "”": '"', "–": "-", "—": "-", "−": "-",
                       "•": "*", "…": "...", " ": " ", "×": "x", "÷": "/", "≤": "<=", "≥": ">=",
                       "≠": "!=", "√": "sqrt", "π": "pi", "°": " deg", "→": "->", "←": "<-"})


def ascii_text(s):
    """The calculator font only has plain characters: swap look-alikes, drop accents, drop the rest."""
    s = s.translate(PUNCT)
    return unicodedata.normalize("NFKD", s).encode("ascii", "ignore").decode("ascii")


class Page:
    def __init__(self, kind, title):
        self.kind, self.title = kind, title
        self.rows = []     # (style, text, [(link id, start, end), ...])
        self.links = {}    # link id -> address

    def add(self, style, text, spans=()):
        if len(self.rows) < MAX_ROWS:
            self.rows.append((style, text[:COLS], [s for s in spans if s[1] < COLS][:MAX_SPANS]))


class LinkTable:
    def __init__(self, page):
        self.page, self.ids = page, {}

    def id_for(self, url):
        if url not in self.ids:
            if len(self.ids) >= 2000:
                return 0
            self.ids[url] = len(self.ids) + 1
            self.page.links[self.ids[url]] = url
        return self.ids[url]


def wrap(words, width=COLS):
    """words: list of (text, link id or 0). -> rows of (text, spans), each row at most `width` characters with at most
    MAX_SPANS link spans (a span is [link id, start column, end column)). A link that carries on over several words is
    one span, spaces included."""
    rows, cur, spans = [], "", []
    for text, lid in words:
        pieces = [text[i:i + width] for i in range(0, len(text), width)] or [""]  # a word longer than the screen is cut
        for piece in pieces:
            sep = 1 if cur else 0
            extends = bool(lid) and bool(spans) and spans[-1][0] == lid and spans[-1][2] == len(cur)
            if cur and (len(cur) + sep + len(piece) > width or (lid and not extends and len(spans) >= MAX_SPANS)):
                rows.append((cur, [tuple(sp) for sp in spans]))
                cur, spans, extends = "", [], False
            if cur:
                cur += " "
            start = len(cur)
            cur += piece
            if lid:
                if extends:
                    spans[-1][2] = len(cur)
                else:
                    spans.append([lid, start, len(cur)])
    if cur:
        rows.append((cur, [tuple(sp) for sp in spans]))
    return rows


class Reader(HTMLParser):
    """Turns an HTML page into rows: headings, paragraphs, list items and links, without scripts, menus or pictures."""
    def __init__(self, base_url, page):
        super().__init__(convert_charrefs=True)
        self.base, self.page, self.links = base_url, page, LinkTable(page)
        self.block, self.style, self.prefix = [], NORMAL, ""
        self.skip = []           # stack of [tag, depth] while inside something that is left out
        self.link_id, self.in_title, self.title, self.cut = 0, False, "", False

    def _skip_class(self, tag, attrs):
        if tag in ("html", "body", "main", "article"):
            return False
        words = ((dict(attrs).get("class") or "") + " " + (dict(attrs).get("id") or "")).split()
        return any(w in SKIP_CLASS_WORDS or w.startswith(SKIP_CLASS_PREFIXES) for w in words)

    def flush(self):
        words = self.block
        self.block = []
        if not words or len(self.page.rows) >= MAX_ROWS:
            self.cut = self.cut or bool(words)
            return
        if self.prefix:
            words = [(self.prefix, 0)] + words
        for text, spans in wrap(words):
            self.page.add(self.style, text, spans)
        self.style, self.prefix = NORMAL, ""
        if len(self.page.rows) >= MAX_ROWS:
            self.cut = True

    def handle_starttag(self, tag, attrs):
        if self.skip:
            if tag == self.skip[-1][0] and tag not in VOID_TAGS:
                self.skip[-1][1] += 1
            return
        if tag in SKIP_TAGS or self._skip_class(tag, attrs):
            if tag not in VOID_TAGS:
                self.skip.append([tag, 1])
            return
        if tag == "title":
            self.in_title = True
        if tag in BLOCK_TAGS:
            self.flush()
        if tag == "h1":
            self.style = BIG
        elif tag in ("h2", "h3", "h4", "h5", "h6"):
            self.style = HEADING
        elif tag == "li":
            self.prefix = "*"
        elif tag == "a":
            href = dict(attrs).get("href") or ""
            url = urllib.parse.urljoin(self.base, href.strip())
            if urllib.parse.urlparse(url).scheme in ("http", "https") and not href.startswith("#"):
                self.link_id = self.links.id_for(url.split("#")[0] if "#" in url else url)

    def handle_endtag(self, tag):
        if self.skip:
            if tag == self.skip[-1][0]:
                self.skip[-1][1] -= 1
                if self.skip[-1][1] == 0:
                    self.skip.pop()
            return
        if tag == "title":
            self.in_title = False
        if tag == "a":
            self.link_id = 0
        if tag in BLOCK_TAGS:
            self.flush()

    def handle_data(self, data):
        if self.skip:
            return
        text = ascii_text(data)
        if self.in_title:
            self.title += text
            return
        for word in text.split():
            self.block.append((word, self.link_id))

    def close(self):
        super().close()
        self.flush()


def reader_page(url, html_text):
    page = Page("P", "")
    reader = Reader(url, page)
    reader.feed(html_text)
    reader.close()
    page.title = " ".join(reader.title.split()) or urllib.parse.urlparse(url).netloc
    if not page.rows:
        page.add(NOTE, "(this page has no text to show)")
    if reader.cut:
        page.rows[-1] = (NOTE, "(page cut here: too long)", [])
    return page


# ---------------------------------------------------------------- fetching (public web only)

def is_public(host):
    try:
        for info in socket.getaddrinfo(host, None):
            if not ipaddress.ip_address(info[4][0]).is_global:
                return False
        return True
    except (OSError, ValueError):
        return False


class CheckedRedirect(urllib.request.HTTPRedirectHandler):
    def __init__(self, allow_local):
        self.allow_local = allow_local

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        check_url(newurl, self.allow_local)
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def check_url(url, allow_local=False):
    parts = urllib.parse.urlparse(url)
    if parts.scheme not in ("http", "https") or not parts.hostname:
        raise ValueError("only http and https addresses can be opened")
    if not allow_local and not is_public(parts.hostname):
        raise ValueError("that address is on this computer or a local network, which is not allowed")


def fetch(url, allow_local=False, headers=None):
    """-> (final address, text). Raises ValueError with a short reason."""
    check_url(url, allow_local)
    opener = urllib.request.build_opener(CheckedRedirect(allow_local))
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT, "Accept": "text/html,text/plain;q=0.9,*/*;q=0.1",
                                               **(headers or {})})
    try:
        with opener.open(req, timeout=15) as r:
            ctype = r.headers.get("Content-Type", "")
            if not any(t in ctype for t in ("html", "text", "xml", "json")):
                raise ValueError("this is not a text page (" + (ctype.split(";")[0] or "unknown type") + ")")
            raw = r.read(MAX_BYTES)
            charset = r.headers.get_content_charset()
            final = r.geturl()
    except urllib.error.HTTPError as e:
        raise ValueError(f"the site answered {e.code}")
    except (urllib.error.URLError, OSError) as e:
        raise ValueError("could not connect")
    if not charset:
        m = re.search(rb"charset=[\"']?([\w-]+)", raw[:3000])
        charset = m.group(1).decode() if m else "utf-8"
    try:
        return final, raw.decode(charset, "replace")
    except LookupError:
        return final, raw.decode("utf-8", "replace")


# ---------------------------------------------------------------- search

def strip_tags(s):
    return " ".join(ascii_text(html.unescape(re.sub(r"<[^>]+>", "", s))).split())


def search_brave(query, key, get=fetch):
    """Brave Search's official web search API (needs a free key)."""
    import json
    url = "https://api.search.brave.com/res/v1/web/search?count=8&q=" + urllib.parse.quote_plus(query)
    final, text = get(url, headers={"X-Subscription-Token": key, "Accept": "application/json"})
    out = []
    for item in json.loads(text).get("web", {}).get("results", []):
        out.append((strip_tags(item.get("title", "")), item.get("url", ""), strip_tags(item.get("description", ""))))
    return [r for r in out if r[1].startswith("http")][:8]


def search_wiki(query, get_json=None):
    import json
    url = ("https://en.wikipedia.org/w/api.php?action=query&list=search&srlimit=8&format=json&srsearch="
           + urllib.parse.quote_plus(query))
    final, text = (get_json or fetch)(url)
    out = []
    for item in json.loads(text).get("query", {}).get("search", []):
        title = item["title"]
        out.append((ascii_text(title), "https://en.wikipedia.org/wiki/" + urllib.parse.quote(title.replace(" ", "_")),
                    strip_tags(item.get("snippet", ""))))
    return out


def results_page(query, results):
    page = Page("R", "Search: " + ascii_text(query))
    links = LinkTable(page)
    for title, url, snippet in results:
        lid = links.id_for(url)
        for text, _ in wrap([(w, 0) for w in ascii_text(title).split()])[:2]:
            page.add(RESULT_TITLE, text, [(lid, 0, len(text))])
        shown = re.sub(r"^https?://(www\.)?", "", url)[:COLS]
        page.add(RESULT_URL, shown, [(lid, 0, len(shown))])
        for text, _ in wrap([(w, 0) for w in snippet.split()])[:2]:
            page.add(SNIPPET, text)
    if not results:
        page.add(NOTE, "No results. Try other words.")
    return page


def looks_like_address(text):
    t = text.strip()
    return bool(re.match(r"^https?://\S+$", t)) or bool(re.match(r"^[\w-]+(\.[\w-]+)+(/\S*)?$", t)) and " " not in t


def message_page(text):
    page = Page("M", "Message")
    for row, _ in wrap([(w, 0) for w in ascii_text(text).split()]):
        page.add(NORMAL, row)
    return page


# ---------------------------------------------------------------- encoding for the calculator

def encode_text(s, limit=255):
    b = ascii_text(s).encode("ascii", "replace")[:limit]
    return bytes([len(b)]) + b


def encode_page(page):
    out = [b"B" + page.kind.encode() + encode_text(page.title, 38)]
    for style, text, spans in page.rows:
        t = ascii_text(text)[:COLS]
        body = bytes([style, len(spans)]) + encode_text(t)
        for lid, start, end in spans:
            body += bytes([lid & 255, lid >> 8, start, min(end, COLS)])
        out.append(b"W" + body)
    out.append(b"E")
    return b"".join(out)


def encode_home(bookmarks):
    b = b"H" + bytes([len(bookmarks)])
    for name, _ in bookmarks:
        b += encode_text(name, 24)
    return b


def encode_status(text):
    return b"T" + encode_text(text, 38)


DEFAULT_BOOKMARKS = [("Wikipedia", "https://en.wikipedia.org/wiki/Main_Page"),
                     ("Desmos", "https://www.desmos.com/"),
                     ("Khan Academy", "https://www.khanacademy.org/math/trigonometry"),
                     ("Hacker News", "https://news.ycombinator.com/"),
                     ("Example page", "https://example.com/")]


class Session:
    """What the browser does for each request from the calculator. No serial port in here, so it can be tested."""
    def __init__(self, search="auto", bookmarks=None, get=fetch, search_web=None, search_wikipedia=None, brave_key=None):
        self.mode, self.bookmarks, self.get = search, bookmarks or DEFAULT_BOOKMARKS, get
        self.brave_key = brave_key
        self.search_web = search_web or (lambda q: search_brave(q, self.brave_key, get))
        self.search_wikipedia = search_wikipedia or (lambda q: search_wiki(q, get))
        self.history, self.page = [], None

    def home(self):
        self.history, self.page = [], None
        return [encode_home(self.bookmarks)]

    def show(self, page, push=True):
        if push and self.page is not None:
            self.history.append(self.page)
        self.page = page
        return [encode_page(page)]

    def open_url(self, url):
        try:
            final, text = self.get(url)
            return self.show(reader_page(final, text))
        except ValueError as e:
            return self.show(message_page(f"Could not open {url[:60]}: {e}."))

    def do_search(self, query):
        results = []
        if self.mode in ("auto", "brave") and self.brave_key:
            try:
                results = self.search_web(query)
            except (ValueError, KeyError):
                results = []
        if not results:
            try:
                results = self.search_wikipedia(query)
            except (ValueError, KeyError):
                return self.show(message_page("Search is not working right now. Check the internet connection."))
        return self.show(results_page(query, results))

    def handle(self, cmd, arg):
        """cmd: 'Q', 'U', 'K' or 'X'; arg: text, or the link number. Returns the messages to send, in order."""
        if cmd == "Q":
            q = arg.strip()
            if not q:
                return []
            if looks_like_address(q):
                return self.handle("U", q)
            return [encode_status("Searching...")] + self.do_search(q)
        if cmd == "U":
            url = arg.strip()
            if "://" not in url:
                url = "https://" + url
            return [encode_status("Loading...")] + self.open_url(url)
        if cmd == "K":
            if self.page is None:
                if 1 <= arg <= len(self.bookmarks):
                    return self.handle("U", self.bookmarks[arg - 1][1])
                return []
            url = self.page.links.get(arg)
            return self.handle("U", url) if url else []
        if cmd == "G":
            return self.home()
        if cmd == "X":
            if self.history:
                self.page = self.history.pop()
                return [encode_page(self.page)]
            return self.home()
        return []


class CalcStream:
    """Cuts the calculator's byte stream into requests."""
    def __init__(self):
        self.buf = b""

    def feed(self, data):
        self.buf += data
        out = []
        while self.buf:
            c = chr(self.buf[0])
            if c in "QU":
                if len(self.buf) < 2 or len(self.buf) < 2 + self.buf[1]:
                    break
                out.append((c, self.buf[2:2 + self.buf[1]].decode("ascii", "replace")))
                self.buf = self.buf[2 + self.buf[1]:]
            elif c == "K":
                if len(self.buf) < 3:
                    break
                out.append(("K", self.buf[1] | self.buf[2] << 8))
                self.buf = self.buf[3:]
            elif c in "XGZ":
                out.append((c, None))
                self.buf = self.buf[1:]
            else:
                self.buf = self.buf[1:]
        return out


def read_calc(fd, n):
    """Reads from the calculator link. A closed link reads as empty bytes (a pulled cable raises an error instead): both mean gone."""
    data = os.read(fd, n)
    if not data:
        raise OSError("the calculator disconnected")
    return data


def read_env_key():
    """BRAVE_API_KEY from the environment or from a .env file next to this script. The key is never printed or logged."""
    if os.environ.get("BRAVE_API_KEY"):
        return os.environ["BRAVE_API_KEY"].strip()
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".env")
    if os.path.exists(path):
        for line in open(path, encoding="utf-8"):
            if line.startswith("BRAVE_API_KEY="):
                return line.split("=", 1)[1].strip().strip("\"'") or None
    return None


def read_bookmarks(path):
    out = []
    for line in open(path, encoding="utf-8"):
        if "|" in line and not line.startswith("#"):
            name, url = line.split("|", 1)
            out.append((ascii_text(name.strip())[:24], url.strip()))
    return out[:8] or DEFAULT_BOOKMARKS


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--search", choices=["auto", "brave", "wiki"], default="auto")
    ap.add_argument("--bookmarks", help="a text file with lines like:  Name | https://address")
    ap.add_argument("--quiet", action="store_true", help="print nothing while running")
    a = ap.parse_args()
    ports = ([os.environ["CALC_PORT"]] if os.path.exists(os.environ["CALC_PORT"]) else []) if os.environ.get("CALC_PORT") else sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("No calculator found. Run BROWSER on it, plug in the cable, close TI Connect CE, then try again.")
    fd = os.open(ports[0], os.O_RDWR | os.O_NOCTTY)
    tty.setraw(fd)
    key = read_env_key()
    if a.search == "brave" and not key:
        sys.exit("--search brave needs BRAVE_API_KEY in a .env file next to this script (see the top of this file).")
    session = Session(a.search, read_bookmarks(a.bookmarks) if a.bookmarks else None, brave_key=key)
    stream = CalcStream()

    def send(data):
        view = memoryview(data)
        while view:
            view = view[os.write(fd, view):]

    if not a.quiet:
        print(f"CalcSearch is running on {ports[0]} (no window; the pages show on the calculator). Clear on the calculator quits.")
    try:
        for m in session.home():
            send(m)
        while True:
            r, _, _ = select.select([fd], [], [], 0.2)
            if not r:
                continue
            for cmd, arg in stream.feed(read_calc(fd, 1024)):
                if cmd == "Z":
                    return
                if not a.quiet:
                    print("  ", cmd, arg if arg is not None else "")
                for m in session.handle(cmd, arg):
                    send(m)
    except OSError:
        if not a.quiet:
            print("The calculator disconnected.")
    finally:
        os.close(fd)


if __name__ == "__main__":
    main()
