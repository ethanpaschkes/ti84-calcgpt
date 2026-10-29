#ifndef PROTOCOL_H
#define PROTOCOL_H

/* The bridge's pure parts: cutting the calculator's bytes into whole messages, following the server's messages so the board's own
 * messages only go between them, the buffer of those own messages, the Wi-Fi list entries, and the log line formats.
 * No ESP-IDF code in here, so tests/test_protocol.c compiles it on a computer. Messages: ../../PROTOCOL.md */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---- the calculator's messages: Q len text, M index, J len ssid len password, and single letters ---- */
struct calc_reader {
    uint8_t msg[2 + 255 + 1 + 255];
    size_t have;
};

/* the length of the complete message at the start of r->msg, or 0 if more bytes are needed */
size_t calc_message_length(const struct calc_reader *r);

/* ---- the server's messages (A, T, L, V, Y: length then text; U, X: one byte; the rest: one letter) ---- */
struct server_reader {
    int left;   /* bytes still to come of the current message; -1 = its length byte comes next; 0 = between messages */
};

void server_follow(struct server_reader *s, const uint8_t *data, size_t n);

/* ---- the board's own messages for the calculator, kept until they can be sent ---- */
#define LOCAL_OUT_MAX 1024
struct local_out {
    uint8_t buf[LOCAL_OUT_MAX];
    size_t len;
};

/* tag + len + text; a tag alone when text is NULL. Text longer than 255 bytes is cut; a message that does not fit is dropped. */
void local_add(struct local_out *o, char tag, const char *text);
/* tag + len + any bytes (a Wi-Fi list entry starts with its signal strength, 0 to 4) */
void local_bytes(struct local_out *o, char tag, const char *data, size_t n);
/* tag + one byte */
void local_byte(struct local_out *o, char tag, uint8_t value);

/* ---- Wi-Fi list entries ('Y'): byte 0 = signal 0-4, byte 1 = '*' locked or ' ', byte 2 = '+' remembered or ' ', then the name ---- */
#define SCAN_ENTRY_MAX (3 + 32)
int signal_bars(int rssi);
/* fills entry (SCAN_ENTRY_MAX + 1 bytes) and returns its length */
size_t scan_entry(char *entry, int rssi, bool locked, bool known, const char *ssid);

/* ---- log lines ---- */
#define LOG_LINE_MAX 256   /* the buffer every log line is formatted into before it leaves over UDP (was 200: too short for the status line) */
#define RESET_NAME_MAX 24

/* the name of an esp_reset_reason_t value */
const char *reset_name_of(unsigned why);

/* the 10 s status line: uptime s, reset name, Wi-Fi name, IP text, calculator text, free heap, lowest heap ever,
 * stack never used by the status task / the session task (bytes) */
#define STATUS_FMT "status: up %lu s (after %s), Wi-Fi %s at %s, calc %s, heap %u min %u, stack free %u/%u"
#define CALC_HERE_TEXT "connected"
#define CALC_GONE_TEXT "no USB signal"
/* the first line after Wi-Fi comes up: reset name, free heap */
#define RESET_FMT "this start came after: %s; free memory %u bytes"

#endif
