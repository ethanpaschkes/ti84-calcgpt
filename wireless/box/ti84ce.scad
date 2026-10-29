// TI-84 Plus CE, 1:1 outside model, and the Calc box pack sitting on its back.
//
// Sources: TI's published size 7.59 x 3.42 x 0.80 in (192.8 x 86.9 x 20.3 mm, with the slide cover on); screen 2.8 in diagonal,
// 320 x 240 pixels (active area 56.9 x 42.7 mm). Everything else (key grid, screen window, port position, cover lips) is taken from
// Ethan's photos of 2026-10-29 scaled to that width: estimates good to about 1 mm, marked PHOTO. No interior: this is the shell.
//
// Frame: x across the calculator (0 = left edge seen from the front), y along it (0 = bottom edge, 192.8 = top edge where the
// mini-USB port is), z up from the back face (keys at z = body_t).
// scene = "calc" | "calc_back" | "pack_on_calc" | "pack_on_calc_exploded"
scene = "pack_on_calc";
pack_wiring = "dupont";

body_l = 192.8; body_w = 86.9;
body_t = 19.0;              // the body without the cover (PHOTO: cover adds ~1.3 on the back)
cover_t = 1.3; cover_lip = 1.6; cover_l = 186;          // PHOTO: white slide cover, stops ~7 mm short of the top edge
calc_r = 9;                 // PHOTO: the body corners
edge_round = 3;             // PHOTO: rounded body edges
screen_w = 62; screen_h = 48; screen_top = 13;          // PHOTO: window; the glass area is a little larger than the active 56.9 x 42.7
usb_from_right = 17; usb_w = 7.7; usb_h = 3.9;          // PHOTO: mini-B port on the top edge, right of centre, centred in the thickness
key_cols = [14.7, 30.4, 46.3, 62.0, 77.7];              // PHOTO: column centres from the left edge, 15.7 mm pitch
key_row0 = 18.7; key_pitch = 12.4; key_rows = 10;       // PHOTO: bottom row centre, 12.4 mm pitch, 10 rows up to y= / window / ...
arrow_c = [70.0, key_row0 + 7.5 * key_pitch]; arrow_d = 24;   // PHOTO: the arrow pad, right of the 2nd / alpha rows
key_h = 1.8;

module rrect(l, w, h, r) { hull() for (x = [r, l - r], y = [r, w - r]) translate([x, y, 0]) cylinder(h = h, r = r, $fn = 40); }

// the body: a rounded slab whose edges are rounded by hulling a smaller top slab with the full one
module body() {
    color("#1b1d22") hull() {
        translate([edge_round, edge_round, 0]) rrect(body_w - 2 * edge_round, body_l - 2 * edge_round, 0.01, calc_r - edge_round);
        translate([0, 0, edge_round]) rrect(body_w, body_l, body_t - 2 * edge_round, calc_r);
        translate([edge_round, edge_round, body_t - 0.01]) rrect(body_w - 2 * edge_round, body_l - 2 * edge_round, 0.01, calc_r - edge_round);
    }
}

module screen() {
    color("#0d1a24") translate([(body_w - screen_w) / 2, body_l - screen_top - screen_h, body_t - 0.5]) cube([screen_w, screen_h, 1.0]);
}

module keys() {
    // rows from the bottom: 0 = "0 . (-) enter", 1-3 = number rows, 4-6 = letter rows (x^2, x^-1, math), 7 = alpha row, 8 = 2nd row, 9 = y= row
    for (r = [0 : key_rows - 1], c = [0 : 4]) {
        y = key_row0 + r * key_pitch;
        number = r <= 3 && c >= 1;                               // the white number pad and the operator column
        skip = (r >= 7 && r <= 8 && c >= 3) || (r == 9 && false);  // the arrow pad takes the top-right of rows 7-8
        if (!skip) color(number || r == 9 ? "#e9e6dc" : "#2a2c31")
            translate([key_cols[c] - (r == 9 ? 6 : number ? 6.5 : 5.5), y - (r == 9 ? 3 : number ? 4.5 : 3.5), body_t])
                cube([r == 9 ? 12 : number ? 13 : 11, r == 9 ? 6 : number ? 9 : 7, key_h]);
    }
    color("#e9e6dc") translate([arrow_c[0], arrow_c[1], body_t]) difference() { cylinder(h = key_h, d = arrow_d, $fn = 48); translate([0, 0, -1]) cylinder(h = key_h + 2, d = 8, $fn = 32); }
}

// the slide cover on the back: a shallow tray whose lips wrap the sides
module cover() {
    color("#f2f1ec") difference() {
        translate([-cover_t, -cover_t, -cover_t]) rrect(body_w + 2 * cover_t, cover_l + cover_t, cover_t + cover_lip + edge_round, calc_r + cover_t);
        translate([0, 0, 0]) rrect(body_w, cover_l + 2, body_t, calc_r);       // the body's room (open toward the top edge)
        translate([-5, -5, cover_t + cover_lip + edge_round - 0.01]) cube([body_w + 10, cover_l + 10, body_t]);
    }
}

module usb_port() {
    color("#555") translate([body_w - usb_from_right - usb_w / 2, body_l - 6, body_t / 2 - usb_h / 2]) cube([usb_w, 7, usb_h]);
}

module ti84ce(with_cover = true) {
    difference() { body(); usb_port(); }
    screen(); keys();
    if (with_cover) cover();
}

// --- the pack on the back ---------------------------------------------------------------------------------------------
include <calcbox.scad>
part = "none";
wiring = pack_wiring;
pack_from_top = 22;         // the pack's cable end sits this far below the top edge, so the plug + adapter (80 mm) loop to the port
module pack(exploded = false) {
    // the box's frame has X along the board (USB end at low X). Turn it so X runs DOWN the calculator (-y) with the USB end at the
    // top, centre it across, and flip it so its floor rests on the cover's back (z = -cover_t) with the lid facing away.
    translate([(body_w - out_w) / 2, body_l - pack_from_top, -with_cover_t()]) rotate([0, 0, -90]) mirror([0, 0, 1]) {
        base(); devkit(); parts(); wires(); cable_outside();
        if (!exploded) translate([0, out_w, base_h + lid_t]) mirror([0, 1, 0]) mirror([0, 0, 1]) lid();
    }
}
function with_cover_t() = cover_t;   // the pack sits on the cover's back face

if (scene == "calc") ti84ce(true);
if (scene == "calc_nocover") ti84ce(false);
if (scene == "cover_only") cover();
if (scene == "pack_only") pack(false);
if (scene == "calc_back") rotate([180, 0, 0]) ti84ce(true);
if (scene == "pack_on_calc") { ti84ce(true); pack(false); }
if (scene == "pack_on_calc_exploded") { ti84ce(true); pack(true); }
echo(str("TI-84 Plus CE shell ", body_w, " x ", body_l, " x ", body_t, " (+", cover_t, " cover); pack ", out_l, " x ", out_w, " x ", base_h + lid_t,
         " on the back, cable end ", pack_from_top, " mm below the top edge, port ", usb_from_right, " mm from the right"));
