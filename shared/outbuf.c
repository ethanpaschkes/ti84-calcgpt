#include "outbuf.h"
#include <string.h>

void out_bytes(struct outbuf *o, const uint8_t *data, int n)
{
    if (n <= 0 || o->len + n > OUTBUF_MAX) return;
    memcpy(o->buf + o->len, data, (size_t)n);
    o->len += n;
}

void out_cmd(struct outbuf *o, char cmd)
{
    uint8_t b = (uint8_t)cmd;
    out_bytes(o, &b, 1);
}

void out_msg(struct outbuf *o, char tag, const char *text, int n)
{
    uint8_t head[2];
    if (n < 0 || n > 255 || o->len + 2 + n > OUTBUF_MAX) return;
    head[0] = (uint8_t)tag;
    head[1] = (uint8_t)n;
    out_bytes(o, head, 2);
    out_bytes(o, (const uint8_t *)text, n);
}
