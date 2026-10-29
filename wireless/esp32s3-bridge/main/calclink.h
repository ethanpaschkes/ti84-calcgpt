#ifndef CALCLINK_H
#define CALCLINK_H

/* The USB link to the calculator, as the bridge (bridge.c) sees it: bytes in, bytes out, "is a calculator program driving the
 * link right now". Two implementations, picked at start with calclink_use():
 *   link_jtag (link_jtag.c): the board is a USB serial DEVICE on the chip's USB-Serial/JTAG port; the calculator is the host
 *              and powers the board (today's wiring: OTG adapter on the calculator; also the box once CALC runs).
 *   link_cdc (../../esp32s3-loader/main/link_cdc.c): the board is the USB HOST (CDC-ACM) with its own power; the calculator
 *              is the device, so the same box can also load programs onto it. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"

struct calclink_ops {
    void (*start)(void);                                            /* driver or host client */
    int (*read)(uint8_t *buf, size_t n, TickType_t wait);           /* bytes from the calculator, 0 when none came in time */
    int (*write)(const uint8_t *buf, size_t n, TickType_t wait);    /* how many were taken, negative when there is no calculator */
    bool (*connected)(void);                                        /* raw "a calculator program is on the other end" */
};

extern const struct calclink_ops link_jtag;
extern const struct calclink_ops link_cdc;

/* chooses the implementation; must come before calclink_start() */
void calclink_use(const struct calclink_ops *ops);
void calclink_start(void);
int calclink_read(uint8_t *buf, size_t n, TickType_t wait);
int calclink_write(const uint8_t *buf, size_t n, TickType_t wait);
bool calclink_connected(void);

#endif
