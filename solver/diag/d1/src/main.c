/* DIAG1: the simplest possible check. Shows a screen and the code of the last key pressed.
 * If this crashes, the problem is the calculator setup (libraries, memory), not the solver. */
#include <graphx.h>
#include <keypadc.h>
#include <ti/getcsc.h>
#include <stdint.h>

int main(void)
{
    uint8_t last = 0;
    int count = 0;

    gfx_Begin();
    gfx_SetDrawBuffer();
    for (;;) {
        uint8_t k = os_GetCSC();
        if (k) {
            last = k;
            count++;
        }
        gfx_FillScreen(255);
        gfx_SetTextFGColor(0);
        gfx_PrintStringXY("DIAG1  basic screen and keys", 10, 10);
        gfx_PrintStringXY("Press keys. Clear quits.", 10, 30);
        gfx_PrintStringXY("Last key code:", 10, 60);
        gfx_SetTextXY(130, 60);
        gfx_PrintInt(last, 3);
        gfx_PrintStringXY("Keys pressed:", 10, 76);
        gfx_SetTextXY(130, 76);
        gfx_PrintInt(count, 4);
        gfx_SwapDraw();
        if (last == sk_Clear) break;
    }
    gfx_End();
    return 0;
}
