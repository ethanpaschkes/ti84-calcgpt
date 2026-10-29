// Calc box, 1:1. A pack for the ESP32-S3 DevKitC-1 (Ethan's is the Lonely Binary N16R8 copy with soldered headers), a 1S LiPo,
// a charger and a 5 V boost, to stick on the back of a TI-84 Plus CE. Two printed parts: base (tray) and lid (friction fit).
// All sizes in mm.
//
// The DevKitC-1 geometry comes from Espressif's drawing (DXF_ESP32-S3-DevKitC-1_V1.1_20220429): board 25.4 x 62.8, pin pads at
// x 1.27 / 24.13, y 7.96 + 2.54 k (22 a side), USB-C J2 (UART) at x 6.0 and J4 (USB) at x 19.4 on the y = 0 end, module 18 wide
// at x 3.75..21.75 from y 43.65 to 69.15 (the antenna sticks 6.3 past the board). Board frame: x across, y along (0 = USB end),
// z up from the PCB's underside. The headers and the module are on opposite faces; the board lies module UP in the box.
//
// wiring = "dupont"   what Ethan has today: full headers, female Dupont housings pushed on the pins, long jumper wires coiled
//                     in the end room. Thick (about 28 mm) but nothing to solder.
// wiring = "soldered" headers cut flush, short wires soldered to the pin stubs. Slim (about 14 mm).
// part = "base" | "lid" | "devkit" | "parts" | "wires" | "cable" | "assembly" | "exploded" | "both"
//   openscad --export-format binstl -o base.stl -D 'wiring="dupont"' -D 'part="base"' calcbox.scad
wiring = "dupont";
part = "exploded";
use_models = true;                  // true: draw the boost and charger from Adafruit's STL files in models/ (same sizes as the numbers above)

// --- the DevKitC-1 (Espressif) -----------------------------------------------------------------
pcb_w = 25.4; pcb_l = 62.8; pcb_t = 1.6;
pin_x = [1.27, 24.13]; pin_y0 = 7.96; pin_pitch = 2.54; pin_n = 22; pin_hole = 1.0;
header_h = 2.5;                     // the black plastic strip of the header, on the underside
pin_len = 6.0;                      // the pin beyond the plastic (what a Dupont housing slides onto)
stub_h = 1.0;                       // what is left after cutting the pins flush (MEASURE after cutting)
module_x = [3.75, 21.75]; module_y = [43.65, 69.15]; module_h = 3.1;
usbc_x = [6.0, 19.4]; usbc_w = 8.94; usbc_d = 7.3; usbc_h = 3.26; usbc_lip = 1.3;
boot_xy = [7.0, 12.7]; reset_xy = [17.4, 12.7]; button_w = 4.2; button_d = 3.2; button_h = 2.5;
led_xy = [7.6, 28.2]; led_w = 5.0; led_h = 1.6;

// --- Ethan's wires and cable (MEASURE) ------------------------------------------------------------
dupont_hsg = [2.54, 2.54, 14.0];    // one female Dupont housing (the plastic bit): footprint and length along the pin
dupont_wire_d = 1.2;                // the jumper wire with insulation
usb_wire_len = 300;                 // MEIRIYFA USB-A to 4 Dupont cable: wire length from housing to plug
ctrl_wire_len = 200;                // EDGELEC jumpers for sense / EN / gate
usb_plug = [14, 7, 32];             // the MEIRIYFA cable's USB-A plug with its overmould: width, thickness, length
otg_adapter = [16, 8, 36];          // rgzhihuifz mini-B male to USB-A female adapter: width, thickness, length incl. the mini plug
solder_wire_d = 1.0;                // 28 AWG silicone wire if soldered instead

