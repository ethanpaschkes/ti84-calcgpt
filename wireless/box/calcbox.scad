// Calc box, 1:1. A pack for the ESP32-S3 DevKitC-1 (Ethan's is the Lonely Binary N16R8 copy with soldered headers), a 1S LiPo,
// a charger and a 5 V boost, to stick on the back of a TI-84 Plus CE. Two printed parts: base (tray) and lid (friction fit).
// All sizes in mm.
//
// The DevKitC-1 geometry comes from Espressif's drawing (DXF_ESP32-S3-DevKitC-1_V1.1_20220429): board 25.4 x 62.8, pin pads at
// x 1.27 / 24.13, y 7.96 + 2.54 k (22 a side), USB-C J2 (UART) at x 6.0 and J4 (USB) at x 19.4 on the y = 0 end, module 18 wide
// at x 3.75..21.75 from y 43.65 to 69.15 (the antenna sticks 6.3 past the board). Board frame: x across, y along (0 = USB end),
// z up from the PCB's underside. The headers and the module are on opposite faces; the board lies module UP in the box.
//
// layout = "stacked"  the LiPo, charger and boost in a row UNDER the board between the header rows (box about 31 wide)
// layout = "flat"     those parts in a bay BESIDE the board, one layer: as thin as the tallest part (box about 52 wide)
// layout = "module"   no DevKit: a bare ESP32-S3-WROOM-1 module on a small carrier board beside the parts (the floor of "thin")
// wiring = "dupont"   what Ethan has today: full headers, female Dupont housings pushed on the pins, long jumper wires coiled
//                     in the end room. Thick (about 28 mm) but nothing to solder.
// wiring = "soldered" headers cut flush, short wires soldered to the pin stubs. Slim (about 10-17 mm).
// cable_exit = "end"  the 5-wire lead to the calculator leaves through the end wall at the USB end
// cable_exit = "side" the lead runs inside the box (on the floor under the board, or beside the parts row) to a slot in a SIDE wall
//                     near the far (antenna) end. cable_wall picks the wall: 0 = the y = 0 wall (the UART socket's side),
//                     1 = the opposite wall (the side bay in "flat"). On the calculator it is the wall facing the right edge.
// The UART USB-C (J2) sits in an open-topped pocket in the end wall so a normal USB-C cable can be plugged in to flash; the
// other USB-C (J4) stays covered. The lid's lip is notched wherever a part reaches above its bottom edge.
// part = "base" | "lid" | "devkit" | "parts" | "wires" | "cable" | "assembly" | "exploded" | "both"
//   openscad --export-format binstl -o base.stl -D 'wiring="dupont"' -D 'part="base"' calcbox.scad
wiring = "dupont";
layout = "stacked";
cable_exit = "end";
cable_wall = 0;
part = "exploded";
use_models = true;                  // true: draw the boost and charger from Adafruit's STL files in models/ (same sizes as the numbers below)

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
wroom = [18.0, 25.5, 3.1]; carrier = [20.0, 34.0, 1.0];   // bare module (KiCad STEP in models/) on a carrier board: across, along, high

// --- Ethan's wires and cable (MEASURE) ------------------------------------------------------------
dupont_hsg = [2.54, 2.54, 14.0];    // one female Dupont housing (the plastic bit): footprint and length along the pin
dupont_wire_d = 1.2;                // the jumper wire with insulation
usb_wire_len = 300;                 // MEIRIYFA USB-A to 4 Dupont cable: wire length from housing to plug
ctrl_wire_len = 200;                // EDGELEC jumpers for sense / EN / gate
usb_plug = [14, 7, 32];             // the MEIRIYFA cable's USB-A plug with its overmould: width, thickness, length
otg_adapter = [16, 8, 36];          // rgzhihuifz mini-B male to USB-A female adapter: width, thickness, length incl. the mini plug
solder_wire_d = 1.0;                // 28 AWG silicone wire if soldered instead
usbc_plug = [12.4, 6.5];            // the flashing cable's USB-C plug body: width, thickness (12.35 x 6.5 is the USB-C spec's largest)

