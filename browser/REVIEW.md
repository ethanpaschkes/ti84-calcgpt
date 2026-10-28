# A web browser that runs only on the calculator: review

Written 2026-10-23 night. **Verdict: yes, for a text-only reader browser. No, for the full modern web.**

## What "doesn't show on the Mac" means here
The Mac runs a small background script (`browserd.py`) with no window. It fetches web pages over the Mac's
internet, turns each page into plain text rows, and sends only those rows over the USB cable. The calculator draws
everything itself, in its own style. The Mac never displays the page.

## What is realistic
| Thing | Verdict | Why |
|---|---|---|
| Search and read articles, docs, news, Wikipedia | **Yes** | Pages become text; the calculator draws it sharply with its own font, which is far more readable than a squeezed screenshot. Very little data goes over the cable, so it is fast. |
| Follow links, back button, bookmarks | **Yes** | The Mac keeps the link list and history; the calculator sends "open link 7" or "back". |
| Typing a search or address | **Yes, but slow** | The calculator keyboard has no letters printed on it; the Alpha layer works (same layout as the SOLVER and control mode). Bookmarks and search suggestions reduce typing. |
| Pictures | **Later** | Possible as small dithered black-and-white or 16-colour thumbnails using the picture messages that already exist. Not in the first version. |
| JavaScript sites, logins, video, web apps | **No** | The page is read as plain HTML. Many modern sites show an empty page without JavaScript. |
| Google results | **Not directly** | Google forbids automated scraping and blocks it. The first version uses DuckDuckGo's plain HTML page and Wikipedia's official API (no key, free). Google's own search API needs a key and has a daily limit; it could be added later. |

## "A mix of the calculator UI and Google"
Calculator side: the same top bar, colours and light-blue selection as SOLVER, with "made by Ethan Paschkes" in the bar.
Search-engine side: a large coloured wordmark (named **CalcSearch**; Google's real logo and name are not used),
a rounded search box, and results shown as blue title, green address, grey snippet. Selected result or link gets a light-blue band.

## How it works
```
calculator (BROWSER)  <--- USB serial --->  Mac (browserd.py, no window)  <--- internet --->  web
 draws everything                           fetch, clean up, wrap to 38 columns, keep links + history
```
Calculator to Mac: `Q` search text, `U` typed address, `K` link number, `X` back, `Z` quit.
Mac to calculator: home page (bookmarks), then a page as rows: each row has a style (heading, normal, result title,
address, snippet), its text (at most 38 characters), and up to 3 link spans (link number, start column, end column).
Pages are cut at 400 rows (about 14 KB of calculator memory).

## Limits to expect
- Text only. Tables become lines of cells; code and maths may look rough.
- Typing speed. A search is typed one calculator key at a time.
- DuckDuckGo's HTML page is not an official API: if it changes or blocks, the browser falls back to Wikipedia search.
- Everything the calculator asks for goes out through your Mac's internet connection, under your IP address.
  Only `http` and `https` are allowed; the browser refuses `file:` and local-network addresses.
- Nothing here has run on the real calculator yet (see `README.md` for the morning test list).

## Build plan and what exists
1. `mac/browserd.py` - fetch, reader view, search (DuckDuckGo + Wikipedia), link table, history. **Built and tested on the Mac** with a local test website.
2. `src/pagemodel.c` - the calculator's page store, scrolling and link jumping, with no drawing code. **Built, tested on the Mac.**
3. `src/typing.c` - which key types what (identical to `reels/mac/calckeys.py`, checked by a test). **Built, tested on the Mac.**
4. `src/main.c` - the screens (home, results, page) and the USB link. **Compiles; the drawing is not testable without the calculator.**
5. Later ideas: thumbnails of images, search suggestions, reading-position memory, a "send this page to read later" list.
