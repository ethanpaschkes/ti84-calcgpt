#include <stdio.h>
#include <string.h>
#include "edit.h"
static int fails = 0;
static void C(const struct editor *e, const char *want, int cur)
{
    if (strcmp(e->s, want) || e->cur != cur) { printf("FAIL '%s' cur %d want '%s' cur %d\n", e->s, e->cur, want, cur); fails++; }
}
int main(void)
{
    struct editor e;
    char l[40], r[40];
    ed_clear(&e); ed_insert(&e, "1"); ed_fraction(&e); C(&e, "[1|]", 3); ed_insert(&e, "2"); C(&e, "[1|2]", 4);
    ed_clear(&e); ed_insert(&e, "x"); ed_insert(&e, "+"); ed_insert(&e, "3"); ed_insert(&e, "x"); ed_fraction(&e); C(&e, "x+[3x|]", 6);
    ed_clear(&e); ed_insert(&e, "sin("); C(&e, "sin()", 4); ed_insert(&e, "x"); ed_close_paren(&e); C(&e, "sin(x)", 6); ed_fraction(&e); C(&e, "[sin(x)|]", 8);
    ed_clear(&e); ed_insert(&e, "x"); ed_fraction(&e); ed_backspace(&e); C(&e, "x", 1);
    ed_clear(&e); ed_insert(&e, "2sin("); ed_backspace(&e); C(&e, "2", 1);
    ed_clear(&e); ed_insert(&e, "x^2"); ed_append_equals(&e); ed_insert(&e, "4"); C(&e, "x^2=4", 5);
    ed_split("x^2=4", l, r, 40); if (strcmp(l, "x^2") || strcmp(r, "4")) { printf("FAIL split\n"); fails++; }
    ed_split("3+4=", l, r, 40); if (strcmp(l, "3+4") || strcmp(r, "")) { printf("FAIL split2\n"); fails++; }
    ed_clear(&e); ed_insert(&e, "[1|2]"); ed_insert(&e, "x"); ed_fraction(&e); C(&e, "[[1|2]x|]", 8);
    ed_clear(&e); ed_insert(&e, "2+"); ed_fraction(&e); C(&e, "2+[|]", 3);
    ed_clear(&e); ed_insert(&e, "x=3"); ed_insert(&e, "x"); ed_fraction(&e); C(&e, "x=[3x|]", 6);
    printf(fails ? "%d FAILED\n" : "edit: all passed\n", fails);
    return fails != 0;
}