// --- the other parts (MEASURE) -----------------------------------------------------------------
lipo = [17, 30, 4.0];               // 1S 150 mAh pouch from the RC plane: width across the board, length along it, thickness
charger = [19.05, 21.0, 6.37];      // Adafruit Micro-Lipo 1904, from Adafruit's CAD (models/adafruit_1904_microlipo.stl); the RC charger stick is bigger: measure it
boost = [17.78, 11.43, 5.07];       // Adafruit MiniBoost 4654, from Adafruit's CAD (models/adafruit_4654_miniboost.stl); long side across the board
mosfet = [4.6, 5.2, 3.7];           // 2N7000 TO-92 lying on its flat face
diode = [2.7, 4.1, 2.7];            // 1N5819 DO-41 body
resistor = [2.4, 6.3, 2.4];         // 1/4 W

// --- the box -------------------------------------------------------------------------------------
wall = 1.6; floor_t = 1.6; lid_t = 1.6; lip_h = 2.5; lip_w = 1.2; fit = 0.25; corner_r = 3;
dupont = wiring == "dupont";
wire_d = dupont ? dupont_wire_d : solder_wire_d;
under_pins = dupont ? header_h + dupont_hsg[2] + 3.0 : stub_h;          // pins side: housing plus a 3 mm wire bend, or just the stubs
bay_h = max(under_pins, max(lipo[2], charger[2], boost[2]) + (dupont ? header_h : stub_h) + 0.5);
top_h = max(usbc_h, module_h, button_h) + 0.6;
// coiled slack: each wire's volume (with 1.8x packing for loose coils) spread over the end room's cross-section
slack_vol = dupont ? 1.8 * PI * pow(wire_d / 2, 2) * (4 * usb_wire_len + 3 * ctrl_wire_len) : 0;
end_room = dupont ? ceil(slack_vol / ((pcb_w + 2) * bay_h)) + 4 : 8;
side_air = 1.0; far_air = 0.8;

// --- derived -------------------------------------------------------------------------------------
in_l = end_room + usbc_lip + module_y[1] + far_air;
in_w = pcb_w + 2 * side_air;
in_h = bay_h + pcb_t + top_h;
out_l = in_l + 2 * wall; out_w = in_w + 2 * wall; base_h = floor_t + in_h;
bx0 = wall + end_room + usbc_lip; by0 = wall + side_air; bz0 = floor_t + bay_h;
function B(p) = [bx0 + p[1], by0 + p[0], bz0 + p[2]];

// --- helpers -------------------------------------------------------------------------------------
module rbox(l, w, h, r) { hull() for (x = [r, l - r], y = [r, w - r]) translate([x, y, 0]) cylinder(h = h, r = r, $fn = 32); }
module at(p) { translate(B(p)) children(); }
module blk(p, s, c) { color(c) at(p) cube([s[1], s[0], s[2]]); }    // p = board corner, s = [across, along, high]
module wire(pts, c = "red") { color(c) for (i = [0 : len(pts) - 2]) hull() { at(pts[i]) sphere(d = wire_d, $fn = 12); at(pts[i + 1]) sphere(d = wire_d, $fn = 12); } }
function pin(side, k) = [pin_x[side], pin_y0 + pin_pitch * k];
// the pins we use: [side, k, name, colour]  (left: G=0, 5V=1, 14=2, 13=3, 12=4; right: G=0, 19=2, 20=3)
used = [[0, 1, "5V", "#d0342c"], [0, 0, "GND", "#1d1c19"], [1, 2, "D- GPIO19", "#f2f2ec"], [1, 3, "D+ GPIO20", "#2a9d3f"],
        [0, 2, "sense GPIO14", "#7a3fb8"], [0, 3, "boost EN GPIO13", "#7a3fb8"], [0, 4, "ID gate GPIO12", "#7a3fb8"]];

