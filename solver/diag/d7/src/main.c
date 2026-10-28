/* DIAG7: solves x^2 = 4x - 3 and prints a marker (T1, T2, ...) as each stage is reached.
 * If the calculator crashes, tell me the LAST T number on the screen. */
#include <graphx.h>
#include <keypadc.h>
#include <ti/getcsc.h>
#include <stdbool.h>
#include "mathx.h"
#include "solve.h"

static unsigned count;

/* Draws only the latest marker in one fixed box, so the screen after a crash shows the last one reached. */
void mx_trace(int n)
{
    gfx_SetColor(255);
    gfx_FillRectangle(10, 60, 300, 40);
    gfx_SetTextFGColor(0);
    gfx_SetTextXY(10, 64);
    gfx_PrintString("last marker T");
    gfx_PrintInt(n, 1);
    gfx_SetTextXY(10, 80);
    gfx_PrintString("markers so far ");
    gfx_PrintUInt(++count, 1);
}

int main(void)
{
    static struct mx_solution sol;

    gfx_Begin();
    gfx_FillScreen(255);
    gfx_SetTextFGColor(0);
    gfx_PrintStringXY("DIAG7  solving x^2 = 4x - 3", 10, 10);
    gfx_PrintStringXY("sol at", 10, 28);
    gfx_PrintUInt((unsigned)&sol, 7);
    mx_degrees = true;
    mx_solve("x^2", "4x-3", &sol);
    mx_trace(99);
    gfx_PrintStringXY(sol.kind == SK_LIST && sol.n == 2 ? "DONE: 2 solutions found" : "DONE but result is wrong", 100, 28);
    while (!os_GetCSC()) {
    }
    gfx_End();
    return 0;
}
