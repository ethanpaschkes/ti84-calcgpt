#ifndef BRIDGE_H
#define BRIDGE_H

/* The calculator <-> Wi-Fi <-> server bridge (bridge.c): Wi-Fi, the server login, the session that copies bytes and answers the
 * calculator's Wi-Fi setup messages, the 10 s status line. It talks to the calculator through calclink.h, so the same code runs
 * on the device-mode board (this project) and in the host-mode loader box (../../esp32s3-loader).
 * Call after nvs_flash_init() and calclink_start(). */
void bridge_start(void);

#endif
