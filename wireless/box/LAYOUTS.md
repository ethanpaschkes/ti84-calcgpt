# Calc box layouts: where the pack goes on the back of the TI-84 Plus CE

This compares every way of building the Calc box (the ESP32-S3 DevKit, the LiPo, the charger and the 5 V boost in a
3D-printed pack stuck on the back of the calculator). Every number comes from the fit report that `ti84ce.scad` prints when
you render a scene, and all the scenes below print **PASS**. (A fit report is a list of checks the model runs on itself. PASS
means the pack is at least 4 mm inside both long sides of the back and within its length, every part fits inside the box, the
cable inside the box misses every part, and the Dupont wires are long enough.)

Words used below:

- **Soldered**: you cut the DevKit's header pins short and solder thin wires to them. **Dupont**: you push the plastic
  jumper-wire plugs you already have onto the pins. That needs no soldering, but the plugs are 14 mm long, so the box gets thick.
- **Flat**: the LiPo, charger and boost sit *beside* the board in one layer (wider, thinner). **Stacked**: they sit *under*
  the board (narrower, thicker). **Module**: no DevKit at all, just the bare ESP32-S3 module on a small custom board.
- **Top / bottom**: which end of the back the pack sits at. **Side exit**: the cable leaves through the box's side wall at the
  end nearest the calculator's USB port. **End exit**: the cable leaves through the box's end wall.
- "Right edge" always means the calculator's right edge **seen from the front** (where the mini-USB port is). When you look at
  the back, that edge is on your left.
- Thickness: the bare calculator is 18.7 mm at the keys (20.3 mm with its slide cover on).

## The options

| Option | Pictures | Thickness added (total at the pack) | Footprint (L x W) | UART port, BOOT button, cable end | Cable route and length | Pros | Cons |
|---|---|---|---|---|---|---|---|
| **(a) Soldered + flat, top** ⭐ | `renders/a_top_flat_soldered.png`, `renders/a_top_flat_soldered_right.png`, `renders/a_top_flat_soldered_exploded.png` | **10.1 mm** (28.8 mm) | 82.5 x 50.7 mm, top 83 mm of the back | UART USB-C pocket opens at the top end, on the port side; BOOT hole 27 mm from the top, 28 mm in from the right edge; cable slot in the side wall facing the right edge, 3 mm above the port | out of the side wall, 22 mm across the back to the right edge, then 3 mm down to a 90° mini-B plug: **25 mm** | thinnest DevKit build; shortest cable; flashing port and BOOT easy to reach; battery door stays free; small print | needs soldering and cutting the header pins |
| (a, end exit) | `renders/a_top_flat_soldered_end.png`, `renders/a_top_flat_soldered_end_right.png` | 10.1 mm (28.8 mm) | 82.5 x 50.7 mm | as (a), but the cable leaves the end wall at the top | along the top edge, then 81 mm down the right edge: **129 mm** | same box | 5x more cable outside to snag on |
| (b) Soldered + flat, bottom | `renders/b_bottom_flat_soldered.png`, `renders/b_bottom_flat_soldered_right.png` | 10.1 mm (28.8 mm) | 82.5 x 50.7 mm, bottom 85 mm of the back | UART pocket at the bottom end, on the left half; BOOT 27 mm from the bottom; cable slot 28 mm below the port | out of the side wall, 22 mm across, 28 mm up the right edge: **50 mm** | same thickness as (a) | covers the battery door (the door position is a guess); cable twice as long; the flashing port is at the end you hold |
| (b) Soldered + stacked, top | `renders/b_top_stacked_soldered.png`, `renders/b_top_stacked_soldered_right.png` | 16.5 mm (35.2 mm) | 82.5 x 30.6 mm | as (a); cable slot 1 mm above the port | 32 mm across the back, 1 mm down: **35 mm** | 20 mm narrower than (a) | 6.5 mm thicker than (a) |
| (c) Slide cover with the pocket (soldered + flat) | `renders/c_cover_flat_soldered.png`, `renders/c_cover_flat_soldered_right.png`, `renders/c_cover_pocket_only.png` | 8.5 mm on top of the cover (28.8 mm, same as (a)) | 82.5 x 50.7 mm on a new full-size cover | as (a) | as (a): **25 mm** | the whole pack slides off with the cover in a second; the cover plate is the box's floor, so no tape | the cover is shaped from photos only (its sliding fit needs a ruler); a ~90 x 202 mm print needs a large print bed; you lose the cover as a screen protector while the pack is on it |
| (d) Dupont + stacked, top | `renders/d_top_stacked_dupont.png`, `renders/d_top_stacked_dupont_right.png` (and `renders/d_bottom_stacked_dupont.png` for the bottom) | 28.2 mm (46.9 mm) | 85.5 x 30.6 mm | UART pocket at the top end; BOOT 30 mm from the top; cable slot 1 mm above the port | your USB-A plug + OTG adapter make a rigid **68 mm stick straight out of the port** (64 mm past the edge); the flexible Dupont wires run 114 mm from its end back to the slot (plus 127 mm inside the box = 241 of your 300 mm; the rest coils in the box) | **nothing to solder: you can build it this week** with what you have | adds 2.8x the thickness of (a); the stick pokes out sideways |
| (e) Bare module, top | `renders/e_top_module.png`, `renders/e_top_module_right.png` | 10.1 mm (28.8 mm) | 73.1 x 45.3 mm | no USB-C: flash over the lead's D-/D+ wires with BOOT held; BOOT hole 27 mm from the top; cable slot 10 mm above the port | 24 mm across the back, 10 mm down: **35 mm** | smallest footprint | no thinner than (a): the 6.4 mm charger sets the height in both; needs a custom carrier board; flashing is awkward |

