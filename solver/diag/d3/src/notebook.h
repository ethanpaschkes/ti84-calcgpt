#ifndef NOTEBOOK_H
#define NOTEBOOK_H

#include <stdbool.h>
#include <stdint.h>
#include "edit.h"
#include "expr.h"
#include "solve.h"

#define LINES_MAX 18
#define LINE_LEN 70

enum { C_BLACK = 0, C_BLUE = 1, C_GRAY = 2, C_RED = 3, C_GREEN = 4 };

/* One notebook entry: what was typed and the lines of its answer. */
struct hentry {
    char input[ED_MAX + 1];
    int nlines;
    char text[LINES_MAX][LINE_LEN];
    uint8_t color[LINES_MAX];
    bool is_step[LINES_MAX];
};

bool nb_has_frac(const char *s);
int nb_row_h(const char *s);
int nb_row_width(const char *s);
bool nb_is_whole(const char *exact);
void nb_plain_text(const char *src, char *dst, int cap);
void nb_add_line(struct hentry *h, const char *text, uint8_t color, bool step);
void nb_add_wrapped(struct hentry *h, const char *text, uint8_t color, bool step);
void nb_build_solution(struct hentry *h, const char *lhs, const char *rhs, const struct mx_solution *s);
void nb_build_eval(struct hentry *h, const struct mx_eval *e);

#endif