// --- the DevKitC-1 ---------------------------------------------------------------------------------
module devkit() {
    color("#17202a") difference() {
        at([0, 0, 0]) cube([pcb_l, pcb_w, pcb_t]);
        for (s = [0, 1], k = [0 : pin_n - 1]) at([pin(s, k)[0], pin(s, k)[1], -1]) cylinder(h = pcb_t + 2, d = pin_hole, $fn = 12);
    }
    if (dupont) {
        for (s = [0, 1]) blk([pin_x[s] - 1.27, pin_y0 - 1.27, -header_h], [2.54, pin_pitch * pin_n, header_h], "#111");
        color("silver") for (s = [0, 1], k = [0 : pin_n - 1]) at([pin(s, k)[0], pin(s, k)[1], -header_h - pin_len]) cylinder(h = pin_len + header_h + pcb_t + 1, d = 0.64, $fn = 8);
    } else {
        color("silver") for (s = [0, 1], k = [0 : pin_n - 1]) at([pin(s, k)[0], pin(s, k)[1], -stub_h]) cylinder(h = stub_h + pcb_t, d = 0.64, $fn = 8);
    }
    blk([module_x[0], module_y[0], pcb_t], [module_x[1] - module_x[0], module_y[1] - module_y[0], module_h], "#5d6670");
    for (x = usbc_x) blk([x - usbc_w / 2, -usbc_lip, pcb_t], [usbc_w, usbc_d + usbc_lip, usbc_h], "#8d9198");
    for (b = [boot_xy, reset_xy]) blk([b[0] - button_w / 2, b[1] - button_d / 2, pcb_t], [button_w, button_d, button_h], "#2c2c2a");
    blk([led_xy[0] - led_w / 2, led_xy[1] - led_w / 2, pcb_t], [led_w, led_w, led_h], "#e8e8e8");
}

// --- the other parts: in a row under the board between the header rows (charger, LiPo, boost); small parts in the end room -----
part_z = dupont ? -header_h : -stub_h;      // their tops touch the header plastic / the stubs
charger_p = [(pcb_w - charger[0]) / 2, 1.0, part_z - charger[2]];
lipo_p = [(pcb_w - lipo[0]) / 2, charger_p[1] + charger[1] + 1.0, part_z - lipo[2]];
boost_p = [(pcb_w - boost[0]) / 2, lipo_p[1] + lipo[1] + 1.0, part_z - boost[2]];
er_y = -usbc_lip - end_room;
mosfet_p = [1.0, er_y + 1.0, -bay_h];
diode_p = [pcb_w - 1.0 - diode[0], er_y + 1.0, -bay_h];
res_p = [[6.5, er_y + 0.8, -bay_h], [pcb_w - 6.5 - resistor[0], er_y + 0.8, -bay_h]];
slack_l = end_room - 4;                      // the coil of spare wire fills the end room above the small parts
slack_p = [1.0, er_y + 2.0, -bay_h + 4.5];

module parts() {
    if (use_models) {
        color("#1f4e6b") at(charger_p) translate([0.66, 0, 0]) import("models/adafruit_1904_microlipo.stl");   // its bbox starts at x = -0.66
        color("#1f6e8b") at(boost_p) import("models/adafruit_4654_miniboost.stl");
    } else {
        blk(charger_p, charger, "#1f4e6b");
        blk(boost_p, boost, "#1f6e8b");
    }
    blk(lipo_p, lipo, "#b9bcc4");
    blk(mosfet_p, mosfet, "#2c2c2a");
    blk(diode_p, diode, "#3a3a3a");
    for (r = res_p) blk(r, resistor, "#e8d5b5");
    if (dupont) color("#555", 0.5) at(slack_p) cube([slack_l, pcb_w - 2, bay_h - 5.5]);   // coiled slack, as a block of the same volume
}

