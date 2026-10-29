/* Puts CALCGPT, SOLVER, their C libraries and (on OS 5.8.5) the arTIfiCE jailbreak on any TI-84 Plus CE plugged in at its
 * home screen. See loader.c. */
#pragma once
#include "usb/usb_host.h"

/* registers its own USB host client and starts watching for calculators; call after usb_host_install() */
void loader_start(void);
