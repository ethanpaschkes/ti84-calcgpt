// Calc box, 1:1. A slim pack for the ESP32-S3 DevKitC-1 v1.1 (pin headers cut flush), a 1S LiPo, a charger and a 5 V boost,
// to stick on the back of a TI-84 Plus CE. Two printed parts: base (tray) and lid (friction fit). All sizes in mm.
//
// The DevKitC-1 geometry comes from Espressif's own drawing (DXF_ESP32-S3-DevKitC-1_V1.1_20220429): board 25.4 x 62.8,
// pin pads at x 1.27 / 24.13, y 7.96 + 2.54 k, USB-C connectors J2 (UART) at x 6.0 and J4 (USB) at x 19.4 on the y = 0 end,
// module 18 wide at x 3.75..21.75, running from y 43.65 to 69.15 (its antenna sticks 6.3 past the board's end).
// Board coordinates below: x across the board (0..25.4), y along it (0 = USB end), z up from the PCB's underside.
//
// Parts marked MEASURE are not Espressif's: measure yours and change the number.
//
// part = "base" | "lid" | "devkit" | "parts" | "wires" | "assembly" | "exploded" | "both"
//   openscad --export-format binstl -o base.stl -D 'part="base"' calcbox.scad   (same for lid, devkit, parts, wires)
part = "exploded";

// --- the DevKitC-1 (Espressif) -----------------------------------------------------------------
pcb_w = 25.4; pcb_l = 62.8; pcb_t = 1.6;
pin_x = [1.27, 24.13]; pin_y0 = 7.96; pin_pitch = 2.54; pin_n = 22; pin_hole = 1.0;
stub_h = 1.0;                       // what is left of the header pins under the board after cutting them flush (MEASURE after cutting)
module_x = [3.75, 21.75]; module_y = [43.65, 69.15]; module_h = 3.1;   // shield can; the antenna end has no can (same height assumed)
usbc_x = [6.0, 19.4]; usbc_w = 8.94; usbc_d = 7.3; usbc_h = 3.26; usbc_lip = 1.3;   // J2 UART, J4 USB; the plug opening sticks out 1.3 past y = 0
boot_xy = [7.0, 12.7]; reset_xy = [17.4, 12.7]; button_w = 4.2; button_d = 3.2; button_h = 2.5;
led_xy = [7.6, 28.2]; led_w = 5.0; led_h = 1.6;

// --- the other parts (MEASURE) -----------------------------------------------------------------
lipo = [17, 30, 4.0];               // 1S 150 mAh pouch from the RC plane: width across the board, length along it, thickness
charger = [15.5, 23, 4.0];          // Adafruit Micro-Lipo MCP73831 incl. its micro-USB; the RC USB charger stick is MUCH bigger: measure it
boost = [17, 12, 3.5];              // TPS61023 board: 17 across the board, 12 along it (its long side across)
mosfet = [4.6, 5.2, 3.7];           // 2N7000 TO-92 lying on its flat face, legs bent back (4.6 wide, 5.2 long, 3.7 thick)
diode = [2.7, 4.1, 2.7];            // 1N5819 DO-41 body
resistor = [2.4, 6.3, 2.4];         // 1/4 W
wire_d = 1.0;                       // 28 AWG silicone jumper wire with insulation (MEASURE yours; Dupont wires are ~1.2)

// --- the box -------------------------------------------------------------------------------------
wall = 1.6; floor_t = 1.6; lid_t = 1.6; lip_h = 2.5; lip_w = 1.2; fit = 0.25; corner_r = 3;
bay_h = max(lipo[2], charger[2], boost[2]) + stub_h + 0.5;   // room under the PCB
top_h = max(usbc_h, module_h, button_h) + 0.6;               // room over the PCB
end_room = 8;                        // the MOSFET, diode, resistors, the wires and the cable splice: at the USB end, beyond the connector lips
side_air = 1.0;                      // air each side of the board
far_air = 0.8;                       // past the antenna end

