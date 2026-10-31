# CalcGPT wireless protocol (version 2: cloud server, Wi-Fi setup from the calculator)

Three parts talk to each other:

    CALCGPT (calculator)  <--USB serial-->  bridge board (ESP32-S3)  <--TLS over Wi-Fi-->  CalcGPT server (cloud, or the Mac)

The calculator <-> server messages are the same CalcGPT messages as over the USB cable (section 1). The board passes them through,
and also answers a few messages of its own for Wi-Fi setup (section 3). Version 1 (calcnet on the Mac, `CALCNET1`) keeps working
unchanged for the other Mac programs.

## 1. CalcGPT messages (unchanged)

Calculator -> server:

| Bytes | Meaning |
|---|---|
| `Q` len text | a question (len = 1 byte, text ASCII) |
| `N` | new chat |
| `S` | stop the answer |
| `Z` | the program is quitting |
| `R` | send the model list again |
| `M` index | pick model number `index` (1 byte) |

Server -> calculator:

| Bytes | Meaning |
|---|---|
| `A` len text | a piece of the answer |
| `T` len text | a short status for the footer (len 0 clears it) |
| `D` | the answer is complete |
| `K` | a model list follows (clears the old one) |
| `L` len label | one model name |
| `U` index | the model in use |
| `C` | clear the chat |

## 2. Board <-> server login (`CALCNET2`)

Over TLS to the server's name on port 443 (any network), or plain TCP on the home network.

1. Server -> board: `CALCNET2` + 16 random bytes (the challenge).
2. Board -> server: device id (16 bytes, ASCII, padded with zero bytes) + HMAC-SHA256(device secret, challenge + `calc`) (32 bytes).
   The device secret is stored as hex; the HMAC key is the decoded bytes (32 bytes for a 64-character secret), not the hex text.
3. Server -> board: `OK`, or it closes the connection (unknown id, wrong answer, device switched off, or too many failures from
   that address: after 5 failures within a minute it drops new connections without a hello until things go quiet).
4. From then on, section 1 messages both ways. The server sends the model list (`K`, `L`..., `U`) first.

Each board has its own id and secret (made by `cloud/add_device.py`), so one leaked board can be switched off alone. The server
holds the OpenAI key and limits each device to a number of questions per day (`T` "Daily limit reached" + `D` after that).

If the server says `CALCNET1` instead, the board uses the old login (32-byte HMAC with the shared secret) and passes bytes through
untouched (for REELS, BROWSER and the other Mac programs).

## 3. Messages between the calculator and the board itself (never sent to the server)

The board reads the calculator's messages (section 1 framing) whenever it is offline or logged in with `CALCNET2`, and keeps these
for itself. Which server it picks follows the program: the first byte from the calculator is `I` (CALC) or `R` (CALCGPT) for
programs that need the CalcGPT server, so the board leaves a `CALCNET1` relay it had joined (at home, calcnet comes first) and
logs in to the `CALCNET2` server instead; REELS (`S`) and BROWSER (`Q`, `G`, `X`, `K`, `Z`) stay on calcnet:

Calculator -> board:

| Bytes | Meaning |
|---|---|
| `W` | scan for Wi-Fi networks |
| `J` len ssid len password | join this network and remember it (password len 0 = an open network, or a remembered one: the board uses the password it saved) |
| `I` | send the connection state (`V`) now |

Board -> calculator (sent only between whole server messages, never in the middle of one):

| Bytes | Meaning |
|---|---|
| `V` len text | the connection line, e.g. `Wi-Fi: Home | online`, `Wi-Fi: none`, `Wi-Fi: School | no server` |
| `X` count | a Wi-Fi list follows, `count` entries (1 byte) |
| `Y` len entry | one network: byte 0 = signal 0-4, byte 1 = `*` locked or space, byte 2 = `+` remembered or space, then the name |
| `T` len text | a status, e.g. `Joining School...`, `Wrong password?`, `Connected to School` |

While the board has no server connection it answers `R` itself with `K` (an empty model list), `T` "No internet: Mode = Wi-Fi", and
`D`, so CALCGPT never waits forever. A `Q` asked while offline gets `T` "Not connected yet" + `D`.

The board sends `V` when CALCGPT starts (on `R` or `I`) and every time the state changes.

## 4. Remembered networks

The board keeps up to 8 networks in its flash (NVS), newest first; the networks built into the firmware come after them. A join
that works moves that network to the front; when the list is full the oldest is dropped. It tries the remembered networks it can
see, strongest signal first.

## 5. Tools: ChatGPT acting on the calculator (2026-10-30)

The model answers with tools (`calcgpt/mac/tools.py`, OpenAI function calling). Server-side tools (a calculator, triangle /
quadratic / linear / system solvers, statistics, trig values, number facts, fractions, unit conversion, points, dates) run on the
server and never touch the link. Calculator-side tools put a card on the calculator's screen. Their messages ride inside the
existing frames so the board's firmware passes them through unchanged:

| Bytes | Meaning |
|---|---|
| `T` len `0x01` text | server -> calculator: a tool request. `text` is `name\|part\|part...` (254 bytes at most): `form\|Title\|hint\|a\|b\|c`, `choice\|Title\|opt\|opt`, `answers\|Title\|rule\|line\|line`, `steps\|Title\|line\|line`, `triangle\|a\|b\|c\|A\|B\|C`, `parabola\|a\|b\|c`, `lines\|a\|b\|c\|d`, `circle\|deg`, `dots\|n\|n...`, `decimals\|n`, `status` |
| `Q` len `0x01` text | calculator -> server: the reply. `form\|a=3\|b=4` (empty fields left out) or `form\|cancel`; `choice\|option` or `choice\|cancel`; `ok` for a card that needs no answer; `status\|time=9:05\|battery=3 of 4\|online=yes`; `error\|why` |
| `Q` len `0x01` `hello\|tools=1` | calculator -> server, once after the model list arrived: this program can show cards. Without it the server offers the model no calculator tools (MATH's Ask ChatGPT, or an older CALCGPT) |

A form or a choice waits for the user (up to 180 s; Y= / a new chat stops the wait). The other cards answer `ok` at once and stay
on the screen until Enter or Clear. A `0x01` reply is never counted as a question for the daily limit. Calculator side:
`calcgpt/src/tools.c` (parsing, keys, replies) and `math/src/cards.c` (drawing).
