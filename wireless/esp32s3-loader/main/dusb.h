/* TI's "DUSB" link protocol, host side: what TI Connect CE uses to put variables on a TI-84 Plus CE that sits at its home screen
 * (the calculator's own USB device, 0451:E008). Only what the loader needs: say hello, read the OS version, list the variables,
 * send a variable or an app. Written from libticalcs2 (github.com/debrouxl/tilibs: dusb_rpkt.cc, dusb_vpkt.cc, dusb_cmd.cc, calc_84p.cc). */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "usb/usb_host.h"

#define TI_VID 0x0451
#define TI84CE_PID 0xE008

#define DUSB_NAME_MAX 9   /* 8 characters and the terminating 0 */

typedef struct {
    usb_host_client_handle_t client;
    usb_device_handle_t dev;
    uint8_t ep_in, ep_out;
    uint16_t in_mps;
    usb_transfer_t *xfer_in, *xfer_out;
    uint32_t raw_max;        /* largest raw packet the calculator accepts (agreed at the start) */
    uint8_t in_buf[64];      /* bytes read from the calculator and not used yet */
    uint16_t in_len, in_pos;
} dusb_t;

/* called for each variable on the calculator */
typedef void (*dusb_var_cb)(const char *name, uint8_t type, bool archived, void *arg);

/* claims the calculator's interface; false if this device is not a TI-84 Plus CE link */
bool dusb_open(dusb_t *d, usb_host_client_handle_t client, usb_device_handle_t dev);
void dusb_close(dusb_t *d);

/* buffer size and "normal mode": must come first */
bool dusb_hello(dusb_t *d);
/* OS version as major, minor, patch (5.8.5 is {5, 8, 5}) */
bool dusb_os_version(dusb_t *d, uint8_t version[3]);
/* every variable and app on the calculator */
bool dusb_list(dusb_t *d, dusb_var_cb cb, void *arg);
/* puts one variable on the calculator (replacing one of the same name) */
bool dusb_send_var(dusb_t *d, const char *name, uint8_t type, uint8_t version, bool archived, const uint8_t *data, uint32_t size);
/* puts a flash app on the calculator (the signed data after a .8ek file's 0x4E-byte header); the calculator checks the signature */
bool dusb_send_app(dusb_t *d, const char *name, const uint8_t *data, uint32_t size);
