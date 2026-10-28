/* DIAG8: evaluates x^2 at x = 0 from deeper and deeper in the call stack.
 * Press a key after each line. Tell me the last line you saw before the crash. */
#include <graphx.h>
#include <keypadc.h>
#include <ti/getcsc.h>
#include <stdbool.h>
#include <stdint.h>
#include "mathx.h"

static int line_y = 28;
static struct mx_prog prog;

static void ok(const char *what, unsigned where)
{
    gfx_SetTextFGColor(0);
    gfx_PrintStringXY(what, 10, line_y);
    gfx_PrintUInt(where, 8);
    line_y += 12;
    gfx_PrintStringXY("press a key", 230, 10);
    while (!os_GetCSC()) {
    }
    gfx_SetColor(255);
    gfx_FillRectangle(230, 10, 90, 10);
}

/* Reserves exactly `pad` bytes of stack, shows where that stack space starts, then fills it. */
static real probe(int pad)
{
    volatile char buf[pad > 0 ? pad : 1];
    int i;
    ok("space starts at", (unsigned)&buf[0]);
    for (i = pad - 1; i >= 0; i--) buf[i] = (char)i;
    return mx_run(&prog, 0.0L);
}

/* The calculator's own stack is only about 4 KB, so the program runs on a bigger one kept in ordinary memory. */
#define BIG_STACK 20000
uint8_t big_stack[BIG_STACK];
unsigned saved_sp;

static int run(void);

int main(void)
{
    int r;
    __asm__ volatile("ld (_saved_sp), sp");
    __asm__ volatile("ld hl, _big_stack + 20000\n\tld sp, hl" ::: "hl");
    r = run();
    __asm__ volatile("ld sp, (_saved_sp)");
    return r;
}

static int run(void)
{
    static const int pads[] = {0, 3000, 4000, 6000, 8000, 12000, 16000};
    int i;

    gfx_Begin();
    gfx_FillScreen(255);
    gfx_SetTextFGColor(0);
    gfx_PrintStringXY("DIAG8  x^2 at different stack depths", 10, 10);
    ok("compile", mx_compile("x^2", 0, &prog));
    for (i = 0; i < 7; i++) {
        real v = probe(pads[i]);
        ok(v == 0.0L ? "  pad ok, depth" : "  pad WRONG", (unsigned)pads[i]);
    }
    ok("ALL DONE", 0);
    while (!os_GetCSC()) {
    }
    gfx_End();
    return 0;
}
