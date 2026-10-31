// TI-84 Plus CE, 1:1 outside model (the shell, the keys, the screen window, the mini-USB port on the right side, the battery
// door on the back, and the white slide cover as its own part), plus scenes that put the Calc box pack on its back.
//
// Numbers marked TI come from Texas Instruments' published size: 7.59 x 3.42 x 0.80 in = 192.8 x 86.9 x 20.3 mm with the slide
// cover on; screen 320 x 240 pixels, 2.8 in diagonal. Numbers marked PHOTO were measured in Ethan's photos (2026-10-29) with
// measure/*.py, using the body width 86.9 as the scale (see measure/README.md). Numbers marked MEASURE are guesses for parts no
// photo shows (the back): ruler needed.
//
// Frame: x across the calculator (0 = left edge seen from the front, 86.9 = right edge where the USB port is), y along it
// (0 = bottom edge, 192.8 = top edge), z up from the back face (back face z = 0, key tops at body_t + key_h, cover at z < 0).
//
// scene = "calc" | "calc_nocover" | "calc_back" | "cover_only" | "cover_pocket_only"
//       | "pack_top" | "pack_bottom" | "pack_cover" | "pack_top_exploded"
//       | "pack_only" | "pack_box_only" | "pack_inside_only" | "pack_cable_only"   (the pack alone, placed at pack_place, for STL export)
// pack_wiring = "dupont" | "soldered"   pack_layout = "stacked" | "flat" | "module"   pack_cable_exit = "side" | "end"   (see calcbox.scad)
scene = "calc";
pack_wiring = "soldered";
pack_layout = "flat";
pack_cable_exit = "side";

// --- outside size ------------------------------------------------------------------------------------------------------
body_l = 192.8;             // RULER 2026-10-29: Ethan measured about 193 (TI publishes 192.8). PHOTO heights are rescaled by y_fix below
body_w = 86.9;              // TI (with the cover's side lips)
total_t = 20.3;             // TI, thickest point with the cover: cover plate + body + key tops
cover_t = 1.6;              // PHOTO: the cover's back plate
cover_lip_t = 1.5;          // PHOTO: the cover's side lips, about as thick as the plate
cover_lip_h = 5.0;          // PHOTO: how far the lips reach up the body's sides
key_h = 1.8;                // PHOTO: the keys stand this far above the face
body_t = total_t - cover_t - key_h;     // 16.9: the bare body's thickness
photo_l = 200.4;            // PHOTO: the length the photos add up to
y_fix = body_l / photo_l;   // scales every PHOTO height to body_l (1 as long as body_l is the photo value; 0.962 for TI's 192.8)
calc_r = 12;              // PHOTO: the four corners
front_round = 3;            // PHOTO: the front edge
back_round = 1.5;           // MEASURE: the back edge (no photo of the back)

// --- front: screen window and keys (PHOTO) --------------------------------------------------------------------------------
key_pitch_x = 14.75;                                   // PHOTO: 5 columns, centred
col = [for (k = [0 : 4]) body_w / 2 + (k - 2) * key_pitch_x];
row_y = [14.7, 21.0, 27.8, 32.5, 41.1, 44.0, 54.3, 55.4, 66.9, 78.2, 89.6, 100.9, 112.5, 125.4];   // PHOTO, from the bottom edge
function Y(v) = v * y_fix;
r_zero = 0; r_enter = 1; r_123 = 2; r_plus = 3; r_456 = 4; r_minus = 5; r_789 = 6; r_times = 7; r_x2 = 8; r_xinv = 9; r_math = 10; r_alpha = 11; r_2nd = 12; r_yeq = 13;
num_key = [10.5, 9.2]; op_key = [10.5, 7.6]; top_key = [10.5, 6.3]; dark_key = [10.3, 7.4]; key_r = 1.6;   // PHOTO: cap sizes
on_y = 19.7;                                           // PHOTO: the on key sits a little below the enter row
arrow_c = [col[2] + 22.0, 106.6]; arrow_od = 19.3; arrow_hub = 7.7;                                         // PHOTO
lit_screen = [61.0, 45.9];                 // PHOTO (TI's 2.8 in / 320 x 240 would be 56.9 x 42.7: the scale question again)
window = [70.5, 53.8]; window_r = 4;       // PHOTO: the black glossy window around the lit area
window_bottom = 125.4 + 10.2;              // PHOTO: 10.2 above the y= row centre
window_depth = 0.6;                        // the glass sits a little below the face
bezel_top_text = "TI-84 Plus CE";

