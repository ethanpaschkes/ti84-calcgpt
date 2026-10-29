#ifndef CALCLINK_H
#define CALCLINK_H

/* The USB link to the calculator, as the bridge (bridge.c) sees it: bytes in, bytes out, "is a calculator program driving the
 * link right now". Two implementations:
 *   link_jtag.c (this project): the board is a USB serial DEVICE on the chip's USB-Serial/JTAG port; the calculator is the
 *                host and powers the board (today's wiring: OTG adapter on the calculator).
 *   ../../esp32s3-loader/main/link_cdc.c: the board is the USB HOST (CDC-ACM) with its own power; the calculator is the device,
 *                so the same box can also load programs onto it (the goal: plug into any calculator). */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"

/* starts the link (driver or host client); the bridge's receive task then calls calclink_read in a loop */
void calclink_start(void);
/* bytes from the calculator, waiting up to `wait` ticks; 0 when none came */
int calclink_read(uint8_t *buf, size_t n, TickType_t wait);
/* bytes to the calculator; how many were taken, or a negative number when there is no calculator */
int calclink_write(const uint8_t *buf, size_t n, TickType_t wait);
/* the raw "a calculator program is on the other end" signal (the bridge smooths it over 3 s) */
bool calclink_connected(void);

#endif
