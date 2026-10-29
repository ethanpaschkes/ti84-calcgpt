# CalcGPT cloud server

CalcGPT on the TI-84 Plus CE without the Mac: the calculator's Wi-Fi bridge board connects over the internet to this
server, and the server asks a cloud model (OpenAI) and streams the answer back to the calculator screen.

    CALCGPT (calculator)  <--USB-->  bridge board  <--TLS over Wi-Fi-->  this server  <--HTTPS-->  OpenAI

- Each board has its own **device id and secret** (made by `add_device.py`), so one lost board can be switched off alone.
- The server holds the **OpenAI key**; boards never see it.
- Each board gets a **daily question limit** (200 by default, counted per UTC day).
- The login is `CALCNET2`, described in `wireless/PROTOCOL.md` section 2. The chat messages are the same as over the
  USB cable (section 1). The server reuses `calcgpt/mac/calcgptd.py` for the chat itself.
- Only cloud models are offered (the `openai` entries of `models.json`); a local `ollama` entry is skipped.

Files: `server.py` (the server), `add_device.py` (add or switch off a board), `test_server.py` (tests),
`Dockerfile`, `fly.toml` and `deploy.sh` (for Fly.io).

## Run it on this Mac

1. Give it the OpenAI key (once). Make a file `cloud/.env` holding one line, `OPENAI_API_KEY=sk-...`. Git ignores it.
   (Or reuse the Mac one: `cp calcgpt/mac/.env cloud/.env`.)
2. Add a board (see below).
3. Start the server from the project folder (`~/Developer/claude/ti84`):

       python3 cloud/server.py

   It listens on port 7979 as plain TCP, which is fine on your home Wi-Fi. Press Ctrl+C to stop it.
   Options: `--port N`, `--devices PATH`, `--models PATH`, and `--cert cert.pem --key key.pem` to do TLS itself.

Models come from `cloud/models.json` if you make one, otherwise from `calcgpt/mac/models.json`.

Run the tests (they use a fake model, so they cost nothing): `python3 cloud/test_server.py`

## Add a board

    python3 cloud/add_device.py "Ethan's board"            # --limit 50 for a different daily limit

It prints two lines, `DEVICE_ID=...` and `DEVICE_SECRET=...`. Put them into that board's `secrets.h`, and nowhere else.
The board's HMAC key is the 32 bytes those 64 hex characters stand for (the firmware's `make_secrets.py` reads these two
lines from a file and writes them into `secrets.h`).

- `python3 cloud/add_device.py --list` shows the boards (never the secrets).
- `python3 cloud/add_device.py --disable ID` switches a board off (`--enable ID` switches it back on). A running server
  notices at once: a switched-off board is dropped at its next question and can't log in again.

## Deploy to Fly.io

Fly.io runs the server on a small computer in the cloud with a public name like `calcgpt-yourname.fly.dev`. Fly does the
TLS (encryption) on port 443, so boards can connect from any network, including school Wi-Fi.

1. Install the Fly command-line tool: `brew install flyctl`
2. Make a Fly account and log in: `fly auth signup` (or `fly auth login`). It needs a credit card.
3. Open `cloud/fly.toml` and change `calcgpt-CHANGE-ME` to a name of your own, e.g. `calcgpt-ethan`.
4. Create the app without starting anything yet:

       cd ~/Developer/claude/ti84/cloud
       fly launch --no-deploy --copy-config --name calcgpt-ethan

   If it asks whether to tweak the settings, answer **No**. (`fly apps create calcgpt-ethan` does the same job.)
5. Make the small disk that keeps the boards list (1 GB, the smallest):

       fly volumes create calcgpt_data --size 1 --region ewr

6. Give the server the OpenAI key as a secret (Fly stores it encrypted; it never goes into Git or the image):

       fly secrets set OPENAI_API_KEY=sk-...

7. Deploy (this builds the server from only the files it needs, then starts it):

       sh ~/Developer/claude/ti84/cloud/deploy.sh

8. Add the boards on the server itself (the boards list lives on the Fly disk, not on the Mac):

       fly ssh console -C "python3 /app/cloud/add_device.py 'Ethan board' --devices /data/devices.json"

   Run `fly ssh console -C "python3 /app/cloud/add_device.py --disable ID --devices /data/devices.json"` to switch one off.
9. Point the board at `calcgpt-ethan.fly.dev` port 443 with TLS.
10. Watch the log: `fly logs`. One line per login and per question (the board's name, never the question).

To update the server later, run `deploy.sh` again. The boards list and usage stay on the disk.

## Costs

Check the current prices at fly.io/docs/about/pricing before you start. In October 2026 they were:

- the server (shared-cpu-1x, 256 MB, always on): about $2.19 a month
- the 1 GB disk: $0.15 a month
- the shared IPv4 address and the TLS certificate: free
- data out: $0.02 per GB (a calculator's answers are tiny)

So about **$2.50 a month** for Fly, plus the **OpenAI cost per question**, which is the part that grows with use. The daily
limit per board caps it. Fly has no free plan for new accounts (only a short trial), and it charges the card monthly.

## Security notes

- **Secrets:** `cloud/.env` (the OpenAI key), `cloud/devices.json` (every board's secret) and `cloud/usage.json` are ignored
  by Git and must never be shared. `devices.json` and `usage.json` are created readable only by you (permission 600).
- **Login:** the server sends a fresh random challenge each time and the board answers with an HMAC of it, so the secret
  itself never travels. Wrong answers are compared in constant time, and unknown ids take the same path as known ones.
- **Guessing:** after 5 failed logins within a minute from one address, that address's new connections are dropped
  without a hello until it goes quiet. On Fly the proxy passes each board's real address (PROXY protocol), so one
  attacker can't lock everyone else out.
- **Encryption:** on Fly the connection from the board is TLS (Fly's certificate for `*.fly.dev`). Running locally without
  `--cert` it is plain TCP: use that only on a network you trust.
- **Privacy:** the log records the board's name and the model, never the question text. The questions do go to OpenAI.
- **A lost or leaked board:** `add_device.py --disable ID` switches off just that one.
- **Spending:** the daily limit stops one board from running up the OpenAI bill. Also set a monthly budget limit in your
  OpenAI account settings.