// --- right side: mini-USB port (PHOTO) and charging LED ----------------------------------------------------------------------
port_y = 113;               // PHOTO: level with the 2nd / mode / del row, on the RIGHT side edge
port_w = 7.6; port_h = 3.9; // mini-B opening, centred in the body's thickness
led_y = port_y - 9;         // MEASURE: the charging LED sits just below the port on the same edge
cover_notch_l = 12;         // PHOTO: the notch in the cover's right lip at the port

// --- back (MEASURE: no photo of the back yet) ---------------------------------------------------------------------------------
door = [56, 62]; door_y0 = 14; door_screw_d = 5;    // MEASURE: the battery door on the lower back, centred, with its screw
cover_ridges = 10; ridge_zone = 45; ridge_w = 1.2;  // PHOTO: grip ridges on the cover's bottom end

// --- helpers -------------------------------------------------------------------------------------------------------------
module rrect(l, w, h, r) { hull() for (x = [r, l - r], y = [r, w - r]) translate([x, y, 0]) cylinder(h = h, r = r, $fn = 48); }
module keycap(c, s, col_, h = key_h) { color(col_) translate([c[0] - s[0] / 2, Y(c[1]) - s[1] / 2, body_t - 0.2]) rrect(s[0], s[1], h + 0.2, key_r); }

// the shell: a rounded slab whose front edge is rounded more than its back edge
module body() {
    color("#23262c") hull() {
        translate([back_round, back_round, 0]) rrect(body_w - 2 * back_round, body_l - 2 * back_round, 0.01, calc_r - back_round);
        translate([0, 0, back_round]) rrect(body_w, body_l, body_t - back_round - front_round, calc_r);
        translate([front_round, front_round, body_t - 0.01]) rrect(body_w - 2 * front_round, body_l - 2 * front_round, 0.01, calc_r - front_round);
    }
}

module screen() {
    wx = (body_w - window[0]) / 2; wy = Y(window_bottom);
    color("#0b0d12") translate([wx, wy, body_t - window_depth]) rrect(window[0], window[1] * y_fix, window_depth + 0.01, window_r);
    color("#cfe3ef") translate([(body_w - lit_screen[0]) / 2, wy + (window[1] - lit_screen[1]) / 2 * y_fix, body_t - window_depth + 0.01]) cube([lit_screen[0], lit_screen[1] * y_fix, 0.01]);
    color("#e9e6dc") translate([body_w / 2, Y(window_bottom + window[1]) + 5.5, body_t]) linear_extrude(0.05) text(bezel_top_text, size = 4.2, halign = "center", valign = "center", font = "Helvetica:style=Bold");
}

light = "#e9e6dc"; dark = "#1c1d20";
module keys() {
    for (k = [0 : 4]) keycap([col[k], row_y[r_yeq]], top_key, light);                     // y= window zoom trace graph
    keycap([col[0], row_y[r_2nd]], dark_key, "#78b7dc");                                  // 2nd
    for (k = [1, 2]) keycap([col[k], row_y[r_2nd]], dark_key, dark);                      // mode del
    keycap([col[0], row_y[r_alpha]], dark_key, "#9fd35a");                                // alpha
    for (k = [1, 2]) keycap([col[k], row_y[r_alpha]], dark_key, dark);                    // x,T,θ,n stat
    for (k = [0 : 4]) keycap([col[k], row_y[r_math]], dark_key, dark);                    // math apps prgm vars clear
    for (k = [0 : 4]) keycap([col[k], row_y[r_xinv]], dark_key, dark);                    // x⁻¹ sin cos tan ^
    for (k = [0 : 3]) keycap([col[k], row_y[r_x2]], dark_key, dark);                      // x² , ( )
    keycap([col[4], row_y[r_x2]], op_key, light);                                         // ÷
    for (r = [[r_789, r_times], [r_456, r_minus], [r_123, r_plus]]) {
        keycap([col[0], row_y[r[0]]], dark_key, dark);                                    // log ln sto→
        for (k = [1 : 3]) keycap([col[k], row_y[r[0]]], num_key, light, key_h + 0.4);     // 7 8 9 / 4 5 6 / 1 2 3
        keycap([col[4], row_y[r[1]]], op_key, light);                                     // × − +
    }
    keycap([col[0], on_y], dark_key, dark);                                                 // on
    for (k = [1 : 3]) keycap([col[k], row_y[r_zero]], num_key, light, key_h + 0.4);       // 0 . (-)
    keycap([col[4], row_y[r_enter]], op_key, light);                                      // enter
    color(light) translate([arrow_c[0], Y(arrow_c[1]), body_t]) difference() {            // the arrow pad: a ring with four gaps
        cylinder(h = key_h, d = arrow_od, $fn = 64);
        translate([0, 0, -1]) cylinder(h = key_h + 2, d = arrow_hub, $fn = 32);
        for (a = [45 : 90 : 315]) rotate([0, 0, a]) translate([0, -0.8, -1]) cube([arrow_od, 1.6, key_h + 2]);
    }
}