// --- wires -------------------------------------------------------------------------------------------
zw = dupont ? -header_h - dupont_hsg[2] - 1.5 : -stub_h - wire_d / 2;   // where the wires run: under the housings, or under the stubs
splice = [pcb_w / 2, er_y + end_room / 2, zw];
function p3(p, z) = [p[0], p[1], z];
module wires() {
    for (u = used) {
        p = pin(u[0], u[1]);
        if (dupont) blk([p[0] - 1.27, p[1] - 1.27, -header_h - dupont_hsg[2]], [2.54, 2.54, dupont_hsg[2]], "#111");   // the housing on the pin
        // down the pin (or from the stub), then along the board's edge to the end room, into the coil
        wire([p3(p, dupont ? -header_h - dupont_hsg[2] : -stub_h), p3(p, zw), [p[0], -usbc_lip - 1, zw], [splice[0] + (u[0] ? 2 : -2) * (u[1] + 1) * 0.4, splice[1], zw]], u[3]);
    }
    // power chain under the board: LiPo -> charger, charger -> boost, boost -> diode -> VBUS, cable 5 V -> charger IN
    zp = part_z - 0.6;
    wire([[lipo_p[0] + 2, lipo_p[1], zp], [charger_p[0] + 2, charger_p[1] + charger[1], zp]], "#e07b00");
    wire([[lipo_p[0] + 4, lipo_p[1], zp], [charger_p[0] + 4, charger_p[1] + charger[1], zp]], "#1d1c19");
    wire([[charger_p[0] + charger[0] - 2, charger_p[1] + charger[1], zp], [pcb_w - 3.5, charger_p[1] + charger[1] + 2, zp], [pcb_w - 3.5, boost_p[1], zp], [boost_p[0] + boost[0] - 1, boost_p[1], zp]], "#e07b00");
    wire([[boost_p[0] + boost[0] - 3, boost_p[1], zp], [pcb_w - 3.5, boost_p[1] - 2, zp], [pcb_w - 3.5, -1, zp], [diode_p[0] + 1, diode_p[1] + diode[1], -bay_h + 1.5]], "#d0342c");
    wire([[diode_p[0] + 1, diode_p[1], -bay_h + 1.5], [splice[0] - 3, splice[1], zw]], "#d0342c");
    wire([[charger_p[0] + 1, charger_p[1], zp], [charger_p[0] + 1, -1, zp], [splice[0] - 3, splice[1], zw]], "#d0342c");
    // five wires leaving to the cable (5 V, GND, D-, D+, ID)
    for (i = [-2 : 2]) wire([[splice[0] + i * 1.5, splice[1], zw], [splice[0] + i * 1.5, er_y - wall - 4, zw]], i == 0 ? "#1f6fd6" : "#777");
}

// --- the cable outside the box, at real length: USB-A plug, then the OTG adapter into the calculator ---------------------
module cable_outside() {
    color("#222") at([pcb_w / 2 - usb_plug[0] / 2, er_y - wall - 4 - usb_plug[2], zw - usb_plug[1] / 2]) cube([usb_plug[2], usb_plug[0], usb_plug[1]]);
    color("#333") at([pcb_w / 2 - otg_adapter[0] / 2, er_y - wall - 4 - usb_plug[2] - otg_adapter[2], zw - otg_adapter[1] / 2]) cube([otg_adapter[2], otg_adapter[0], otg_adapter[1]]);
    color("#999") at([pcb_w / 2, er_y - wall - 4 - usb_plug[2] - otg_adapter[2] - 8, zw]) rotate([0, 90, 0]) cylinder(h = 8, d = 3, $fn = 12);   // the mini-B plug
}

// --- the box -----------------------------------------------------------------------------------------
module base() {
    difference() {
        rbox(out_l, out_w, base_h, corner_r);
        translate([wall, wall, floor_t]) rbox(in_l, in_w, in_h + 1, corner_r - wall);
        // UART USB-C: a channel at the connector's height from the end wall through the end room
        translate([-1, B([usbc_x[0] - usbc_w / 2 - 0.5, 0, 0])[1], bz0 + pcb_t - 0.4]) cube([wall + end_room + usbc_lip + 2, usbc_w + 1, usbc_h + 1.0]);
        // cable slot in the end wall at the wires' height
        translate([-1, B([splice[0], 0, 0])[1] - 5.5, B([0, 0, zw])[2] - 3]) cube([wall + 2, 11, 6]);
    }
    // ledges the board's underside rests on, outside the header rows (the Dupont housings hang between the ledge and the wall)
    for (y = [B([0, 0, 0])[1] - side_air, B([pcb_w, 0, 0])[1] + side_air - 1.0])
        translate([B([0, 1, 0])[0], y, floor_t]) cube([module_y[1] - 1, 1.0, bay_h]);
    translate([B([0, module_y[1], 0])[0] + 0.3, wall, floor_t]) cube([0.5, in_w, bay_h + pcb_t]);   // stop at the antenna end
}

