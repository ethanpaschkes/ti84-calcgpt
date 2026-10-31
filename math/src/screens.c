#include "screens.h"
#include <graphx.h>
#include <string.h>
#include "cards.h"
#include "chatmodel.h"
#include "draw.h"
#include "num.h"

/* palette numbers from cards.c that the home list uses */
enum { M_TEXT = 1, M_DIM = 2, M_BLUE = 4, M_GRAY = 8, M_INK = 9, M_FILL = 10, M_SEL = 14 };

void screens_palette(void)
{
    cards_palette();
}

static void draw_home(const struct app *a)
{
    int i;
    char buf[NUM_CHARS];
    text_at("What do you want to solve?", 56, 28, M_DIM);
    for (i = 0; i <= P_COUNT; i++) {
        int y = 46 + i * 24;
        bool sel = i == a->home_sel;
        const char *title = i < P_COUNT ? PTYPES[i].title : "Decimals", *hint = i < P_COUNT ? PTYPES[i].hint : "Left/Right to change";
        if (sel) {
            gfx_SetColor(M_SEL);
            gfx_FillRectangle(8, y - 4, 304, 20);
        }
        text_at(title, 16, y + 2, sel ? M_INK : M_TEXT);
        if (i == P_COUNT) {
            fmt_num((float)num_decimals, 0, buf);
            text_at(buf, 16 + 8 * 10, y + 2, sel ? M_BLUE : M_TEXT);
        }
        text_at(hint, 144, y + 2, sel ? M_GRAY : M_DIM);
    }
}

static void draw_kind(const struct app *a)
{
    static const char *const kinds[2] = {"Right triangle", "Any triangle"};
    draw_home(a);
    card_choice("What kind of triangle?", "right = one 90 degree corner", kinds, 2, a->kind_sel);
}

static void draw_result(const struct app *a)
{
    const char *lines[ANS_LINES];
    int i;
    for (i = 0; i < a->ans.n; i++) lines[i] = a->ans.line[i];
    card_answers("Answers are:", lines, a->ans.n, a->ans.rule,
                 a->tri.n == 2 && (a->ptype == P_TRI || a->ptype == P_PYTH) ? a->ans.n - 1 : -1);
    switch (a->ptype) {
    case P_TRI:
    case P_PYTH: diagram_triangle(&a->tri.sol[a->which]); break;
    case P_QUAD: diagram_quadratic(a->qa, a->qb, a->qc, &a->quad); break;
    case P_LIN: diagram_lines(a->la, a->lb, a->lc, a->ld, a->lkind, a->lx); break;
    case P_UNIT: diagram_unit_circle(&a->trig); break;
    default: diagram_dots(&a->stats); break;
    }
}

static void draw_chat(const struct chat *c)
{
    int i, view = 13;
    cards_card();
    for (i = 0; i < view && c->top + i < c->nrows; i++) {
        const struct chat_row *r = &c->rows[c->top + i];
        char buf[CHAT_COLS + 1];
        int len = r->len > CHAT_COLS - 2 ? CHAT_COLS - 2 : r->len;
        memcpy(buf, c->buf + r->start, (size_t)len);
        buf[len] = 0;
        text_at(buf, r->role == ROLE_USER ? 28 : 20, CARD_Y + 8 + i * 12, r->role == ROLE_USER ? M_GRAY : M_INK);
    }
    if (c->busy) text_at(c->status[0] ? c->status : "Thinking...", 20, CARD_Y + CARD_H - 14, M_BLUE);
}

void screens_draw(const struct app *a, bool online, const struct chat *chat)
{
    cards_background(a->screen == S_HOME || a->screen == S_KIND ? "Math" : a->screen == S_CHAT ? "ChatGPT" : PTYPES[a->ptype].title, online);
    switch (a->screen) {
    case S_HOME: draw_home(a); break;
    case S_KIND: draw_kind(a); break;
    case S_FORM: card_form(&a->form, "Solve"); break;
    case S_RESULT: draw_result(a); break;
    case S_STEPS: card_steps(a->ans.rule, a->ans.steps); break;
    default: draw_chat(chat); break;
    }
    if (a->screen == S_FORM && a->show_err) cards_error(a->ans.err);
    else cards_help(app_help(a, online));
    gfx_SwapDraw();
}