module port_cut() {
    translate([body_w - 6, Y(port_y) - port_w / 2, body_t / 2 - port_h / 2]) cube([7, port_w, port_h]);
    translate([body_w - 1, Y(led_y), body_t / 2]) rotate([0, 90, 0]) cylinder(h = 2, d = 1.6, $fn = 16);
}

module battery_door() {
    color("#2b2e35") translate([(body_w - door[0]) / 2, Y(door_y0), -0.01]) difference() {
        rrect(door[0], door[1], 0.4, 3);
        translate([door[0] / 2, 6, -1]) cylinder(h = 3, d = door_screw_d, $fn = 24);
    }
}

// the slide cover: a plate on the back with lips up the sides and the bottom end, open at the top so it slides on; a notch in
// the right lip at the port; grip ridges across its bottom end
module cover() color("#f2f1ec") cover_shape();
module cover_shape() {
    difference() {
        union() {
            translate([-cover_lip_t, -cover_lip_t, -cover_t]) rrect(body_w + 2 * cover_lip_t, body_l + cover_lip_t, cover_t + cover_lip_h, calc_r + cover_lip_t);
            for (i = [0 : cover_ridges - 1]) translate([8, Y(8) + i * ridge_zone / cover_ridges, -cover_t - 0.5]) cube([body_w - 16, ridge_w, 0.5]);
        }
        translate([0, 0, 0]) rrect(body_w, body_l + 2, body_t, calc_r);                                   // the body's room
        translate([body_w - 1, Y(port_y) - cover_notch_l / 2, -0.5]) cube([cover_lip_t + 2, cover_notch_l, cover_lip_h + 1]);
    }
}

module ti84ce(with_cover = true) {
    difference() { body(); port_cut(); }
    screen(); keys(); battery_door();
    if (with_cover) cover();
}

// --- the pack on the back ------------------------------------------------------------------------------------------------
// the scene's placement (set before the include: OpenSCAD evaluates the overrides below where calcbox.scad first assigns them)
pack_place = "top";                    // the placement the *_only scenes use
place = scene == "pack_bottom" ? "bottom" : scene == "pack_cover" ? "cover" : scene == "pack_top" || scene == "pack_top_exploded" ? "top" : pack_place;
include <calcbox.scad>
part = "none";
wiring = pack_wiring;
layout = pack_layout;
cable_exit = pack_cable_exit;
cable_wall = place_dir(place) > 0 ? 1 : 0;     // the box wall that faces the calculator's right edge (where the port is)
pack_margin = 4;                       // keep the pack this far inside the rounded sides of the back
port_depth = 4;                        // MEASURE: how far a mini-B plug goes into the port
mini_ra = [8, 11, 14];                 // MEASURE: a 90-degree ("up angle") mini-B plug body: out from the side, along the edge, top to cable tail
lead_d = 3;                            // MEASURE: the 5-wire lead (or the 4 Dupont wires) drawn as one bundle
// placements, as the y of the pack's USB end (the end the UART plug goes in) and which way the board points
//   top:    USB end 3 mm below the top edge, board pointing down the back; cover off
//   bottom: USB end 3 mm above the bottom edge, board pointing up the back (over the battery door); cover off
//   cover:  like "top", but the box's walls are printed onto the outside of a new slide cover: the cover plate is the box's floor
function place_y(p) = p == "bottom" ? 3 : body_l - 3;
function place_dir(p) = p == "bottom" ? 1 : -1;
function place_z(p) = 0;
pack_x0 = (body_w - out_w) / 2;
// the box's frame has X along the board (USB end at X = 0), Y across, Z up from the floor. On the back: X runs along the calculator
// from the USB end, centred across, floor against the back, lid away from it (-z); turned over by a rotation (not a mirror), so the
// DevKit is the real one, not its mirror image
function to_calc(place, q) = place_dir(place) > 0 ? [pack_x0 + q[1], place_y(place) + q[0], place_z(place) - q[2]]
                                                  : [pack_x0 + out_w - q[1], place_y(place) - q[0], place_z(place) - q[2]];
