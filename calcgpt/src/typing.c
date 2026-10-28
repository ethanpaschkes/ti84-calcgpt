#include "typing.h"
#include "typing_tables.h"

int typing_key(struct typing *t, int key, char *ch)
{
    int c;
    if (key < 0 || key > 63) return TY_NOTHING;
    if (key == KEY_ALPHA) {
        t->alpha = !t->alpha;
        return TY_LAYER;
    }
    if (key == KEY_2ND) {
        t->shift = !t->shift;
        return TY_LAYER;
    }
    c = (t->alpha ? TY_LETTERS[key] : 0);
    if (!c && t->shift) c = TY_SHIFTED[key];
    if (!c) c = TY_NORMAL[key];
    if (c && t->alpha && t->shift && c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (!c) return TY_NOTHING;
    t->shift = false;
    *ch = (char)c;
    return TY_CHAR;
}

void typing_used_other_key(struct typing *t)
{
    t->shift = false;
}
