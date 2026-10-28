#include "viewer.h"
#include <string.h>

enum { S_CMD, S_PALETTE, S_FRAME, S_DELTA_HEAD, S_DELTA_ROWS, S_BADGE_LEN, S_BADGE_TEXT };

#define PALETTE_BYTES 48
#define FRAME_MAX 19200

static const struct viewer_host *host;
static uint8_t data_buf[FRAME_MAX];
static uint8_t pal_rgb[PALETTE_BYTES];
static char badge[VIEWER_BADGE_MAX + 1];
static int badge_len;

/* lookup tables so each source byte becomes its 8 (or 4) screen pixels with one copy */
static uint8_t tab1[256][8];
static uint8_t tab2[256][4];

static int state, have, want, kind, delta_y, delta_n;
static uint8_t head[2];

void viewer_init(const struct viewer_host *h)
{
    int b, i;
    host = h;
    for (b = 0; b < 256; b++) {
        for (i = 0; i < 8; i++) tab1[b][i] = (uint8_t)((b >> (7 - i)) & 1);
        for (i = 0; i < 4; i++) tab2[b][i] = (uint8_t)((b >> (6 - 2 * i)) & 3);
    }
    state = S_CMD;
    have = want = kind = badge_len = 0;
}

/* source rows y0 .. y0 + n - 1 of a 160x120 picture, drawn twice as wide and twice as tall */
static void draw_color_rows(int y0, int n)
{
    int y;
    for (y = y0; y < y0 + n; y++) {
        uint8_t *row = host->vram + y * 640;
        const uint8_t *src = data_buf + (y - y0) * 80;
        int x;
        for (x = 0; x < 80; x++) {
            uint8_t b = src[x], hi = b >> 4, lo = b & 15;
            row[0] = row[1] = hi;
            row[2] = row[3] = lo;
            row += 4;
        }
        memcpy(host->vram + y * 640 + 320, host->vram + y * 640, 320);
    }
}

static void draw_mono_rows(int y0, int n)
{
    uint8_t *dst = host->vram + y0 * 320;
    const uint8_t *src = data_buf;
    int i;
    for (i = 0; i < n * 40; i++) {
        memcpy(dst, tab1[src[i]], 8);
        dst += 8;
    }
}

static void draw_gray_rows(int y0, int n)
{
    uint8_t *dst = host->vram + y0 * 320;
    const uint8_t *src = data_buf;
    int i;
    for (i = 0; i < n * 80; i++) {
        memcpy(dst, tab2[src[i]], 4);
        dst += 4;
    }
}

/* kind is F, M or G (whole picture) or f, m or g (some rows) */
static void draw_rows(int k, int y0, int n)
{
    switch (k) {
    case 'F': case 'f': draw_color_rows(y0, n); break;
    case 'M': case 'm': draw_mono_rows(y0, n); break;
    default: draw_gray_rows(y0, n); break;
    }
}

static int row_bytes(int k)
{
    return (k == 'm' || k == 'M') ? 40 : 80;
}

static int row_count(int k)
{
    return (k == 'f' || k == 'F') ? 120 : 240;
}

static void command(uint8_t c)
{
    if (c == 'P') {
        state = S_PALETTE; have = 0; want = PALETTE_BYTES;
    } else if (c == 'I') {
        state = S_BADGE_LEN;
    } else if (c == 'F' || c == 'M' || c == 'G') {
        state = S_FRAME; have = 0; kind = c;
        want = row_bytes(c) * row_count(c);
    } else if (c == 'f' || c == 'm' || c == 'g') {
        state = S_DELTA_HEAD; have = 0; kind = c;
    }
}

void viewer_feed(const uint8_t *data, int length)
{
    int pos = 0;
    while (pos < length) {
        if (state == S_CMD) {
            command(data[pos++]);
        } else if (state == S_BADGE_LEN) {
            int len = data[pos++];
            if (len > VIEWER_BADGE_MAX) len = VIEWER_BADGE_MAX;
            want = len;
            have = 0;
            if (len == 0) {
                badge_len = 0;
                host->draw_badge("", 0);
                state = S_CMD;
            } else {
                state = S_BADGE_TEXT;
            }
        } else if (state == S_BADGE_TEXT) {
            badge[have++] = (char)data[pos++];
            if (have == want) {
                badge_len = want;
                host->draw_badge(badge, badge_len);
                state = S_CMD;
            }
        } else if (state == S_DELTA_HEAD) {
            head[have++] = data[pos++];
            if (have == 2) {
                delta_y = head[0];
                delta_n = head[1];
                have = 0;
                want = delta_n * row_bytes(kind);
                if (want == 0 || want > FRAME_MAX) {
                    state = S_CMD;  /* nothing to draw, or more than a picture can hold: ignore */
                } else {
                    state = S_DELTA_ROWS;
                }
            }
        } else {
            int take = length - pos;
            uint8_t *dst = state == S_PALETTE ? pal_rgb : data_buf;
            if (take > want - have) take = want - have;
            memcpy(dst + have, data + pos, (size_t)take);
            have += take;
            pos += take;
            if (have == want) {
                if (state == S_PALETTE) {
                    host->set_palette(pal_rgb);
                } else if (state == S_FRAME) {
                    draw_rows(kind, 0, row_count(kind));
                    host->draw_badge(badge, badge_len);
                } else if (delta_y + delta_n <= row_count(kind)) {
                    draw_rows(kind, delta_y, delta_n);
                    host->draw_badge(badge, badge_len);
                }
                state = S_CMD;
            }
        }
    }
}
