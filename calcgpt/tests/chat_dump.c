/* Runs a script through the chat store and prints the rows. Script lines: "U text" (you ask), "F hexbytes" (bytes from the Mac).
 * Usage: chat_dump script.txt [chunk]   (the hex bytes are fed in random pieces of 1..chunk bytes) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/chatmodel.h"

static struct chat c;

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "r");
    int chunk = argc > 2 ? atoi(argv[2]) : 16, i;
    static char line[1 << 17];
    static uint8_t bytes[1 << 16];
    if (!f) return 2;
    srand(5);
    chat_init(&c);
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        if (line[0] == 'U') {
            chat_user(&c, line + 2);
        } else if (line[0] == 'F') {
            int n = 0, pos = 0;
            const char *p = line + 2;
            while (p[0] && p[1]) { unsigned v; sscanf(p, "%2x", &v); bytes[n++] = (uint8_t)v; p += 2; }
            while (pos < n) {
                int k = 1 + rand() % chunk;
                if (k > n - pos) k = n - pos;
                chat_feed(&c, bytes + pos, k);
                pos += k;
            }
        }
    }
    chat_layout(&c, CHAT_VIEW_ROWS);
    for (i = 0; i < c.nrows; i++) printf("ROW %c %d %.*s\n", c.rows[i].role, c.rows[i].first, c.rows[i].len, c.buf + c.rows[i].start);
    printf("STATUS %s\nBUSY %d\n", c.status, c.busy);
    return 0;
}
