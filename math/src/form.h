#ifndef FORM_H
#define FORM_H

#include <stdbool.h>

/* The one form popup every problem type uses: up to ten number fields and a Solve row. The calculator's number keys type
 * (0-9, the dot, the (-) key for a minus sign), Del erases, Enter moves to the next field and finally to Solve, the arrows move
 * between fields, Clear goes back. No drawing in here; the screen reads the fields. Key numbers: ../../shared/keys.h */

#define FORM_FIELDS 10
#define FIELD_CHARS 9

struct form_field {
    const char *label;
    char text[FIELD_CHARS + 1];
    int len;
};

struct form {
    const char *title;
    const char *hint;
    int n;                            /* fields; row n is the Solve row */
    struct form_field f[FORM_FIELDS];
    int cur;                          /* the chosen row, 0 to n */
    int cols;                         /* 1, or 2 when there are more than 6 fields */
};

enum { FORM_NONE, FORM_CHANGED, FORM_SOLVE, FORM_BACK };

void form_init(struct form *fm, const char *title, const char *hint, const char *const *labels, int n);
/* a key went down; FORM_CHANGED when something to redraw changed */
int form_key(struct form *fm, int key);
/* field i as a number; false when it is empty or not a number */
bool form_value(const struct form *fm, int i, float *v);
/* how many fields have something typed */
int form_filled(const struct form *fm);
/* rows in each column */
int form_rows(const struct form *fm);

#endif
