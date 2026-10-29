# The box: a self-powered loader + bridge for any TI-84 Plus CE (design, 2026-10-29)

Goal (Ethan): plug the box into any TI-84 Plus CE, with nothing else, and it loads CALC and friends onto the calculator, starts
CALC, and from then on is CALC's Wi-Fi link. Decision 2026-10-29: a small LiPo battery in the box, charged by the calculator
while CALC runs.

## Why a battery is needed at all

TI-OS powers the calculator's USB port only when the calculator is the USB **host** (a program such as CALC/CALCGPT turned USB
on, or it is sending). To receive files the calculator must be the **device** (TI Connect style), and as a device it does not
power the port (measured 2026-10-28: the board's LED stayed off in 2nd LINK > RECEIVE through the OTG adapter). So the box must
bring the energy for the first install. After that, every time CALC runs, the calculator is the host and powers (and charges) the
box, which is how it works today.

## One cable, two roles

The calculator's mini-USB port is a mini-AB port: the **ID pin** of the plug decides the calculator's role. ID to ground = the
calculator is the host (today's OTG adapter does exactly that). ID floating = the calculator is the device. The box controls the
ID pin with a transistor, so the same cable works both ways:

| Phase | ID pin | 5 V on the cable | Box's USB role | Box firmware |
|---|---|---|---|---|
| Install (box on battery) | floating | from the box (boost converter on) | host | loader.c: wakes the calculator, sends what is missing, starts CALC (`dusb_execute`) |
| Hand-over | pulled low | box 5 V off | restart as device (RTC flag) | role.c |
| CALC / CALCGPT running | low | from the calculator | device (USB-Serial/JTAG) | bridge.c over link_jtag.c; charger charges the LiPo |

Rule at power-on (role.c, `role_decide`): if the restart flag says "device", be the device. Else if 5 V is present on the cable
(the calculator is already the host), be the device. Else be the host. The BOOT button in device mode goes back to host mode
(re-install or restart CALC).


## What Ethan already has (Amazon orders of 2026-10-27, seen 10-29)

| Have | What it is | Role in the box |
|---|---|---|
| Lonely Binary ESP32-S3 N16R8 | a DevKitC-1 copy (same 25.4 x 62.8 board, 44 pins) with the pin headers already soldered on the underside | the brain |
| rgzhihuifz USB mini-B male to USB-A female adapter, 2 pcs | the OTG adapter (ID grounded inside), 36 mm long | the plug into the calculator; one gets opened for the ID wire |
| MEIRIYFA USB 2.0 A to 4-pin Dupont cable | USB-A plug, ~30 cm, four single female Dupont housings | today's link: housings on the 5V, GND, GPIO19, GPIO20 pins |
| MELIFE USB 2.0 breakout boards, 10 pcs | USB-A female sockets on tiny PCBs | a clean socket for a later version |
| EDGELEC 20 cm Dupont jumpers, 120 pcs | male/female jumper wires | the sense / boost EN / ID gate wires |
| 1S LiPo from an RC plane, probably a 1S USB charger stick | | battery; charger if the stick is 0.1-0.2 A |

Two ways to wire the box (`wiring` in `box/calcbox.scad`):
- **dupont** (what the parts above allow, no soldering of the board): headers stay, Dupont housings push onto the pins, the long
  wires coil in the end room. Box about 93 x 31 x 28 mm. Fine for the first working box.
- **soldered** (the slim one): headers cut flush, short wires soldered to the stubs. Box about 82 x 31 x 14 mm.

## Parts (first build on the DevKitC-1; a bare ESP32-S3-WROOM-1 module later)

| Part | Example | Role |
|---|---|---|
| LiPo 3.7 V, 150 mAh | Adafruit 1317 (~$6) | 30 s install at ~250 mA peak needs far less than one charge; also 1 h of bridge use with the calculator off |
| LiPo charger from 5 V | Adafruit Micro-Lipo MCP73831 (1904, ~$7) or TP4056 module | charges from the cable's 5 V when the calculator is the host |
| 5 V boost with enable pin | Adafruit MiniBoost TPS61023 (4654, ~$5) | makes the cable's 5 V during the install; EN = `PIN_VBUS_OUT_EN` |
| N-MOSFET, small | 2N7000 / BSS138 + 100 k pull-down | grounds the ID pin when `PIN_ID_LOW` is high |
| 2 resistors 100 k | | divider from the cable's 5 V to `PIN_VBUS_SENSE` (2.5 V = high) |
| Schottky diode | | so the boost's 5 V never back-feeds the calculator's 5 V (or power the ESP from the LiPo only and keep the two 5 V rails apart) |
| Cable | Ethan's existing OTG adapter (mini plug -> USB-A socket), opened: cut its ID-to-GND bridge and bring ID out on a wire to the MOSFET; plus the existing USB-A-to-Dupont jumper cable | the only plug into the calculator; no coupler or extra cable needed |

Pins (placeholders in `esp32s3-loader/main/box_pins.h`; change them to the wiring when it exists): VBUS sense GPIO4, boost enable
GPIO5, ID-low GPIO6. The ESP32-S3's own USB data pins stay GPIO19/GPIO20. `BOX_HW 0` = the DevKitC-1 as it is today (host mode
only, powered from its UART USB-C port or the 5V pin); `BOX_HW 1` = the box above.

First host-mode test (before the battery build): modified OTG adapter with ID left open -> jumper cable -> board (5V pin now
feeds the cable, with the board powered from its UART USB-C port). The calculator should wake as a device and the loader run.

Parts check 2026-10-29 evening: Ethan has a 1S RC-plane LiPo and probably a 1S USB charger stick (check its printed current: usable
if 0.1-0.2 A), the OTG adapter and the jumper cable. To order: boost (TPS61023 with EN), 2N7000 MOSFET, 1N5819 diodes, a resistor
assortment; the charger board only if the RC stick is 0.3 A or more.

## Power budget (rough)

- Install: Wi-Fi off would be best, but the loader runs with Wi-Fi up (logs over UDP): ~120 mA average, ~300 mA peaks, 30 s = 1 mAh.
- Bridge while CALC runs: the calculator's port gives 5 V at up to ~100 mA by the USB rule; the ESP32-S3 with Wi-Fi draws more at
  TX peaks, so the LiPo also smooths peaks. Charging at 100 mA refills a 150 mAh cell in under 2 h of CALC use.
- Idle (device mode, calculator off, nothing to do): add light sleep later; the first build may just run down and recharge.

## Open questions (need the hardware)

1. Does the calculator wake from OFF when 5 V appears on its port? (It does on a computer.) If not, the user turns it on first.
2. The 44 KB Inequalz app transfer in one go (only for OS 5.8.5 calculators).
3. How fast CALC's `usb_Init` follows `dusb_execute`: the box restarts into device mode in about 1 s; CALC retries its link,
   so a late start should be fine.
4. Whether `BOOT` is the right "install again" button for the final case.
