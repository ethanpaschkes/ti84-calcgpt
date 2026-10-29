/* Every key number in ../../shared/keys.h against CEdev's own keypadc.h (compiled with -I $CEDEV/include): the group is the
 * kb_Data index and the bit is the mask in that header. Bug (b) of 2026-10-29: [mode] seemed dead and there was no test that
 * K_MODE = 14 really is kb_Data[1] & kb_Mode. Run by test_calcgpt.py. */
#include <keypadc.h>
#include <stdio.h>
#include "../../shared/keys.h"
#include "../../shared/typing.h"

static int fails;

static void check(const char *name, int key, int group, int mask)
{
    if (K_GROUP(key) != group || K_MASK(key) != mask) {
        printf("FAIL %s: key %d is group %d mask 0x%02x, keypadc.h says group %d mask 0x%02x\n", name, key, K_GROUP(key), K_MASK(key), group, mask);
        fails++;
    }
}

int main(void)
{
    check("Graph", K_GRAPH, 1, kb_Graph);
    check("Trace", K_TRACE, 1, kb_Trace);
    check("Zoom", K_ZOOM, 1, kb_Zoom);
    check("Window", K_WINDOW, 1, kb_Window);
    check("Y=", K_YEQU, 1, kb_Yequ);
    check("2nd", K_2ND, 1, kb_2nd);
    check("Mode", K_MODE, 1, kb_Mode);
    check("Del", K_DEL, 1, kb_Del);
    check("Alpha", K_ALPHA, 2, kb_Alpha);
    check("Enter", K_ENTER, 6, kb_Enter);
    check("Clear", K_CLEAR, 6, kb_Clear);
    check("Down", K_DOWN, 7, kb_Down);
    check("Left", K_LEFT, 7, kb_Left);
    check("Right", K_RIGHT, 7, kb_Right);
    check("Up", K_UP, 7, kb_Up);
    if (K_MODE != 14) {
        printf("FAIL [mode] must be key 14 (group 1, bit 6), got %d\n", K_MODE);
        fails++;
    }
    if (KEY_2ND != K_2ND || KEY_ALPHA != K_ALPHA) {
        printf("FAIL typing.h's 2nd/Alpha numbers (%d, %d) differ from keys.h (%d, %d)\n", KEY_2ND, KEY_ALPHA, K_2ND, K_ALPHA);
        fails++;
    }
    if (fails) return 1;
    printf("key numbers match keypadc.h\n");
    return 0;
}
