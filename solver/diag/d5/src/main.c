/* DIAG5: how much memory is free? SOLVER needs about 110,000 bytes free in RAM to run. */
#include <graphx.h>
#include <keypadc.h>
#include <ti/getcsc.h>
#include <ti/vars.h>
#include <stdint.h>

int main(void)
{
    void *free_ptr = 0;
    size_t free_bytes = os_MemChk(&free_ptr);
    int local = 0;

    gfx_Begin();
    gfx_FillScreen(255);
    gfx_SetTextFGColor(0);
    gfx_PrintStringXY("DIAG5  memory check", 10, 10);
    gfx_PrintStringXY("Free RAM (bytes):", 10, 40);
    gfx_SetTextXY(10, 54);
    gfx_PrintInt((int)free_bytes, 8);
    gfx_PrintStringXY("SOLVER needs about 110000 free.", 10, 80);
    gfx_PrintStringXY(free_bytes >= 125000 ? "OK: enough room." : "NOT ENOUGH: free more RAM.", 10, 96);
    gfx_PrintStringXY("Stack address (hex digits):", 10, 130);
    gfx_SetTextXY(10, 144);
    gfx_PrintInt((int)((uintptr_t)&local >> 12), 6);
    gfx_PrintStringXY("Press any key to quit", 10, 226);
    while (!os_GetCSC()) {
    }
    gfx_End();
    return 0;
}
