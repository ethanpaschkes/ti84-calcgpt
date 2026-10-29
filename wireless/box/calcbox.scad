// Calc box: a slim pack for the ESP32-S3 DevKitC-1 (header pins cut flush), a 150 mAh LiPo, the charger and the boost,
// to stick on the back of a TI-84 Plus CE. Two parts: base (tray) and lid (friction fit). All sizes in mm.
// Print: PLA or PETG, 0.2 mm layers, base open side up, lid flat side down, no supports needed.
// Edit the numbers below, then: openscad -o base.stl -D 'part="base"' calcbox.scad ; openscad -o lid.stl -D 'part="lid"' calcbox.scad

part = "both";            // "base", "lid" or "both" (both = side by side for a look)

// --- what goes inside -----------------------------------------------------------------------
board_l = 63.5;           // DevKitC-1 length
board_w = 25.5;           // DevKitC-1 width
board_t = 1.6;            // PCB thickness
top_h = 6.5;              // tallest thing on the board's top side (USB-C sockets, module shield)
bay_h = 5.5;              // room under the board: LiPo (4 mm) and charger with its micro-USB (4.5 mm)
wire_room = 8;            // extra length at the far end for wires, the MOSFET, diode and resistors
side_room = 1.0;          // air each side of the board

// --- walls and fit ------------------------------------------------------------------------
wall = 1.6;
floor_t = 1.6;
lid_t = 1.6;
lip_h = 2.5;              // the lid's lip that goes inside the walls
fit = 0.25;               // clearance per side for the lip
corner_r = 3;             // outer corner radius

// --- cutouts (positions along the board, x from the USB end; y from the board's centre line) ---
uart_x_off = -6.3;        // the UART USB-C connector's centre, left of the board's centre line seen from the USB end
usbc_w = 9.6; usbc_h = 3.6;
open_usb_port = false;    // true also opens the port marked USB (keep false: a charger there breaks the calculator link)
boot_from_end = 8.5;      // BOOT button centre, mm from the USB end of the board
boot_y_off = 9.5;         // ... and from the centre line (negative = other side)
boot_hole_d = 6;
led_from_end = 14;        // the RGB LED, same measuring
led_y_off = 9.5;
led_hole_d = 2.2;
cable_slot_w = 11;        // the jumper wires leave at the far end, near the floor
cable_slot_h = 5;

// --- derived ------------------------------------------------------------------------------
in_l = board_l + 2 * side_room + wire_room;
in_w = board_w + 2 * side_room;
in_h = bay_h + board_t + top_h;
out_l = in_l + 2 * wall;
out_w = in_w + 2 * wall;
base_h = floor_t + in_h;
board_z = floor_t + bay_h;                 // underside of the PCB
board_x0 = wall + side_room;               // the USB end of the board (x grows toward the wire room)
cx = out_w / 2;                            // the board's centre line

module rbox(l, w, h, r) {
    hull() for (x = [r, l - r], y = [r, w - r]) translate([x, y, 0]) cylinder(h = h, r = r, $fn = 32);
}

module base() {
    difference() {
        rbox(out_l, out_w, base_h, corner_r);
        // the cavity
        translate([wall, wall, floor_t]) rbox(in_l, in_w, in_h + 1, corner_r - wall);
        // USB-C port(s) in the end wall at the board's USB end
        for (off = open_usb_port ? [uart_x_off, -uart_x_off] : [uart_x_off])
            translate([-1, cx + off - usbc_w / 2, board_z + board_t]) cube([wall + 2, usbc_w, usbc_h + 1.5]);
        // cable slot in the far end wall, at the floor
        translate([out_l - wall - 1, cx - cable_slot_w / 2, floor_t]) cube([wall + 2, cable_slot_w, cable_slot_h]);
    }
    // four corner ledges the board rests on (the LiPo and charger lie underneath)
    for (x = [board_x0, board_x0 + board_l - 3], y = [wall, out_w - wall - 3])
        translate([x, y, floor_t]) cube([3, 3, bay_h]);
    // a stop so the board cannot slide toward the wire room
    translate([board_x0 + board_l + 0.3, wall, floor_t]) cube([1.2, in_w, bay_h + board_t]);
}

module lid() {
    difference() {
        union() {
            rbox(out_l, out_w, lid_t, corner_r);
            translate([wall + fit, wall + fit, lid_t]) rbox(in_l - 2 * fit, in_w - 2 * fit, lip_h, corner_r - wall);
        }
        // hollow the lip so it is a ring, not a plug
        translate([wall + fit + 1.2, wall + fit + 1.2, lid_t]) rbox(in_l - 2 * fit - 2.4, in_w - 2 * fit - 2.4, lip_h + 1, 1);
        // BOOT button and LED holes (the lid is printed flat side down, so these are mirrored in y when it is turned over)
        translate([board_x0 + boot_from_end, cx - boot_y_off, -1]) cylinder(h = lid_t + 2, d = boot_hole_d, $fn = 32);
        translate([board_x0 + led_from_end, cx - led_y_off, -1]) cylinder(h = lid_t + 2, d = led_hole_d, $fn = 24);
    }
}

if (part == "base") base();
if (part == "lid") lid();
if (part == "both") { base(); translate([0, out_w + 10, 0]) lid(); }
echo(str("outside: ", out_l, " x ", out_w, " x ", base_h + lid_t, " mm; inside: ", in_l, " x ", in_w, " x ", in_h, " mm"));