// --- the other parts (MEASURE) -----------------------------------------------------------------
lipo = [17, 30, 4.0];               // 1S 150 mAh pouch from the RC plane: width across the board, length along it, thickness
charger = [19.05, 21.0, 6.37];      // Adafruit Micro-Lipo 1904, from Adafruit's CAD (models/adafruit_1904_microlipo.stl); the RC charger stick is bigger: measure it
boost = [17.78, 11.43, 5.07];       // Adafruit MiniBoost 4654, from Adafruit's CAD (models/adafruit_4654_miniboost.stl); long side across the board
mosfet = [4.6, 5.2, 3.7];           // 2N7000 TO-92 lying on its flat face
diode = [2.7, 4.1, 2.7];            // 1N5819 DO-41 body
resistor = [2.4, 6.3, 2.4];         // 1/4 W

// --- the box -------------------------------------------------------------------------------------
wall = 1.6; floor_t = 1.6; lid_t = 1.6; lip_h = 2.5; lip_w = 1.2; fit = 0.25; corner_r = 3;
dupont = wiring == "dupont" && layout != "module";   // the bare module has no header pins to push housings on
flat = layout != "stacked";                 // parts beside the board (or beside the bare module)
bare = layout == "module";
side = cable_exit == "side";
wire_d = dupont ? dupont_wire_d : solder_wire_d;
under_pins = dupont ? header_h + dupont_hsg[2] + 3.0 : max(stub_h, wire_d) + 0.2;   // pins side: housing plus a 3 mm wire bend, or the stubs with the wires soldered beside them
parts_h = max(lipo[2], charger[2], boost[2]);
bay_h = bare ? 0 : flat ? under_pins : max(under_pins, parts_h + (dupont ? header_h : stub_h) + 0.5);
top_h = bare ? wroom[2] + 0.6 : max(usbc_h, module_h, button_h) + 0.6;
bay_w = flat ? max(lipo[0], charger[0], boost[0]) + 1.0 : 0;      // the side bay's width, across
board_w = bare ? carrier[0] : pcb_w; board_l = bare ? carrier[1] : module_y[1]; board_t = bare ? carrier[2] : pcb_t;
tab_y = bare ? carrier[1] : pcb_l;          // where the board's end face is (the module's antenna hangs on past it, above the board)
lip_room = lip_w + fit + 0.3;               // keep the bay's row this far from the end walls: the lid's lip hangs down inside them
// coiled slack: each wire's volume (with 1.8x packing for loose coils) spread over the end room's cross-section
slack_vol = dupont ? 1.8 * PI * pow(wire_d / 2, 2) * (4 * usb_wire_len + 3 * ctrl_wire_len) : 0;
end_room = dupont ? ceil(slack_vol / ((pcb_w + 2) * bay_h)) + 4 : 8;
side_air = 1.0; far_air = 0.8;

// --- the lead (5 wires: 5V, GND, D-, D+, ID) inside the box ----------------------------------------
lead_n = 5; lead_p = wire_d + 0.05;
lead_cols = bare ? 1 : !flat && !dupont ? 2 : 5;            // wires side by side (the rest stack on top)
lead_ext = [(lead_cols - 1) * lead_p + wire_d, (ceil(lead_n / lead_cols) - 1) * lead_p + wire_d];   // across (along after the turn), high
// side exit: where the lead runs along the box (board frame x) and its bottom (z):
//   flat / Dupont: on the floor under the board, below the USB socket J4;  soldered stacked: beside the parts row, inside the right
//   pin row, above the pin wires;  bare module: in the gap between the carrier and the bay
run_x = bare ? board_w + (side_air + 0.5) / 2 : flat || dupont ? pcb_w - 6 : pcb_w - 2.1;
function lead_off(i) = [((i % lead_cols) - (lead_cols - 1) / 2) * lead_p, wire_d / 2 + floor(i / lead_cols) * lead_p];