// --- derived -------------------------------------------------------------------------------------
in_l = end_room + usbc_lip + module_y[1] + far_air;
in_w = pcb_w + 2 * side_air;
in_h = bay_h + pcb_t + top_h;
out_l = in_l + 2 * wall; out_w = in_w + 2 * wall; base_h = floor_t + in_h;
// board frame -> box frame: box X = bx0 + by, box Y = by0 + bx, box Z = bz0 + bz
bx0 = wall + end_room + usbc_lip; by0 = wall + side_air; bz0 = floor_t + bay_h;
function B(p) = [bx0 + p[1], by0 + p[0], bz0 + p[2]];   // a board point to the box frame

// --- helpers -------------------------------------------------------------------------------------
module rbox(l, w, h, r) { hull() for (x = [r, l - r], y = [r, w - r]) translate([x, y, 0]) cylinder(h = h, r = r, $fn = 32); }
module at(p) { translate(B(p)) children(); }                           // place children at a board-frame point
module blk(p, s, c) { color(c) at(p) cube(s); }                         // a block: board-frame corner p, size s
module wire(pts, c = "red") { color(c) for (i = [0 : len(pts) - 2]) hull() { at(pts[i]) sphere(d = wire_d, $fn = 12); at(pts[i + 1]) sphere(d = wire_d, $fn = 12); } }
function pin(side, k) = [pin_x[side], pin_y0 + pin_pitch * k];        // side 0 = left (5V, G, 14, 13, 12...), 1 = right (G, G, 19, 20, 21...)

// --- the DevKitC-1 ---------------------------------------------------------------------------------
module devkit() {
    color("#17202a") difference() {
        at([0, 0, 0]) cube([pcb_l, pcb_w, pcb_t]);
        for (s = [0, 1], k = [0 : pin_n - 1]) at([pin(s, k)[0], pin(s, k)[1], -1]) cylinder(h = pcb_t + 2, d = pin_hole, $fn = 12);
    }
    color("silver") for (s = [0, 1], k = [0 : pin_n - 1]) at([pin(s, k)[0], pin(s, k)[1], -stub_h]) cylinder(h = stub_h + pcb_t, d = 0.64, $fn = 8);
    blk([module_x[0], module_y[0], pcb_t], [module_y[1] - module_y[0], module_x[1] - module_x[0], module_h], "#5d6670");
    blk([module_x[0], pcb_l, pcb_t + module_h], [module_y[1] - pcb_l, module_x[1] - module_x[0], 0.01], "#9aa3ad");   // antenna end marker
    for (x = usbc_x) blk([x - usbc_w / 2, -usbc_lip, pcb_t], [usbc_d + usbc_lip, usbc_w, usbc_h], "#8d9198");
    for (b = [boot_xy, reset_xy]) blk([b[0] - button_w / 2, b[1] - button_d / 2, pcb_t], [button_d, button_w, button_h], "#2c2c2a");
    blk([led_xy[0] - led_w / 2, led_xy[1] - led_w / 2, pcb_t], [led_w, led_w, led_h], "#e8e8e8");
}

// --- the other parts, laid out under the board in a row along y (charger, LiPo, boost), small parts beside them ---------
charger_p = [(pcb_w - charger[0]) / 2, 1.0, -stub_h - charger[2]];
lipo_p = [(pcb_w - lipo[0]) / 2, charger_p[1] + charger[1] + 1.0, -stub_h - lipo[2]];
boost_p = [(pcb_w - boost[0]) / 2, lipo_p[1] + lipo[1] + 1.0, -stub_h - boost[2]];
er_y = -usbc_lip - end_room;                                   // the end room spans board y from er_y to -usbc_lip
mosfet_p = [1.0, er_y + 1.0, -bay_h + 0.0];                      // small parts lie on the floor of the end room
diode_p = [pcb_w - 1.0 - diode[0], er_y + 1.0, -bay_h];
res_p = [[6.5, er_y + 0.8, -bay_h], [pcb_w - 6.5 - resistor[0], er_y + 0.8, -bay_h]];

module parts() {
    blk(charger_p, [charger[1], charger[0], charger[2]], "#1f4e6b");
    blk(lipo_p, [lipo[1], lipo[0], lipo[2]], "#b9bcc4");
    blk(boost_p, [boost[1], boost[0], boost[2]], "#1f6e8b");
    blk(mosfet_p, [mosfet[0], mosfet[1], mosfet[2]], "#2c2c2a");
    blk(diode_p, [diode[0], diode[1], diode[2]], "#3a3a3a");
    for (r = res_p) blk(r, [resistor[0], resistor[1], resistor[2]], "#e8d5b5");   // standing across the end room
}