module placed(place) {
    translate([pack_x0, place_y(place), place_z(place)])
        if (place_dir(place) > 0) rotate([0, 0, 90]) rotate([180, 0, 0]) children();
        else rotate([0, 0, -90]) translate([0, out_w, 0]) rotate([180, 0, 0]) children();
}
module pack_box(place, exploded) {
    placed(place) {
        color("#2f8ea3") if (place != "cover") base();
        color("#2f8ea3") translate([0, out_w, base_h + lid_t + (exploded ? 20 : 0)]) mirror([0, 1, 0]) mirror([0, 0, 1]) lid();
    }
}
module pack_inside(place) placed(place) { devkit(); parts(); wires(); }
module pack(place, exploded = false) { pack_box(place, exploded); pack_inside(place); cable_to_port(place); }
// one printed part: the slide cover with the box's walls grown on its outer face
module cover_pocket() color("#f2f1ec") union() { cover_shape(); placed("cover") base_walls(); }

// --- the cable from the pack's slot to the port on the right side ----------------------------------------------------------
// soldered: a thin 5-wire lead from the slot across the back, past the right edge, along it to a 90-degree mini-B plug in the port.
// Dupont: Ethan's USB-A plug + OTG adapter make a rigid 68 mm stick straight out of the port; the flexible Dupont wires run from
// its far end back to the slot.
port_c = [body_w, Y(port_y), body_t / 2];
stick_end = body_w - port_depth + otg_adapter[2] + usb_plug[2];
function slot_box() = side ? [bx0 + turn_y, cable_wall == 0 ? -0.5 : out_w + 0.5, bz0 + run_z0 + lead_ext[1] / 2]
                           : [-0.5, by0 + end_x, bz0 + end_z + wire_d / 2];
function slot_c(place) = to_calc(place, slot_box());
function cable_pts(place) = let(s = slot_c(place), tail = [body_w + mini_ra[0] / 2, Y(port_y), body_t / 2 + 4 - mini_ra[2]]) dupont
    ? [s, [stick_end + 6, s[1], s[2]], [stick_end + 6, Y(port_y), body_t / 2], [stick_end, Y(port_y), body_t / 2]]
    : [s, [body_w + mini_ra[0] / 2, s[1], s[2]], tail];
function path_len(p) = len(p) < 2 ? 0 : norm(p[1] - p[0]) + path_len([for (i = [1 : len(p) - 1]) p[i]]);
// Dupont wires also run inside the box: back along the lead's path to the splice, then to the farthest used pin
inside_len = path_len([for (i = [len(lead_path(0)) - 1 : -1 : 0]) lead_path(0)[i]]) + abs(splice[0] - pin_x[0]) + (pin(0, 4)[1] - cross_y);
module cable_to_port(place) {
    p = cable_pts(place);
    color("#333") for (i = [0 : len(p) - 2]) hull() { translate(p[i]) sphere(d = lead_d, $fn = 12); translate(p[i + 1]) sphere(d = lead_d, $fn = 12); }
    if (dupont) {   // the stick, centred on the port: adapter (its mini plug in the port), then the USB-A plug
        color("#333") translate([body_w - port_depth, Y(port_y) - otg_adapter[0] / 2, body_t / 2 - otg_adapter[1] / 2]) cube([otg_adapter[2], otg_adapter[0], otg_adapter[1]]);
        color("#222") translate([body_w - port_depth + otg_adapter[2], Y(port_y) - usb_plug[0] / 2, body_t / 2 - usb_plug[1] / 2]) cube([usb_plug[2], usb_plug[0], usb_plug[1]]);
    } else {        // the 90-degree mini-B: metal in the port, body beside the edge, cable leaving toward the back
        color("#999") translate([body_w - port_depth, Y(port_y) - 3.4, body_t / 2 - 1.5]) cube([port_depth, 6.8, 3]);
        color("#222") translate([body_w, Y(port_y) - mini_ra[1] / 2, body_t / 2 + 4 - mini_ra[2]]) cube([mini_ra[0], mini_ra[1], mini_ra[2]]);
    }
}

if (scene == "calc") ti84ce(true);
if (scene == "calc_nocover") ti84ce(false);
if (scene == "calc_back") rotate([0, 180, 0]) ti84ce(true);
if (scene == "cover_only") cover();
if (scene == "cover_pocket_only") cover_pocket();
if (scene == "pack_only") pack(place);
if (scene == "pack_box_only") pack_box(place, false);
if (scene == "pack_inside_only") pack_inside(place);
if (scene == "pack_cable_only") cable_to_port(place);
if (scene == "pack_top") { ti84ce(false); pack("top"); }
if (scene == "pack_bottom") { ti84ce(false); pack("bottom"); }
if (scene == "pack_cover") { ti84ce(false); cover_pocket(); pack("cover"); }
if (scene == "pack_top_exploded") { ti84ce(false); pack("top", true); }