// --- derived -------------------------------------------------------------------------------------
row_l = charger[1] + lipo[1] + boost[1] + 2.0;          // flat: the charger, LiPo, boost row along the bay
in_l = max(end_room + usbc_lip + board_l + far_air, flat ? 2 * lip_room + row_l + (side ? lead_ext[0] + 1.0 : 0) : 0);
in_w = board_w + 2 * side_air + bay_w;
in_h = max(bay_h + board_t + top_h, flat ? parts_h + 0.5 : 0);
out_l = in_l + 2 * wall; out_w = in_w + 2 * wall; base_h = floor_t + in_h;
bx0 = wall + end_room + usbc_lip; by0 = wall + side_air; bz0 = floor_t + bay_h;
function B(p) = [bx0 + p[1], by0 + p[0], bz0 + p[2]];
er_y = -usbc_lip - end_room;                 // the end wall's inner face (board frame y)
y_far = in_l - end_room - usbc_lip;          // the far wall's inner face
turn_y = y_far - lead_ext[0] / 2 - 0.5;      // side exit: where the lead turns toward the side wall
wall_out_x = cable_wall == 0 ? -side_air - wall - 3 : in_w - side_air + wall + 3;   // just outside the chosen side wall
run_z0 = -bay_h + (!flat && !dupont && !bare ? wire_d + 0.1 : 0);

// UART plug pocket: the plug body enters the box up to the socket; open to the top, closed off from the end room by a wall
pk_w = usbc_plug[0] + 1.0; pk_h = usbc_plug[1] + 1.0;
pk_x = [usbc_x[0] - pk_w / 2, usbc_x[0] + pk_w / 2];                 // board frame, across
pk_z = pcb_t + usbc_h / 2 - pk_h / 2;                                 // the pocket's bottom, board frame
pk_shelf = bz0 + pk_z - wall > floor_t;                               // the pocket needs its own floor (tall boxes)

// --- helpers -------------------------------------------------------------------------------------
module rbox(l, w, h, r) { hull() for (x = [r, l - r], y = [r, w - r]) translate([x, y, 0]) cylinder(h = h, r = r, $fn = 32); }
module at(p) { translate(B(p)) children(); }
module blk(p, s, c) { color(c) at(p) cube([s[1], s[0], s[2]]); }    // p = board corner, s = [across, along, high]
module wire(pts, c = "red") { color(c) for (i = [0 : len(pts) - 2]) hull() { at(pts[i]) sphere(d = wire_d, $fn = 12); at(pts[i + 1]) sphere(d = wire_d, $fn = 12); } }
function pin(side_, k) = [pin_x[side_], pin_y0 + pin_pitch * k];
// the pins we use: [side, k, name, colour]  (left: G=0, 5V=1, 14=2, 13=3, 12=4; right: G=0, 19=2, 20=3)
used = [[0, 1, "5V", "#d0342c"], [0, 0, "GND", "#1d1c19"], [1, 2, "D- GPIO19", "#f2f2ec"], [1, 3, "D+ GPIO20", "#2a9d3f"],
        [0, 2, "sense GPIO14", "#7a3fb8"], [0, 3, "boost EN GPIO13", "#7a3fb8"], [0, 4, "ID gate GPIO12", "#7a3fb8"]];

// --- the DevKitC-1 ---------------------------------------------------------------------------------
module devkit() {
    if (bare) {
        blk([0, 0, 0], [carrier[0], carrier[1], carrier[2]], "#17202a");
        blk([(carrier[0] - wroom[0]) / 2, carrier[1] - wroom[1] - 1, carrier[2]], wroom, "#5d6670");
        blk([3, 2, carrier[2]], [button_w, button_d, button_h], "#2c2c2a");            // BOOT button on the carrier
    } else {
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
}

// --- the other parts: the charger, LiPo, boost row (under the board, or in the side bay); small parts in the end room ---------
part_z = dupont ? -header_h : -stub_h;      // stacked: their tops touch the header plastic / the stubs
bay_x = board_w + side_air + 0.5;             // flat: the side bay starts here (board frame, across)
bay_y0 = er_y + lip_room;                     // flat: the row starts just clear of the end wall's lid lip
charger_p = flat ? [bay_x, bay_y0, -bay_h] : [(pcb_w - charger[0]) / 2, 1.0, part_z - charger[2]];
lipo_p = flat ? [bay_x, charger_p[1] + charger[1] + 1.0, -bay_h] : [(pcb_w - lipo[0]) / 2, charger_p[1] + charger[1] + 1.0, part_z - lipo[2]];
boost_p = flat ? [bay_x, lipo_p[1] + lipo[1] + 1.0, -bay_h] : [(pcb_w - boost[0]) / 2, lipo_p[1] + lipo[1] + 1.0, part_z - boost[2]];
// the end room: across from the UART pocket's wall to the bay (flat) or the far side wall (stacked)
er_x0 = bare ? 0.5 : pk_x[1] + wall + 0.5;
er_x1 = (flat ? bay_x : in_w - side_air) - 0.5;
mosfet_p = [er_x0, er_y + lip_room, -bay_h];
res_p = [[er_x0 + mosfet[0] + 0.5, er_y + 0.8, -bay_h], [er_x0 + mosfet[0] + resistor[0] + 1.0, er_y + 0.8, -bay_h]];
diode_x = res_p[1][0] + resistor[0] + 0.5;
diode_p = diode_x + diode[0] <= er_x1 ? [diode_x, er_y + lip_room, -bay_h] : [er_x0, er_y + lip_room, -bay_h + mosfet[2] + 0.3];   // no room in the row: on the MOSFET
slack_l = end_room - 4;                      // Dupont: the coil of spare wire fills the end room above the small parts, below the pocket
slack_z = -bay_h + mosfet[2] + diode[2] + 0.6;
slack_p = [1.0, er_y + 2.0, slack_z];
slack_h = pk_z - wall - 0.2 - slack_z;

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
    if (dupont) color("#555", 0.5) at(slack_p) cube([slack_l, pcb_w - 2, slack_h]);   // coiled slack, as a block
}