// --- wires: from the pin stubs (underside) to where they go; the cable splice sits in the end room ---------------------
zw = -stub_h - wire_d / 2;             // wires run just under the stubs
splice = [pcb_w / 2, -usbc_lip - end_room / 2, zw];   // where the OTG adapter's five wires meet ours
function p3(p, z) = [p[0], p[1], z];
module wires() {
    // cable side: 5 V, GND, D-, D+ (right column) and ID
    wire([p3(pin(0, 1), zw), [pin_x[0], 0, zw], [splice[0] - 3, splice[1], zw]], "#d0342c");              // 5V pin -> VBUS
    wire([p3(pin(0, 0), zw), [pin_x[0], -2, zw], [splice[0] - 1.5, splice[1], zw]], "#1d1c19");           // GND
    wire([p3(pin(1, 2), zw), [pin_x[1], 0, zw], [splice[0] + 1.5, splice[1], zw]], "#f2f2ec");            // GPIO19 D-
    wire([p3(pin(1, 3), zw), [pin_x[1], -2, zw], [splice[0] + 3, splice[1], zw]], "#2a9d3f");             // GPIO20 D+
    // control: GPIO14 = 5 V sense (divider), GPIO13 = boost EN, GPIO12 = MOSFET gate (left column, right above the 5V pin)
    wire([p3(pin(0, 2), zw), [3, pin(0, 2)[1], zw], [3, -1, zw], [res_p[0][0] + 1, res_p[0][1] + 3, zw]], "#7a3fb8");
    wire([p3(pin(0, 3), zw), [3, pin(0, 3)[1], zw], [3, boost_p[1] + 2, zw], [boost_p[0], boost_p[1] + 2, zw]], "#7a3fb8");
    wire([p3(pin(0, 4), zw), [2.2, pin(0, 4)[1], zw], [2.2, -0.5, zw], [mosfet_p[0] + 2, mosfet_p[1] + 2.5, zw]], "#7a3fb8");
    // power chain: LiPo -> charger B+/B-, charger OUT -> boost VIN, boost 5V -> diode -> VBUS, cable 5 V -> charger IN
    wire([[lipo_p[0] + 2, lipo_p[1], zw - 1], [charger_p[0] + 2, charger_p[1] + charger[1], zw - 1]], "#e07b00");
    wire([[lipo_p[0] + 4, lipo_p[1], zw - 1], [charger_p[0] + 4, charger_p[1] + charger[1], zw - 1]], "#1d1c19");
    wire([[charger_p[0] + charger[0] - 2, charger_p[1] + charger[1], zw - 1], [pcb_w - 2.5, charger_p[1] + charger[1] + 2, zw - 1], [pcb_w - 2.5, boost_p[1], zw - 1], [boost_p[0] + boost[0] - 1, boost_p[1], zw - 1]], "#e07b00");
    wire([[boost_p[0] + boost[0] - 3, boost_p[1], zw - 1], [pcb_w - 1.5, boost_p[1] - 2, zw - 1], [pcb_w - 1.5, -1, zw - 1], [diode_p[0] + 1, diode_p[1] + diode[1], zw - 1]], "#d0342c");
    wire([[diode_p[0] + 1, diode_p[1], zw - 1], [splice[0] - 3, splice[1], zw - 1]], "#d0342c");
    wire([[charger_p[0] + 1, charger_p[1], zw - 1], [charger_p[0] + 1, -1, zw - 1], [splice[0] - 3, splice[1], zw - 1]], "#d0342c");
    // the five wires leaving to the OTG adapter
    for (i = [-2 : 2]) wire([[splice[0] + i * 1.5, splice[1], zw], [splice[0] + i * 1.5, -usbc_lip - end_room - wall - 6, zw]], i == 0 ? "#1f6fd6" : "#777");
}