The box drawings on their own are `renders/box_flat_soldered.png`, `renders/box_stacked_soldered.png`, `renders/box_stacked_dupont.png`,
`renders/box_flat_dupont.png`, `renders/box_module_soldered.png` and `renders/box_module_dupont.png` (lid lifted, side exit).

## Recommendation

**Build (a): soldered + flat, at the top of the back, cable out of the side wall.** It adds **10.1 mm** (28.8 mm in total at
the pack), needs only **25 mm** of cable from the box to a 90° mini-B plug, and takes **82.5 x 50.7 mm** of the back.

One-line verdicts (thickness / cable route / convenience):

- **(a) soldered + flat, top:** thinnest you can build with the DevKit / shortest cable, it leaves right beside the port / flashing port and BOOT at the top end, battery door free. **The one to build.**
- **(a) with end exit:** same box / 129 mm of cable along two edges, easy to snag / no gain. Use the side exit.
- **(b) soldered + flat, bottom:** same thickness / 50 mm cable up the edge / covers the battery door, and you flash it at the end you hold. Top wins.
- **(b) soldered + stacked, top:** 6.5 mm thicker / 35 mm cable, fine / only worth it if 20 mm less width matters more than thickness.
- **(c) slide cover with pocket:** same total thickness as (a) / same 25 mm cable / takes off in one slide, but print it only after you have measured the cover with a ruler. **The upgrade after (a).**
- **(d) Dupont + stacked, top:** 28 mm thick / a rigid 68 mm stick out of the port plus 114 mm of loose wire / **buildable this week with no soldering.** The stick is the price of no soldering.
- **(e) bare module, top:** no thinner (the charger sets the height) / 35 mm cable / smaller, but needs a custom board and a new way to flash. Not now.

How to stick (a) on: thin double-sided tape (about 1 mm, not counted above). The original slide cover should still slide over
the front for storage, because its side lips only reach 5 mm up the sides and the pack stays 18 mm away from each edge. Check
this on the real calculator.

What changed in the box design (calcbox.scad) while making this study:

- The flashing (UART) USB-C socket used to sit 9.6 mm behind a 4 mm-high hole. A normal USB-C plug's metal part is only about
  6.6 mm long, so its plastic body has to go into the box, and the hole was too small for that. Now an open-topped pocket
  (13.4 mm wide) goes right up to the socket, with a matching notch in the lid.
- The lid's inner lip (the rim that grips the walls) hit the charger and the antenna end. It now has a notch wherever a part reaches up into it.
- The board is held at its end by small tabs on the side walls, so the cable can pass underneath.
- `cable_exit = "side"` routes the cable along the floor of the box to a slot in the side wall near the antenna end.

## Needs your ruler

The model is only as good as these guesses. Measure them, put the numbers into the files (they are marked `// MEASURE` or
`// PHOTO`), render again and re-read the fit report:

1. **Length: done.** Ethan measured the bare body at about 193 mm on 2026-10-29 (TI publishes 192.8), so `body_l = 192.8` and every
   photo height is scaled by 0.962 (`y_fix`). The port is now at 109 mm from the bottom. **Still measure the width** (cover off) and set `body_w` if it is not 86.9.
2. **The battery door's size and position on the back** (`door`, `door_y0` in `ti84ce.scad`). There is no photo of the back, so it is a guess.
3. **The RC plane LiPo's real size** (`lipo` in `calcbox.scad`; the guess is 17 x 30 x 4 mm).
4. **The Dupont plug housing length** (`dupont_hsg`; the guess is 14 mm). That one number sets most of option (d)'s 28 mm thickness.

Also worth measuring: the charger you will really use (the RC charger stick is bigger than the Adafruit board in the model), the
plug body of your USB-C flashing cable (the pocket fits up to 12.4 x 6.5 mm), a 90° mini-B plug (`mini_ra`), and the pin stubs
after cutting (`stub_h`).

## How to make the pictures again

Each command runs OpenSCAD (the 3D modelling program) and saves a picture. `-D` sets a setting for that run only:

```sh
openscad -o renders/a_top_flat_soldered.png --viewall --autocenter --imgsize=1200,900 --camera=0,0,0,235,0,25,0 \
  -D 'scene="pack_top"' -D 'pack_wiring="soldered"' -D 'pack_layout="flat"' -D 'pack_cable_exit="side"' ti84ce.scad
```

Scenes: `pack_top`, `pack_bottom`, `pack_cover`, `pack_top_exploded`. The fit report appears in the terminal (the lines
starting with `ECHO:`). `python3 make_viewer.py` rebuilds the 3D viewer `calc-box-3d.html` (it needs an internet connection to open, for three.js).