// --- wires -------------------------------------------------------------------------------------------
zw = dupont ? -header_h - dupont_hsg[2] - 1.5 : -bay_h + wire_d / 2;   // where the pin wires run: under the housings, or on the floor beside the stubs
cross_y = -usbc_lip / 2;                     // the pin wires cross over to the splice here: under the USB sockets' lips, clear of the pocket
end_x = (res_p[0][0] + res_p[1][0] + resistor[0]) / 2;   // end exit: the lead leaves over the two resistors
end_z = -bay_h + resistor[2] + 0.3;
splice = [side ? run_x : end_x, cross_y, zw];
function p3(p, z) = [p[0], p[1], z];
// the lead's path, board frame, wire i
function lead_path(i) = let(o = lead_off(i), e = (i - (lead_n - 1) / 2) * lead_p, dy = cable_wall == 0 ? o[0] : -o[0]) side
    ? [splice, [run_x + o[0], cross_y, run_z0 + o[1]], [run_x + o[0], turn_y + dy, run_z0 + o[1]], [wall_out_x, turn_y + dy, run_z0 + o[1]]]
    : [splice, [end_x + e, cross_y, end_z + wire_d / 2], [end_x + e, er_y - wall - 3, end_z + wire_d / 2]];
module wires() {
    if (!bare) for (u = used) {
        p = pin(u[0], u[1]);
        if (dupont) blk([p[0] - 1.27, p[1] - 1.27, -header_h - dupont_hsg[2]], [2.54, 2.54, dupont_hsg[2]], "#111");   // the housing on the pin
        // down the pin (or from the stub), along the pin row to the USB end, across to the splice
        wire([p3(p, dupont ? -header_h - dupont_hsg[2] : -stub_h), p3(p, zw), [p[0], cross_y, zw], [splice[0] + (u[0] ? 1 : -1) * (u[1] + 1) * 0.3, cross_y, zw]], u[3]);
    }
    // power chain: LiPo -> charger, charger -> boost, boost -> diode -> VBUS, cable 5 V -> charger IN
    zp = max(part_z - 0.6, -bay_h + wire_d / 2);
    wire([[lipo_p[0] + 2, lipo_p[1], zp], [charger_p[0] + 2, charger_p[1] + charger[1], zp]], "#e07b00");
    wire([[lipo_p[0] + 4, lipo_p[1], zp], [charger_p[0] + 4, charger_p[1] + charger[1], zp]], "#1d1c19");
    wire([[charger_p[0] + charger[0] - 2, charger_p[1] + charger[1], zp], [boost_p[0] + boost[0] - 1, boost_p[1], zp]], "#e07b00");
    wire([[boost_p[0] + boost[0] - 3, boost_p[1], zp], [diode_p[0] + 1, diode_p[1] + diode[1], diode_p[2] + 1.5]], "#d0342c");
    wire([[diode_p[0] + 1, diode_p[1], diode_p[2] + 1.5], splice], "#d0342c");
    wire([[charger_p[0] + 1, charger_p[1], zp], splice], "#d0342c");
    for (i = [0 : lead_n - 1]) wire(lead_path(i), i == 0 ? "#1f6fd6" : "#777");
}