// --- the box -----------------------------------------------------------------------------------------
module base() {
    difference() {
        rbox(out_l, out_w, base_h, corner_r);
        translate([wall, wall, floor_t]) rbox(in_l, in_w, in_h + 1, corner_r - wall);
        // UART USB-C through the end wall and the end room: a straight channel at the connector's height
        translate([-1, B([usbc_x[0] - usbc_w / 2 - 0.5, 0, 0])[1], bz0 + pcb_t - 0.4]) cube([wall + end_room + usbc_lip + 2, usbc_w + 1, usbc_h + 1.0]);
        // cable slot at the floor, centred on the splice
        translate([-1, B([splice[0], 0, 0])[1] - 5.5, floor_t]) cube([wall + 2, 11, 5]);
    }
    // corner ledges the board rests on; the LiPo and charger lie under it
    for (y = [B([0, 0, 0])[1] - side_air, B([pcb_w, 0, 0])[1] + side_air - 3])
        for (x = [B([0, 0.5, 0])[0], B([0, pcb_l - 3.5, 0])[0]]) translate([x, y, floor_t]) cube([3, 3, bay_h]);
    // a stop at the antenna end so the board cannot slide
    translate([B([0, module_y[1], 0])[0] + 0.3, wall, floor_t]) cube([0.5, in_w, bay_h + pcb_t]);
}

module lid() {
    difference() {
        union() {
            rbox(out_l, out_w, lid_t, corner_r);
            translate([wall + fit, wall + fit, lid_t]) rbox(in_l - 2 * fit, in_w - 2 * fit, lip_h, corner_r - wall);
        }
        translate([wall + fit + lip_w, wall + fit + lip_w, lid_t]) rbox(in_l - 2 * fit - 2 * lip_w, in_w - 2 * fit - 2 * lip_w, lip_h + 1, 1);
        // BOOT and LED holes, mirrored in Y because the lid prints flat side down and is turned over
        for (h = [[boot_xy, 6], [led_xy, 2.4]]) translate([B([h[0][0], h[0][1], 0])[0], out_w - B([h[0][0], h[0][1], 0])[1], -1]) cylinder(h = lid_t + 2, d = h[1], $fn = 32);
    }
}

// --- fit report (printed in the console) ---------------------------------------------------------------
eps = 0.01;   // rounding slack for the fit check
function inside(name, p, s) = let(c = B(p), ok = c[0] >= wall - eps && c[1] >= wall - eps && c[2] >= floor_t - eps && c[0] + s[0] <= wall + in_l + eps && c[1] + s[1] <= wall + in_w + eps && c[2] + s[2] <= floor_t + in_h + eps)
    str(name, ok ? ": fits" : ": DOES NOT FIT", " (", s[0], "x", s[1], "x", s[2], " at ", [round(c[0] * 10) / 10, round(c[1] * 10) / 10, round(c[2] * 10) / 10], ")");
echo(str("box outside ", out_l, " x ", out_w, " x ", base_h + lid_t, " mm; inside ", in_l, " x ", in_w, " x ", in_h, " mm"));
echo(inside("DevKit incl. antenna and USB-C lips", [0, -usbc_lip, -stub_h], [module_y[1] + usbc_lip, pcb_w, stub_h + pcb_t + usbc_h]));
echo(inside("charger", charger_p, [charger[1], charger[0], charger[2]]));
echo(inside("LiPo", lipo_p, [lipo[1], lipo[0], lipo[2]]));
echo(inside("boost", boost_p, [boost[1], boost[0], boost[2]]));
echo(inside("MOSFET", mosfet_p, [mosfet[0], mosfet[1], mosfet[2]]));
echo(inside("diode", diode_p, [diode[0], diode[1], diode[2]]));
echo(inside("resistor", res_p[0], [resistor[0], resistor[1], resistor[2]]));
echo(str("row under the board ends at y = ", boost_p[1] + boost[1], " of ", module_y[1], " available"));
echo(str("bay height ", bay_h, ": LiPo+stubs need ", lipo[2] + stub_h, ", charger+stubs ", charger[2] + stub_h));

// --- output ---------------------------------------------------------------------------------------------
if (part == "base") base();
if (part == "lid") lid();
if (part == "devkit") devkit();
if (part == "parts") parts();
if (part == "wires") wires();
if (part == "assembly") { color("#2f8ea3", 0.35) base(); devkit(); parts(); wires(); }
if (part == "exploded") { base(); devkit(); parts(); wires(); translate([0, 0, base_h + 25]) mirror([0, 0, 1]) translate([0, out_w, -lid_t]) mirror([0, 1, 0]) lid(); }
if (part == "both") { base(); translate([0, out_w + 10, 0]) lid(); }
