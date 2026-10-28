# Wireless TI-84 Plus CE with an ESP32-S3: parts, wiring, firmware

The calculator plugs into an ESP32-S3 board through a USB OTG adapter. The calculator powers the board, the board joins
Wi-Fi, and `calcnet.py` on the Mac turns that connection into a serial port that looks exactly like the USB cable:

    calculator --OTG adapter--> USB-A-to-jumper cable --4 wires--> ESP32-S3 board ~~Wi-Fi~~> calcnet.py on the Mac

Tested on 2026-10-28 with a TI-84 Plus CE running CALCGPT and an ESP32-S3-DevKitC-1 style board: the board starts when CALCGPT
opens, logs in to the Mac within a few seconds, and answers stream back over Wi-Fi.

## What to buy (about $25-35)

| Item | Notes |
|---|---|
| **ESP32-S3 dev board** with pin headers, such as the *ESP32-S3-DevKitC-1* or a clone (two USB-C ports marked **UART** and **USB**) | The pins `5V`, `GND`, `19` and `20` must be on the headers. Any memory size works (N8, N16R8, ...). |
| **USB 2.0 A (male) to 4-pin Dupont cable**, sometimes sold as a "motherboard header adapter" | Red, white, green and black wires, each ending in a female socket that slides onto a board pin. |
| **USB-A female to mini-USB male OTG adapter** | Makes the calculator the USB host, so the calculator powers the board. A plain mini-USB cable will **not** work here. |

## Wiring (everything unplugged)

| Cable wire | Signal | Board pin |
|---|---|---|
| Red | 5 V | **5V** |
| Black | Ground | **GND** (any `G`) |
| White | D- | **19** |
| Green | D+ | **20** |

Wire colours are usually standard, but check: if the calculator never connects, try swapping white and green.

**Keep the board's own USB-C ports empty while it is wired to the calculator.** The port marked USB shares pins 19 and 20, so a
charger or computer plugged in there gets in the way of the calculator's data. A phone charger on that port also stops the board
from staying on Wi-Fi.

## Firmware (ESP-IDF 5.4)

1. Install [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32s3/get-started/) 5.4.
2. Make the secret the board and the Mac share: `python3 wireless/calcnet.py token`. It is stored in `~/.calcnet-token`.
3. List your Wi-Fi networks in `~/.calcnet-wifi.txt`, one per line, as `name | password` (an open network is `name |`).
   The first line is your home network.
4. Write the board's settings and build:
   ```
   cd wireless/esp32s3-bridge
   python3 make_secrets.py --mac-ip 192.168.1.50     # your Mac's address on your home Wi-Fi
   idf.py build
   ```
   `main/secrets.h` now holds your Wi-Fi passwords and the secret. It is ignored by git: never share it.
5. Load it onto the board: unplug the calculator wiring, plug the board's **USB** port into the computer, hold **BOOT**, press and
   release **RST**, release **BOOT**, then run `idf.py -p /dev/cu.usbmodemXXXX flash` (your port name will differ). Unplug the board.

## Using it

1. Mac: `python3 wireless/calcnet.py listen` (leave it running).
2. Mac: `CALC_PORT=~/.calc-port python3 calcgpt/mac/calcgptd.py`. This works for any Mac program in this repository.
3. Calculator: plug in the adapter and start **CALCGPT**. The board's light comes on, and the model list appears within about 10 seconds.

To watch what the board is doing, run `python3 wireless/boardlog.py`. Every 10 seconds the board sends a status line (uptime, why it
last restarted, Wi-Fi, and whether the calculator is connected) to UDP port 9999 on the Mac.

## How the firmware talks to the calculator (lessons learned)

- The calculator is the USB host, so the board must be a USB **device**. The firmware uses the ESP32-S3's built-in
  **USB Serial/JTAG** port as that device, a plain USB serial port the calculator's `srldrvce` library understands. That port is
  on pins 19 and 20 from the moment the chip powers on, so the calculator sets it up straight away.
- A software USB device (TinyUSB) does **not** work here. The chip shows the built-in port first; the calculator connects to it and never
  notices when TinyUSB takes the pins over later. That's also why the firmware's log output is kept off the USB port
  (`CONFIG_ESP_CONSOLE_SECONDARY_NONE`): otherwise the calculator receives the log text.
- The calculator sends a start-of-frame packet every millisecond, but with small gaps now and then. The firmware only counts the
  calculator as gone after 3 seconds without one. Reacting to every gap kept dropping the link.
- The board runs from the calculator's battery, so a session drains the calculator faster than usual.

## Security

Anything that knows the secret can act as the calculator, so keep `~/.calcnet-token` and `secrets.h` private. The secret is checked
with a challenge and response and never sent over Wi-Fi. The data after login is **not** encrypted, so use a network you trust.