// --- the cable outside the box, drawn straight out of its slot: the plug chain (Dupont) or a short lead and a mini-B plug ---------
module cable_outside() {
    p0 = lead_path(0)[len(lead_path(0)) - 1];
    d = side ? (cable_wall == 0 ? -1 : 1) : 0;                   // side exit: out along -x / +x; end exit: out along -y
    // a straight piece of the chain: off = its distance from the slot, s = [width, thickness, length]
    module piece(off, s, c) {
        if (d == 0) blk([p0[0] - s[0] / 2, p0[1] - off - s[2], p0[2] - s[1] / 2], [s[0], s[2], s[1]], c);
        else blk([d < 0 ? p0[0] - off - s[2] : p0[0] + off, p0[1] - s[0] / 2, p0[2] - s[1] / 2], [s[2], s[0], s[1]], c);
    }
    if (dupont) { piece(0, usb_plug, "#222"); piece(usb_plug[2], otg_adapter, "#333"); }
    else { piece(0, [3, 3, 20], "#333"); piece(20, [11, 8, 14], "#222"); }      // 20 mm of lead to a mini-B plug body
}

// --- the box -----------------------------------------------------------------------------------------
right_tab = !(side && run_x + lead_ext[0] / 2 > board_w - 0.8);   // the stop tab under the board's right corner, unless the lead runs there
tabs = [[-side_air, 0.6], if (right_tab) [board_w - 0.6, board_w + side_air]];
tab_z = max(-bay_h, -2.0) - 0.01;            // the tabs reach 2 mm below the board (down to the floor in thin boxes)
module base() {
    difference() {
        rbox(out_l, out_w, base_h, corner_r);
        translate([wall, wall, floor_t]) rbox(in_l, in_w, in_h + 1, corner_r - wall);
        // the lead's slot: in the chosen side wall near the far end, or in the end wall over the resistors
        if (side) translate([bx0 + turn_y - lead_ext[0] / 2 - 1, cable_wall == 0 ? -1 : wall + in_w - 1, max(floor_t, bz0 + run_z0 - 1)]) cube([lead_ext[0] + 2, wall + 2, lead_ext[1] + 2]);
        else translate([-1, by0 + end_x - (lead_n - 1) / 2 * lead_p - wire_d / 2 - 1, bz0 + end_z - 1]) cube([wall + 2, (lead_n - 1) * lead_p + wire_d + 2, wire_d + 2]);
        if (!bare) translate([-1, by0 + pk_x[0], bz0 + pk_z]) cube([bx0 - usbc_lip + 1, pk_w, base_h]);   // the UART plug pocket
    }
    if (!bare) {   // the pocket's wall toward the end room, and its own floor when the box is tall
        z0 = pk_shelf ? bz0 + pk_z - wall : floor_t - 0.01;
        translate([wall - 0.01, by0 + pk_x[1], z0]) cube([bx0 - usbc_lip - wall - 0.2, wall, base_h - z0]);
        if (pk_shelf) translate([wall - 0.01, wall - 0.01, z0]) cube([bx0 - usbc_lip - wall - 0.2, by0 + pk_x[1] - wall + 0.02, wall]);
    }
    // ledges the board's underside rests on, outside the header rows (the Dupont housings hang between the ledge and the wall)
    if (bay_h > 0) for (y = [B([0, 0, 0])[1] - side_air, B([board_w, 0, 0])[1] + side_air - 1.0])
        translate([B([0, 1, 0])[0], y, floor_t - 0.01]) cube([tab_y - 2, 1.0, bay_h + 0.01]);
    for (t = tabs) translate([B([0, tab_y + 0.2, 0])[0], by0 + t[0], B([0, 0, tab_z])[2]]) cube([1.0, t[1] - t[0], board_t - 0.1 - tab_z]);   // stops at the board's end, off the side walls
}
// the box without its floor: the walls the "cover" version grows on the outside of the calculator's slide cover
module base_walls() { difference() { base(); translate([-1, -1, -1]) cube([out_l + 2, out_w + 2, floor_t + 1]); } }

