#ifndef OUTBUF_H
#define OUTBUF_H

#include <stdint.h>

/* Bytes a screen wants sent over the USB link, collected here so the key handling has no USB code in it (and runs in tests on a
 * computer). The program's main loop sends and empties it. */

#define OUTBUF_MAX 400

struct outbuf {
    uint8_t buf[OUTBUF_MAX];
    int len;
};

/* a one-letter command such as 'W' (scan) or 'R' (send the model list) */
void out_cmd(struct outbuf *o, char cmd);
/* tag, one length byte, then n bytes of text (n at most 255); a message that does not fit is dropped */
void out_msg(struct outbuf *o, char tag, const char *text, int n);
/* any bytes as they are */
void out_bytes(struct outbuf *o, const uint8_t *data, int n);

#endif
