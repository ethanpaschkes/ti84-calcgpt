#include "launch.h"

int launch_tokens(uint8_t *out, const char *program)
{
    int n = 0, i;
    out[n++] = 0xBB;   /* two-byte token: Asm( */
    out[n++] = 0x6A;
    out[n++] = 0x5F;   /* prgm */
    for (i = 0; program[i] && i < 8; i++) {
        char ch = program[i];
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) out[n++] = (uint8_t)ch;   /* letters and digits are their own tokens */
    }
    return n;
}
