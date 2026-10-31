#ifndef TOOLS_H
#define TOOLS_H

#include <stdbool.h>
#include "answer.h"
#include "chatmodel.h"
#include "form.h"
#include "outbuf.h"
#include "probs.h"
#include "tri.h"

/* The tools ChatGPT can use ON the calculator (../../wireless/PROTOCOL.md, section 5). The server sends a request as a status
 * message whose text starts with byte 1: "form|Title|hint|a|b|c", "choice|Title|opt|opt", "answers|Title|rule|line|line",
 * "steps|Title|line|line", "triangle|a|b|c|A|B|C", "parabola|a|b|c", "lines|a|b|c|d", "circle|deg", "dots|n|n|...",
 * "decimals|n", "status". The calculator answers with a question message whose text starts with byte 1: "form|a=3|b=4",
 * "form|cancel", "choice|opt", "ok", "status|time=..|battery=..|online=yes", "error|why". A form or a choice waits for the
 * user; the others answer at once and the card stays up until Enter or Clear. No drawing in here (tests/tools_test.c). */

enum { TOOL_NONE, TOOL_FORM, TOOL_CHOICE, TOOL_ANSWERS, TOOL_STEPS, TOOL_TRIANGLE, TOOL_PARABOLA, TOOL_LINES, TOOL_CIRCLE,
       TOOL_DOTS };

#define TOOL_PARTS 24
#define TOOL_OPTIONS 8

struct calc_status {
    int hh, mm, battery;
    bool online;
};

struct tool_ui {
    int kind;
    bool open;                         /* a card is showing (keys go to it) */
    char text[CHAT_TOOL_MAX + 1];      /* the request, cut into parts */
    const char *part[TOOL_PARTS];
    int nparts;
    const char *title;
    const char *options[TOOL_OPTIONS];
    int noptions, sel;
    struct form form;
    struct answer ans;
    /* the numbers behind a diagram */
    struct tri_sol tri;
    struct quad_out quad;
    float qa, qb, qc, la, lb, lc, ld, lx;
    int lkind;
    struct trig_out trig;
    struct stats_out stats;
};

void tools_init(struct tool_ui *t);
/* a request arrived (the bytes after the marker); replies that need no user go to out at once. Returns the kind. */
int tool_open(struct tool_ui *t, const char *payload, int len, struct outbuf *out, const struct calc_status *st);
/* a key while a card is open; the card may close and its reply go to out */
void tool_key(struct tool_ui *t, int key, struct outbuf *out);
const char *tool_help(const struct tool_ui *t);

#endif
