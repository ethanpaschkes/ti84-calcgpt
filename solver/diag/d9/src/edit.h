#ifndef EDIT_H
#define EDIT_H

#include <stdbool.h>

/* The Desmos-style line editor: stacked fractions and self-closing brackets.
 * A fraction is stored as "[numerator|denominator]". */

#define ED_MAX 60

struct editor {
    char s[ED_MAX + 1];
    int cur;
};

void ed_clear(struct editor *e);
void ed_set(struct editor *e, const char *text);   /* cursor goes to the end */
void ed_insert(struct editor *e, const char *t);   /* "sin(" also adds the closing bracket */
void ed_close_paren(struct editor *e);             /* ")" steps over a self-closed one */
void ed_fraction(struct editor *e);                /* the "/" key: stack the term on the left */
void ed_backspace(struct editor *e);
void ed_left(struct editor *e);
void ed_right(struct editor *e);
bool ed_has_equals(const struct editor *e);
void ed_append_equals(struct editor *e);           /* put an = at the end */

/* Split "lhs=rhs" at the first '='. rhs is empty when there is nothing after it. */
void ed_split(const char *s, char *lhs, char *rhs, int cap);

#endif
