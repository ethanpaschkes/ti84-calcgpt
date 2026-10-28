/* Reads lines "key alpha shift" from stdin and prints what typing_key does: the character code, "layer" or "none". */
#include <stdio.h>
#include "../src/typing.h"

int main(void)
{
    int key, alpha, shift;
    while (scanf("%d %d %d", &key, &alpha, &shift) == 3) {
        struct typing t = {alpha != 0, shift != 0};
        char ch = 0;
        int r = typing_key(&t, key, &ch);
        if (r == TY_CHAR) printf("%d %d %d\n", (unsigned char)ch, t.alpha, t.shift);
        else printf("%s %d %d\n", r == TY_LAYER ? "layer" : "none", t.alpha, t.shift);
    }
    return 0;
}