// everything that reaches above the lid lip's bottom edge gets a notch in the lip: [board corner, size, name]
tall = [if (!bare) [[module_x[0], module_y[0], pcb_t], [module_x[1] - module_x[0], module_y[1] - module_y[0], module_h], "module + antenna"],
        if (bare) [[(carrier[0] - wroom[0]) / 2, carrier[1] - wroom[1] - 1, carrier[2]], wroom, "WROOM module"],
        [charger_p, charger, "charger"], [lipo_p, lipo, "LiPo"], [boost_p, boost, "boost"],
        [mosfet_p, mosfet, "MOSFET"], [diode_p, diode, "diode"], [res_p[0], resistor, "resistor"], [res_p[1], resistor, "resistor"]];
lip_bottom = floor_t + in_h - lip_h;
lip_zone = wall + fit + lip_w;
function lip_hit(t) = let(c = B(t[0]), s = t[1]) c[2] + s[2] > lip_bottom - 0.2 &&
    (c[0] < lip_zone || c[0] + s[1] > out_l - lip_zone || c[1] < lip_zone || c[1] + s[0] > out_w - lip_zone);
lip_notched = [for (t = tall) if (lip_hit(t)) t];

module lid() {
    difference() {
        union() {
            rbox(out_l, out_w, lid_t, corner_r);
            translate([wall + fit, wall + fit, lid_t]) rbox(in_l - 2 * fit, in_w - 2 * fit, lip_h, corner_r - wall);
        }
        translate([wall + fit + lip_w, wall + fit + lip_w, lid_t]) rbox(in_l - 2 * fit - 2 * lip_w, in_w - 2 * fit - 2 * lip_w, lip_h + 1, 1);
        for (h = [[boot_xy, 6], [led_xy, 2.4]]) translate([B([h[0][0], h[0][1], 0])[0], out_w - B([h[0][0], h[0][1], 0])[1], -1]) cylinder(h = lid_t + 2, d = h[1], $fn = 32);
        // the lid is flipped onto the base, so the box's y maps to out_w - y here
        if (!bare) translate([-1, out_w - by0 - pk_x[1], -1]) cube([bx0 - usbc_lip + 1, pk_w, lid_t + lip_h + 2]);   // over the UART pocket
        for (t = lip_notched) let(c = B(t[0]), s = t[1]) translate([c[0] - 0.3, out_w - c[1] - s[0] - 0.3, lid_t - 0.01]) cube([s[1] + 0.6, s[0] + 0.6, lip_h + 1]);
    }
}

// --- fit report ---------------------------------------------------------------------------------------
eps = 0.01;
function fits(p, s) = let(c = B(p)) c[0] >= wall - eps && c[1] >= wall - eps && c[2] >= floor_t - eps && c[0] + s[1] <= wall + in_l + eps && c[1] + s[0] <= wall + in_w + eps && c[2] + s[2] <= floor_t + in_h + eps;
function inside(name, p, s) = str(name, fits(p, s) ? ": fits" : ": DOES NOT FIT", " (", s[0], " x ", s[1], " x ", s[2], ")");
// boxes the lead must not run through: [min corner, max corner, name], board frame
function bx(p, s, n) = [p, p + s, n];
solids = concat([for (t = tall) bx(t[0], t[1], t[2])],
    bare ? [bx([0, 0, 0], carrier, "carrier")] : [bx([0, 0, 0], [pcb_w, pcb_l, pcb_t], "DevKit board")],
    !bare ? [for (s = [0, 1]) dupont ? bx([pin_x[s] - 1.27, pin_y0 - 1.27, -header_h - dupont_hsg[2]], [2.54, pin_pitch * pin_n, header_h + dupont_hsg[2]], "header + housings")
                                       : bx([pin_x[s] - 0.32, pin_y0 - 0.32, -stub_h], [0.64, pin_pitch * (pin_n - 1) + 0.64, stub_h], "pin stubs")] : [],
    bay_h > 0 ? [bx([-side_air, 1, -bay_h], [1, tab_y - 2, bay_h], "ledge"), bx([board_w + side_air - 1, 1, -bay_h], [1, tab_y - 2, bay_h], "ledge")] : [],
    [for (t = tabs) bx([t[0], tab_y + 0.2, tab_z], [t[1] - t[0], 1.0, board_t - 0.1 - tab_z], "stop tab")],
    !bare ? [bx([pk_x[0], er_y - wall, pk_z - (pk_shelf ? wall : bay_h)], [pk_w + wall, end_room + wall, 99], "UART pocket")] : []);
