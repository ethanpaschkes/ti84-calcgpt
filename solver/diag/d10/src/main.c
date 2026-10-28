/* DIAG10: how accurate are the calculator's float maths functions? Each line shows the value it
 * computed and the error against the true value, in parts per million (ppm). 0 or 1 is perfect. */
#include <graphx.h>
#include <keypadc.h>
#include <ti/getcsc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "mathx.h"
#include "exact.h"
#include "triangle.h"

#define BIG_STACK 32000
uint8_t big_stack[BIG_STACK];
unsigned saved_sp;

static int line_y = 4;

static void row(const char *name, float got, float want)
{
    char buf[24];
    int ppm = (int)((got - want) / want * 1e6f);
    gfx_SetTextFGColor(0);
    gfx_PrintStringXY(name, 4, line_y);
    mx_decimal(got, 7, buf, sizeof(buf));
    gfx_PrintStringXY(buf, 120, line_y);
    gfx_SetTextXY(220, line_y);
    gfx_PrintInt(ppm, 1);
    gfx_PrintString(" ppm");
    line_y += 11;
}

static int run(void)
{
    volatile float one = 1.0f, half = 0.5f, two = 2.0f, v = 0.4797689f;
    struct tri_in in;
    static char out[1800];
    const char *p;

    gfx_Begin();
    gfx_FillScreen(255);
    row("sinf(1)", sinf(one), 0.84147098f);
    row("cosf(0.5)", cosf(half), 0.87758256f);
    row("tanf(1)", tanf(one), 1.55740772f);
    row("asinf(0.5)", asinf(half), 0.52359878f);
    row("acosf(0.5)", acosf(half), 1.04719755f);
    row("acosf(0.4797689)", acosf(v), 1.07094f);
    row("atanf(1)", atanf(one), 0.78539816f);
    row("sqrtf(2)", sqrtf(two), 1.41421356f);
    row("logf(2)", logf(two), 0.69314718f);
    row("expf(1)", expf(one), 2.71828183f);
    row("powf(2,0.5)", powf(two, half), 1.41421356f);
    row("sinf(0.9677)", sinf(0.9677f * one), 0.82339f);
    memset(&in, 0, sizeof(in));
    in.has_s[0] = in.has_s[1] = in.has_s[2] = true;
    in.s[0] = 1028; in.s[1] = 1046; in.s[2] = 965;
    tri_work(&in, out, sizeof(out));
    p = strstr(out, "Area");
    gfx_SetTextFGColor(0);
    gfx_PrintStringXY("Bermuda area (true 442816):", 4, line_y + 6);
    gfx_PrintStringXY(p ? p + 20 : "?", 4, line_y + 18);
    gfx_PrintStringXY("angles:", 4, line_y + 34);
    p = strstr(out, "A = ");
    {
        const char *q = strstr(out, "\nA = ");
        gfx_PrintStringXY(q ? q + 1 : (p ? p : "?"), 4, line_y + 46);
    }
    gfx_PrintStringXY("press a key", 4, 226);
    while (!os_GetCSC()) {
    }
    gfx_End();
    return 0;
}

int main(void)
{
    int r;
    __asm__ volatile("ld (_saved_sp), sp");
    memset(big_stack, 0xA5, BIG_STACK);
    __asm__ volatile("ld hl, _big_stack + 32000\n\tld sp, hl" ::: "hl");
    r = run();
    __asm__ volatile("ld sp, (_saved_sp)");
    return r;
}