module lid() {
    difference() {
        union() {
            rbox(out_l, out_w, lid_t, corner_r);
            translate([wall + fit, wall + fit, lid_t]) rbox(in_l - 2 * fit, in_w - 2 * fit, lip_h, corner_r - wall);
        }
        translate([wall + fit + lip_w, wall + fit + lip_w, lid_t]) rbox(in_l - 2 * fit - 2 * lip_w, in_w - 2 * fit - 2 * lip_w, lip_h + 1, 1);
        for (h = [[boot_xy, 6], [led_xy, 2.4]]) translate([B([h[0][0], h[0][1], 0])[0], out_w - B([h[0][0], h[0][1], 0])[1], -1]) cylinder(h = lid_t + 2, d = h[1], $fn = 32);
    }
}

// --- fit report ---------------------------------------------------------------------------------------
eps = 0.01;
function inside(name, p, s) = let(c = B(p), ok = c[0] >= wall - eps && c[1] >= wall - eps && c[2] >= floor_t - eps && c[0] + s[1] <= wall + in_l + eps && c[1] + s[0] <= wall + in_w + eps && c[2] + s[2] <= floor_t + in_h + eps)
    str(name, ok ? ": fits" : ": DOES NOT FIT", " (", s[0], " x ", s[1], " x ", s[2], ")");
echo(str("wiring = ", wiring, ": box outside ", out_l, " x ", out_w, " x ", base_h + lid_t, " mm; inside ", in_l, " x ", in_w, " x ", in_h, " mm; end room ", end_room, " mm"));
echo(inside("DevKit incl. antenna, USB-C lips and header/stubs", [0, -usbc_lip, -(dupont ? header_h + pin_len : stub_h)], [pcb_w, module_y[1] + usbc_lip, (dupont ? header_h + pin_len : stub_h) + pcb_t + usbc_h]));
if (dupont) echo(inside("Dupont housing on the 5V pin", [pin(0, 1)[0] - 1.27, pin(0, 1)[1] - 1.27, -header_h - dupont_hsg[2]], dupont_hsg));
echo(inside("charger", charger_p, charger)); echo(inside("LiPo", lipo_p, lipo)); echo(inside("boost", boost_p, boost));
echo(inside("MOSFET", mosfet_p, mosfet)); echo(inside("diode", diode_p, diode)); echo(inside("resistor", res_p[0], resistor));
if (dupont) echo(str("coiled slack: ", round(slack_vol), " mm3 of wire (", 4 * usb_wire_len + 3 * ctrl_wire_len, " mm), end room ", end_room, " mm"));
echo(str("row under the board ends at y = ", boost_p[1] + boost[1], " of ", module_y[1], " available; cable chain outside the box: ", usb_plug[2] + otg_adapter[2] + 12, " mm"));

// --- output ---------------------------------------------------------------------------------------------
if (part == "base") base();
if (part == "lid") lid();
if (part == "devkit") devkit();
if (part == "parts") parts();
if (part == "wires") wires();
if (part == "cable") cable_outside();
if (part == "assembly") { color("#2f8ea3", 0.35) base(); devkit(); parts(); wires(); cable_outside(); }
if (part == "exploded") { base(); devkit(); parts(); wires(); cable_outside(); translate([0, 0, base_h + 25]) mirror([0, 0, 1]) translate([0, out_w, -lid_t]) mirror([0, 1, 0]) lid(); }
if (part == "both") { base(); translate([0, out_w + 10, 0]) lid(); }