// --- fit report for the scene ---------------------------------------------------------------------------------------------
if (scene == "calc" || scene == "calc_nocover" || scene == "calc_back")
    echo(str("TI-84 Plus CE shell ", body_w, " x ", body_l, " x ", body_t, " (+", cover_t, " cover, +", key_h, " keys = ", total_t, "); window ", window, ", lit ", lit_screen,
             "; port on the right side ", round(Y(port_y)), " from the bottom (", round(body_l - Y(port_y)), " from the top)"));
function side_name(x) = x > body_w / 2 ? "right" : "left";
function end_name(y) = y > body_l / 2 ? "top" : "bottom";
if (scene == "pack_top" || scene == "pack_bottom" || scene == "pack_cover" || scene == "pack_top_exploded") {
    dir = place_dir(place); y0 = place_y(place); y1 = y0 + dir * out_l;
    added = place == "cover" ? base_h + lid_t - floor_t : base_h + lid_t;    // the cover version shares its floor with the cover plate
    total = (place == "cover" ? total_t : body_t + key_h) + added;
    width_ok = pack_x0 >= pack_margin && pack_x0 + out_w <= body_w - pack_margin;
    length_ok = min(y0, y1) >= 2 && max(y0, y1) <= body_l - 2;
    door_clear = min(y0, y1) > Y(door_y0 + door[1]) || max(y0, y1) < Y(door_y0);
    p = cable_pts(place); lead = path_len(p); s = slot_c(place);
    wires_ok = !dupont || lead + inside_len <= usb_wire_len;
    uart = to_calc(place, B([usbc_x[0], -usbc_lip, 0])); boot = to_calc(place, B([boot_xy[0], boot_xy[1], 0])); led_ = to_calc(place, B([led_xy[0], led_xy[1], 0]));
    ant = to_calc(place, B([pcb_w / 2, module_y[1], 0]));
    pass = width_ok && length_ok && box_ok && wires_ok;
    echo(str("PLACEMENT ", place, " (", wiring, ", ", layout, ", cable exit ", cable_exit, "): pack ", out_l, " x ", out_w, " x ", base_h + lid_t, " on the back, from y = ", min(y0, y1), " to ", max(y0, y1),
             "; width ", width_ok ? "fits" : "DOES NOT FIT", " (", pack_margin, " mm margin each side, x = ", pack_x0, " to ", pack_x0 + out_w, "), length ", length_ok ? "fits" : "DOES NOT FIT",
             "; ", door_clear ? "clear of the battery door" : "over the battery door (pack must come off to change batteries)"));
    echo(str("  thickness added to the ", place == "cover" ? "calculator with its cover on" : "bare calculator", ": ", added, " mm -> total ", total, " mm at the pack (", place == "cover" ? total_t : body_t + key_h, " without it)"));
    if (!bare) echo(str("  UART USB-C socket in the ", side_name(uart[0]), " half (x = ", round(uart[0]), ", y = ", round(uart[1]), "), its plug pocket open at the ", end_name(uart[1]), " end; BOOT hole at x = ", round(boot[0]), ", y = ", round(boot[1]),
             "; LED hole at y = ", round(led_[1]), "; antenna end at y = ", round(ant[1]), ", module facing away from the calculator"));
    else echo(str("  bare module: no USB-C on the carrier (flash over the lead's D-/D+ with BOOT held); BOOT hole at x = ", round(boot[0]), ", y = ", round(boot[1])));
    echo(str("  cable slot ", side ? str("in the side wall facing the right edge, at y = ", round(s[1])) : str("in the end wall at the ", end_name(s[1]), " end, y = ", round(s[1])),
             "; port at y = ", round(Y(port_y)), " on the right side"));
    if (dupont) echo(str("  cable: rigid USB-A plug + OTG adapter stick ", usb_plug[2] + otg_adapter[2], " mm, sticks out ", round(stick_end - body_w), " mm from the right side; flexible Dupont wire from its end to the slot ",
             round(lead), " mm + ", round(inside_len), " mm inside to the pins = ", round(lead + inside_len), " of ", usb_wire_len, " mm (", wires_ok ? str(round(usb_wire_len - lead - inside_len), " mm to coil in the end room") : "TOO SHORT", ")"));
    else echo(str("  cable: ", round(lead), " mm of thin 5-wire lead from the slot to a 90-degree mini-B plug (its body sits ", mini_ra[0], " mm out from the right side)"));
    echo(str("FIT REPORT ", scene, " (", wiring, ", ", layout, ", ", cable_exit, "): ", pass ? "PASS" : "FAIL"));
}
