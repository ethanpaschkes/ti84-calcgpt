/* Feeds a recorded stream through the calculator's viewer code (src/viewer.c) on a computer.
 * Usage: viewer_host_test stream.bin dumps.bin [chunk]
 * stream.bin is records of (4-byte little-endian length, bytes). Each record is fed in pieces of random size up to `chunk`
 * (default 64; 1 = byte by byte), then the 320x240 screen memory is appended to dumps.bin.
 * Palette and badge calls are printed as lines: "record PAL <hex>" and "record BADGE <text>". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/viewer.h"

static uint8_t vram[VIEWER_WIDTH * VIEWER_HEIGHT];
static int record;

static void set_palette(const uint8_t *rgb)
{
    int i;
    printf("%d PAL ", record);
    for (i = 0; i < 48; i++) printf("%02x", rgb[i]);
    printf("\n");
}

static void draw_badge(const char *text, int length)
{
    printf("%d BADGE %.*s\n", record, length, text);
}

int main(int argc, char **argv)
{
    struct viewer_host host = {vram, set_palette, draw_badge};
    FILE *in, *out;
    int chunk = argc > 3 ? atoi(argv[3]) : 64;
    uint8_t lenbuf[4];
    if (argc < 3) return 2;
    in = fopen(argv[1], "rb");
    out = fopen(argv[2], "wb");
    if (!in || !out) return 2;
    srand(12345);
    viewer_init(&host);
    while (fread(lenbuf, 1, 4, in) == 4) {
        uint32_t len = lenbuf[0] | (lenbuf[1] << 8) | (lenbuf[2] << 16) | ((uint32_t)lenbuf[3] << 24);
        uint8_t *data = malloc(len ? len : 1);
        uint32_t pos = 0;
        if (fread(data, 1, len, in) != len) return 2;
        while (pos < len) {
            uint32_t n = 1 + (uint32_t)rand() % (uint32_t)chunk;
            if (n > len - pos) n = len - pos;
            viewer_feed(data + pos, (int)n);
            pos += n;
        }
        free(data);
        fwrite(vram, 1, sizeof(vram), out);
        record++;
    }
    fclose(out);
    return 0;
}
