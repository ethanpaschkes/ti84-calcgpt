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

## Parts check 2026-10-30 (school engineering room, loose Arduino parts)
Found and taken: NPN transistor (2N2222-class; replaces the 2N7000 MOSFET for the ID-pin pull-down: base through 10 k to the
GPIO, emitter to GND, collector to ID), 2x 100 k resistors (VBUS sense divider), passive buzzer 12 mm (message beep on a GPIO),
slide switch 3-leg (box power switch). Not found: 1N4007/Schottky diode, boost converter, charger board (the room has no
power modules). Sound: no I2S mic exists in Arduino kits; the KY-038 "sound sensor" cannot capture speech -> if voice input is
wanted later, order an INMP441 (I2S) module. Still to order: boost 5 V with EN (Adafruit MiniBoost TPS61023 #4654 or MT3608),
1N5819 Schottky diodes, LiPo 150 mAh (Adafruit 1317) unless the RC-plane cell fits, charger (TP4056 or Adafruit 1904) only if
the RC charger stick is 0.3 A or more.

## Inventory from Ethan's photos, 2026-10-30 15:20 (home desk)
Have: Lonely Binary ESP32-S3 DevKitC-1 + USB-C cable; 2x mini-B male -> USB-A female OTG adapters (one opened, grey ID wire
attached); 6x USB-A female breakout boards (VBUS/D+/D-/GND pads, one with wires soldered); school parts (NPN transistor, buzzer,
push button, slide switch, 2x 100 k); a donor TI-84 Plus CE in pieces (GECKO mainboard with mini-B, keypad, grey shell);
batteries: black 1S 3.7 V 500 mAh 651740 pack with white JST (THE box battery; 1.85 Wh), SkyViper drone 1S pack (red plug),
small silver 1S pack (white plug); Urgenex 1800 mAh 2S 7.4 V (NOT usable, two cells); Elegoo MB102 breadboard power module
(steps down only, skip); Grove speaker board (could replace the buzzer) and Grove sound sensor (clap detector, not a mic);
a 220 uF electrolytic capacitor (smooth the 5 V rail).
Not owned: boost, charger, Schottky diode, JST PH pigtail, heat shrink, soldering iron (Ethan has none, home or school).
BUY LIST (~$50): Adafruit MiniBoost TPS61023 #4654 or MT3608 2-pack; TP4056 charger 5-pack (plain version; add protection only
if the 500 mAh pack has none); 1N5819 x20; JST PH 2.0 pigtails 10-pack; heat-shrink assortment; a basic soldering iron kit
(~$20, temperature-controlled, with solder and a stand).
Update 2026-10-30 15:40: no spare power bank at home (a power-bank board would have replaced boost + charger). Ethan HAS the
SkyViper USB charger (current unreadable, assume 0.5-1 A, fine for the 500 mAh pack) -> charger OFF the buy list; its plug
fits the SkyViper pack only, so JST pigtails stay. Final buy list: boost (TPS61023 #4654 or MT3608), 1N5819 x20, JST PH
pigtails, heat shrink (or electrical tape for the first build), soldering iron kit (or one sitting at the school iron).
Size target (Ethan, 2026-10-30 15:50): the box must be about a LARGE USB DRIVE (~70 x 25 x 12 mm). The slim DevKitC-1 box
(82 x 31 x 14) misses that. Proposal: final box on a Seeed XIAO ESP32S3 (21 x 17.5 mm, same chip, USB-C, on-board LiPo charger
on the BAT pads -> no TP4056, no pigtail-to-charger wiring) + MiniBoost + the 500 mAh pack + USB-A breakout on one end
= about 48-62 x 22 x 12 mm. DevKitC-1 stays the development board; firmware needs only a pin table for the XIAO
(role.c / box_pins.h). Cost ~$7.50. Not yet approved by Ethan.
2026-10-30 15:55: XIAO ESP32S3 PROPOSED for the final box, NOT decided (Ethan wants to see the shop links first). Household tools confirmed: hot glue gun, multimeter, spare USB
cables to cut, tape/zip ties. Heat shrink dropped (hot glue + tape). CANDIDATE BUY LIST (to review): Seeed XIAO ESP32S3 (~$7.50), Adafruit
MiniBoost TPS61023 #4654 (~$5) or MT3608, 1N5819 x20 (~$5), JST PH 2.0 pigtails (~$6), soldering iron kit (~$20) unless the
school iron is used. Next firmware step: pin table + sdkconfig for the XIAO in esp32s3-loader (BOX_HW), keeping the DevKitC-1
build. Box model (wireless/box/*.scad) needs a XIAO variant.
2026-10-30 16:30: Ethan picked the Seeed XIAO ESP32S3 SENSE (camera OV2640 + PDM digital mic + microSD, $23.99 on Amazon
B0C69FFVHH, $13.99 at Seeed) for the candidate list. Only the S3 variants can be USB host (C3/C6 XIAOs cannot -> never
substitute). Future features it unlocks (after the plain box works): photo-solve (capture JPEG -> server -> GPT vision -> answer
+ drawn diagram on the calculator), voice questions (PDM mic -> server speech-to-text), photo preview on the 320x240 screen.
Camera board adds ~2 mm thickness. Classroom note: looks like a cheating device; homework tool only. Still NOT ordered.
2026-10-30 17:00 ORDERED (Ethan): XIAO ESP32S3 Sense (arrives Nov 10), generic TPS61023 boost module with EN, 60 W soldering
iron kit, and probably JST-PH pigtails (or a socket harvested from a spare charger) -- the non-XIAO items arrive ~Oct 31/Nov 1.
Plan: week of Nov 1 = test rig on the DevKitC-1 (battery + boost + switch + opened adapter): prove the loader wakes a
calculator from battery and installs CALC. Nov 10 = move the wiring to the XIAO, print the small case at school. Before the
10th: write the XIAO pin table (box_pins.h / BOX_HW) so the firmware builds for both boards.
Final size spec with the XIAO Sense (2026-10-30 17:30), 1.5 mm printed walls, adapter outside:
parts: XIAO Sense 21 x 17.8, ~8 mm tall with camera board; battery 651740 = 40 x 17 x 6.5; TPS61023 module ~18 x 11 x 3;
USB-A socket 14 x 14 x 6; slide switch 9 x 4 x 4. The 12 x 9.5 mm buzzer does not fit layout A; use a flat piezo disc or drop it.
Layout A (stacked, battery under XIAO + boost): 58 x 22 x 17.5 mm outer. Layout B (side by side): 58 x 39 x 11 mm.
Large USB drive for reference: ~70 x 22 x 11. Recommended: A (USB-drive length and width, 6 mm thicker, camera window on top,
USB-A on the short end, switch on the long side). Off-the-shelf fallback for A: a plastic Tic Tac box (68 x 43 x 20) with foam;
no metal tins (block Wi-Fi). Primary case: 3D print at school from wireless/box (needs a XIAO variant of calcbox.scad).
Verified from Seeed docs 2026-10-30 18:00: XIAO Sense charger is only 50-100 mA (500 mAh pack = 5-10 h from the calculator);
camera module varies by batch (OV2640 / OV3660 / OV5640); 8 MB octal PSRAM + 8 MB flash (sdkconfig must say OCTAL PSRAM);
PDM mic on GPIO41 (data) / GPIO42 (clock); microSD over SPI, 32 GB FAT max. USB D+/D- exist ONLY on the XIAO's USB-C socket
(no pads) -> the box needs a short USB-C plug pigtail inside (cut from a spare cable), +~10 mm: layout A becomes ~68 x 22 x 17.5.
UNVERIFIED: USB host through the XIAO's USB-C socket (chip supports it; first XIAO test proves it). Capability table given to
Ethan (old bridge vs new box): everything Wi-Fi/ChatGPT/CALC carries over; new = self-loading, battery, photo-solve, voice,
SD cache, standalone; far-fetched = wake word, face detect, live video, BLE phone/keyboard, USB-keyboard output, speech out.

## PRODUCT PLAN (Ethan, 2026-10-30 18:30): build 10 boxes and sell them
- Price: $89 each with the first year of ChatGPT included (floor $69). Parts ~$42/unit at Amazon prices, ~$32 in bulk from
  Seeed/AliExpress; server ~$5/month + ~$1-2/month per active user. ~1 h soldering per unit.
- Buttons: BUTTON 1 = INSTALL (box as USB host: wake calculator, install the arTIfiCE hook if OS >= 5.5, copy CALC + apps,
  start CALC). BUTTON 2 = SETUP (hold: box becomes hotspot "CALC-xxxx" with a captive portal page for the buyer's Wi-Fi name +
  password; short press later = photo shutter). Two tactile push buttons from school.
- Must exist before unit 1 ships: loader proven on OS 5.4 and 5.5+; Setup hotspot + portal; public server on Fly.io with one
  account per device, usage caps, OpenAI key never on the box; protected cells only, drop-tested case, ground shipping;
  quick-start card; a parent handles payments/tax/returns (Ethan is 15); sell as a homework tool, expect school bans.
Form-factor options discussed 2026-10-30 19:00: (A) clip-on printed BACK PANEL replacing the slide-off cover, module flat
inside (~4 mm thicker), short cable looping into the mini-B port: calculator untouched, any CE, removable. (B) module in the
BATTERY BAY with the stock 1200 mAh pack swapped for the 500 mAh one; needs 4 wires soldered to the main board's USB pads +
an internal plug: invisible but not "any calculator", and halves the calculator's battery life. Not decided.
