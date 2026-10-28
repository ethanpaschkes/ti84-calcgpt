#ifndef VIEWER_H
#define VIEWER_H

#include <stdint.h>

/* Understands the pictures a Mac sends to the calculator and draws them into screen memory.
 * Nothing here knows about the calculator's graphics library, so it can be tested on a computer.
 *
 * Messages, Mac to calculator (one stream of bytes, in any chunk sizes):
 *   'P' + 48 bytes                the first 16 colours, as red, green, blue
 *   'F' + 9600 bytes              a whole 160x120 picture, 4 bits per pixel, two pixels per byte (left pixel in the high
 *                                 half), shown at double size
 *   'M' + 9600 bytes              a whole 320x240 picture, 1 bit per pixel, 8 pixels per byte (leftmost in the high bit)
 *   'G' + 19200 bytes             a whole 320x240 picture, 2 bits per pixel, 4 pixels per byte (leftmost in the high bits)
 *   'f' or 'm' or 'g' + y + n + rows
 *                                 only n changed rows starting at row y, packed like F, M or G (80, 40 or 80 bytes per row;
 *                                 F rows are 160x120 rows, shown double height)
 *   'I' + length + text           a short badge (up to 15 characters) in the top left corner; length 0 hides it */

#define VIEWER_WIDTH 320
#define VIEWER_HEIGHT 240
#define VIEWER_BADGE_MAX 15

struct viewer_host {
    uint8_t *vram;                                                  /* 320 x 240 bytes, one byte per pixel */
    void (*set_palette)(const uint8_t *rgb);                        /* 16 colours, 3 bytes each */
    void (*draw_badge)(const char *text, int length);               /* called after each picture and when the badge changes */
};

void viewer_init(const struct viewer_host *host);

/* hand over bytes as they arrive from the USB link */
void viewer_feed(const uint8_t *data, int length);

#endif
