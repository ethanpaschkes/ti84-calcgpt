#include "num.h"
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

int num_decimals = 2;

static const float POW10[6] = {1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f, 100000.0f};

static int put_uint(uint32_t v, char *out)
{
    char tmp[12];
    int k = 0, n = 0;
    do {
        tmp[k++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (k) out[n++] = tmp[--k];
    return n;
}

void fmt_num(float v, int dec, char *out)
{
    int n = 0, k;
    uint32_t scaled, unit, frac;
    bool neg = v < 0;

    if (v != v) {
        strcpy(out, "undefined");
        return;
    }
    if (neg) v = -v;
    if (dec < 0) dec = 0;
    if (dec > 5) dec = 5;
    while (dec > 0 && v * POW10[dec] > 4.0e9f) dec--;
    if (v > 4.0e9f) {
        strcpy(out, neg ? "-huge" : "huge");
        return;
    }
    unit = (uint32_t)POW10[dec];
    scaled = (uint32_t)(v * POW10[dec] + 0.5f);
    frac = scaled % unit;
    if (neg && scaled) out[n++] = '-';
    n += put_uint(scaled / unit, out + n);
    if (frac) {
        out[n++] = '.';
        for (k = 0; k < dec; k++) {
            out[n + dec - 1 - k] = (char)('0' + frac % 10);
            frac /= 10;
        }
        n += dec;
        while (out[n - 1] == '0') n--;
    }
    out[n] = 0;
}

bool parse_num(const char *s, float *v)
{
    bool neg = false, digits = false, dot = false;
    float val = 0, scale = 0.1f;
    if (*s == '-') {
        neg = true;
        s++;
    }
    for (; *s; s++) {
        if (*s >= '0' && *s <= '9') {
            digits = true;
            if (!dot) {
                val = val * 10.0f + (float)(*s - '0');
            } else {
                val += (float)(*s - '0') * scale;
                scale *= 0.1f;
            }
        } else if (*s == '.' && !dot) {
            dot = true;
        } else {
            return false;
        }
    }
    if (!digits) return false;
    *v = neg ? -val : val;
    return true;
}

/* ---------------------------------------------------------------- say */

static char *o_, *end_;

void say_to(char *buf, int cap)
{
    o_ = buf;
    end_ = buf + cap - 1;
    *o_ = 0;
}

static void put(const char *s)
{
    while (*s && o_ < end_) *o_++ = *s++;
    *o_ = 0;
}

void say(const char *fmt, ...)
{
    va_list ap;
    char buf[NUM_CHARS];
    va_start(ap, fmt);
    for (; *fmt; fmt++) {
        if (*fmt == '%' && fmt[1]) {
            fmt++;
            if (*fmt == 's') {
                put(va_arg(ap, const char *));
            } else if (*fmt == 'n') {
                fmt_num((float)va_arg(ap, double), num_decimals, buf);
                put(buf);
            } else if (*fmt == 'd') {
                int v = va_arg(ap, int);
                if (v < 0) {
                    put("-");
                    v = -v;
                }
                buf[put_uint((uint32_t)v, buf)] = 0;
                put(buf);
            } else {
                buf[0] = *fmt;
                buf[1] = 0;
                put(buf);
            }
        } else {
            buf[0] = *fmt;
            buf[1] = 0;
            put(buf);
        }
    }
    va_end(ap);
}

/* ---------------------------------------------------------------- wrapping */

int wrap_text(const char *text, int cols, char out[][WRAP_W], int max_lines)
{
    int n = 0;
    if (cols > WRAP_W - 1) cols = WRAP_W - 1;
    while (*text && n < max_lines) {
        int len = 0, cut;
        while (text[len] && text[len] != '\n') len++;
        cut = len;
        if (cut > cols) {
            cut = cols;
            while (cut > 0 && text[cut] != ' ') cut--;
            if (cut == 0) cut = cols;   /* one long word: break it */
        }
        memcpy(out[n], text, (size_t)cut);
        out[n][cut] = 0;
        n++;
        text += cut;
        while (*text == ' ') text++;
        if (*text == '\n') text++;
    }
    return n;
}
