#ifndef LINK_H
#define LINK_H

#include <stdbool.h>
#include <stdint.h>

/* The USB serial link to the Mac (calcgptd.py over the cable) or to the bridge board (srldrvce; calculator only). The calculator
 * is the USB host and powers the board through this link, so the board starts when link_init() runs and stops at link_close().
 * Shared by CALCGPT and CALC so both show the same Online/Offline. */

/* starts USB; false if the calculator refused (nothing to clean up then) */
bool link_init(void);
/* call often: USB events, and the serial port is opened when a device appears */
void link_poll(void);
/* a serial device is connected */
bool link_up(void);
/* true once after the device went away, so the program can forget what it knew about the other side */
bool link_lost(void);
void link_send(const uint8_t *data, int n);
/* bytes that arrived, up to n; 0 when there are none */
int link_read(uint8_t *buf, int n);
/* lets the last bytes leave (about half a second of events), then shuts USB down */
void link_close(void);

#endif
