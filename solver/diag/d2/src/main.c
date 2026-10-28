/* DIAG2: does the 64-bit math work on the calculator, and how fast is it?
 * Shows each result times 1,000,000 as a whole number so you can read it. */
#include <graphx.h>
#include <keypadc.h>
#include <ti/getcsc.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

static void show(const char *name, long double v, int y)
{
    gfx_PrintStringXY(name, 10, y);
    gfx_SetTextXY(130, y);
    gfx_PrintInt((int)(v * 1000000.0L), 9);
}

int main(void)
{
    long double acc = 0;
    int i;
    clock_t t0, t1;

    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_FillScreen(255);
    gfx_SetTextFGColor(0);
    gfx_PrintStringXY("DIAG2  64-bit maths test", 10, 10);
    gfx_PrintStringXY("Working... (a few seconds)", 10, 30);
    gfx_SwapDraw();

    t0 = clock();
    for (i = 0; i < 100; i++) acc += sinl((long double)i * 0.01L) + powl(1.0001L, (long double)i) + sqrtl((long double)i);
    t1 = clock();

    gfx_FillScreen(255);
    gfx_PrintStringXY("DIAG2  64-bit maths test", 10, 10);
    show("sin(0.5)", sinl(0.5L), 40);
    show("sqrt(2)", sqrtl(2.0L), 56);
    show("2 ^ 0.5", powl(2.0L, 0.5L), 72);
    show("asin(0.5)", asinl(0.5L), 88);
    show("ln(2)", logl(2.0L), 104);
    show("1/3", 1.0L / 3.0L, 120);
    gfx_PrintStringXY("(correct: 479426, 1414213, 1414213,", 10, 144);
    gfx_PrintStringXY(" 523598, 693147, 333333)", 10, 156);
    gfx_PrintStringXY("100 rounds of sin+pow+sqrt took", 10, 180);
    gfx_SetTextXY(10, 192);
    gfx_PrintInt((int)((t1 - t0) * 1000 / CLOCKS_PER_SEC), 6);
    gfx_PrintString(" milliseconds");
    gfx_SetTextXY(10, 210);
    gfx_PrintInt((int)acc, 6);
    gfx_PrintStringXY("Press any key to quit", 10, 226);
    gfx_SwapDraw();
    while (!os_GetCSC()) {
    }
    gfx_End();
    return 0;
}
