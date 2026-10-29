#ifndef ROLE_H
#define ROLE_H

/* Which USB role the box takes at power-on, and the hand-over between them (../../BOX_DESIGN.md, "One cable, two roles"):
 *   ROLE_HOST    the box powers the cable and is the USB host: loader.c installs what is missing and starts CALC.
 *   ROLE_DEVICE  the calculator is the host (CALC or CALCGPT turned USB on) and powers the box: the bridge runs over the
 *                USB-Serial/JTAG port, and the LiPo charges.
 * The hand-over is a restart: the ESP32-S3's USB pins can be the OTG host or the Serial/JTAG device, and switching cleanly is
 * done by restarting with a flag kept in RTC memory. With BOX_HW 0 the role is always ROLE_HOST. */

enum { ROLE_HOST, ROLE_DEVICE };

/* reads the restart flag and the 5 V sense pin; sets the ID and 5 V-out pins for the role it returns */
int role_decide(void);

/* the loader is done with the calculator (programs sent, CALC started): give the host role to the calculator. With BOX_HW this
 * turns the cable's 5 V off, grounds the ID pin and restarts into ROLE_DEVICE; without it, nothing happens (the DevKitC-1 stays
 * the host and talks to CALC through link_cdc). */
void role_hand_over(void);

/* in ROLE_DEVICE: watches the BOOT button; a press restarts into ROLE_HOST (install or start CALC again) */
void role_watch_button(void);

#endif