function hit(a, b) = a[0][0] < b[1][0] && b[0][0] < a[1][0] && a[0][1] < b[1][1] && b[0][1] < a[1][1] && a[0][2] < b[1][2] && b[0][2] < a[1][2];
// the lead's straight runs as boxes (the short drop from the splice is left out: it is where the wires join)
function seg_box(a, b) = [[min(a[0], b[0]), min(a[1], b[1]), min(a[2], b[2])] - [1, 1, 1] * wire_d / 2, [max(a[0], b[0]), max(a[1], b[1]), max(a[2], b[2])] + [1, 1, 1] * wire_d / 2];
lead_boxes = [for (i = [0 : lead_n - 1]) let(p = lead_path(i)) for (k = [1 : len(p) - 2]) seg_box(p[k], p[k + 1])];
lead_hits = [for (s = solids) if (len([for (b = lead_boxes) if (hit(b, s)) 1]) > 0) s[2]];
parts_fit = fits(charger_p, charger) && fits(lipo_p, lipo) && fits(boost_p, boost) && fits(mosfet_p, mosfet) && fits(diode_p, diode) && fits(res_p[1], resistor)
    && (bare ? fits([0, 0, 0], [carrier[0], carrier[1], carrier[2] + wroom[2]]) : fits([0, -usbc_lip, -(dupont ? header_h + pin_len : stub_h)], [pcb_w, module_y[1] + usbc_lip, (dupont ? header_h + pin_len : stub_h) + pcb_t + usbc_h]))
    && (!dupont || fits([pin(0, 1)[0] - 1.27, pin(0, 1)[1] - 1.27, -header_h - dupont_hsg[2]], dupont_hsg))
    && (!dupont || slack_h > 2);
box_ok = parts_fit && len(lead_hits) == 0;
echo(str("wiring = ", wiring, ", layout = ", layout, ", cable_exit = ", cable_exit, side ? str(" (wall ", cable_wall, ")") : "", ": box outside ", out_l, " x ", out_w, " x ", base_h + lid_t,
         " mm; inside ", in_l, " x ", in_w, " x ", in_h, " mm; end room ", end_room, " mm"));
if (bare) echo(inside("carrier board with the WROOM module", [0, 0, 0], [carrier[0], carrier[1], carrier[2] + wroom[2]]));
else echo(inside("DevKit incl. antenna, USB-C lips and header/stubs", [0, -usbc_lip, -(dupont ? header_h + pin_len : stub_h)], [pcb_w, module_y[1] + usbc_lip, (dupont ? header_h + pin_len : stub_h) + pcb_t + usbc_h]));
if (dupont) echo(inside("Dupont housing on the 5V pin", [pin(0, 1)[0] - 1.27, pin(0, 1)[1] - 1.27, -header_h - dupont_hsg[2]], dupont_hsg));
echo(inside("charger", charger_p, charger)); echo(inside("LiPo", lipo_p, lipo)); echo(inside("boost", boost_p, boost));
echo(inside("MOSFET", mosfet_p, mosfet)); echo(inside("diode", diode_p, diode)); echo(inside("resistor", res_p[1], resistor));
if (dupont) echo(str("coiled slack: ", round(slack_vol), " mm3 of wire (", 4 * usb_wire_len + 3 * ctrl_wire_len, " mm), end room ", end_room, " mm, coil ", slack_h > 2 ? "fits" : "DOES NOT FIT", " above the small parts"));
if (!bare) echo(str("UART USB-C: in an open-topped pocket ", pk_w, " x ", wall + end_room, " mm in the end wall, so a ", usbc_plug[0], " x ", usbc_plug[1], " plug reaches the socket"));
echo(str("cable lead (", cable_exit, " exit): ", len(lead_hits) == 0 ? "clear of every part" : str("RUNS THROUGH ", lead_hits)));
echo(str("lid lip notched over: ", len(lip_notched) == 0 ? "nothing" : [for (t = lip_notched) t[2]]));
echo(str("box: ", box_ok ? "all parts fit" : "SOMETHING DOES NOT FIT", "; ", flat ? "row beside the board ends at y = " : "row under the board ends at y = ", boost_p[1] + boost[1], " of ", flat ? y_far - lip_room - (side ? lead_ext[0] + 1.0 : 0) : board_l, " available"));

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
