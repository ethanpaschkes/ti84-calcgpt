#ifndef BOX_PINS_H
#define BOX_PINS_H

/* The box's extra wiring (../../BOX_DESIGN.md). BOX_HW 0 = the bare DevKitC-1 as used so far: no battery, no ID control, always
 * the USB host, powered from its UART USB-C port or 5V pin. BOX_HW 1 = the LiPo box; the pin numbers below are placeholders until
 * the wiring exists: change them here only. */
#define BOX_HW 0

#define PIN_VBUS_SENSE 4    /* high when the cable carries 5 V from the calculator (100 k / 100 k divider) */
#define PIN_VBUS_OUT_EN 5   /* high = the boost converter puts 5 V on the cable (the box is the host) */
#define PIN_ID_LOW 6        /* high = the MOSFET grounds the cable's ID pin (the calculator becomes the host) */
#define PIN_BOOT_BUTTON 0   /* the board's BOOT button: in device mode, press to install / start CALC again */

#endif
