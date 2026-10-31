#include "form.h"
#include <string.h>
#include "keys.h"
#include "num.h"
#include "typing.h"

void form_init(struct form *fm, const char *title, const char *hint, const char *const *labels, int n)
{
    int i;
    memset(fm, 0, sizeof(*fm));
    fm->title = title;
    fm->hint = hint;
    fm->n = n > FORM_FIELDS ? FORM_FIELDS : n;
    for (i = 0; i < fm->n; i++) fm->f[i].label = labels[i];
    fm->cols = fm->n > 6 ? 2 : 1;
}

int form_rows(const struct form *fm)
{
    return (fm->n + fm->cols - 1) / fm->cols;
}

/* the character a number key types, or 0 */
static char number_char(int key)
{
    struct typing plain = {false, false};
    char ch = 0;
    if (typing_key(&plain, key, &ch) != TY_CHAR) return 0;
    if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-') return ch;
    return 0;
}

static int move(struct form *fm, int to)
{
    if (to < 0 || to > fm->n || to == fm->cur) return FORM_NONE;
    fm->cur = to;
    return FORM_CHANGED;
}

int form_key(struct form *fm, int key)
{
    struct form_field *f = fm->cur < fm->n ? &fm->f[fm->cur] : NULL;
    char ch;
    if (key == K_CLEAR) return FORM_BACK;
    if (key == K_ENTER) {
        if (fm->cur == fm->n) return FORM_SOLVE;
        return move(fm, fm->cur + 1);
    }
    if (key == K_UP) return move(fm, fm->cur - 1);
    if (key == K_DOWN) return move(fm, fm->cur + 1);
    if (key == K_LEFT || key == K_RIGHT) {
        int rows = form_rows(fm), to;
        if (fm->cols == 1 || fm->cur == fm->n) return FORM_NONE;
        to = key == K_LEFT ? fm->cur - rows : fm->cur + rows;
        return to >= 0 && to < fm->n ? move(fm, to) : FORM_NONE;
    }
    if (!f) return FORM_NONE;
    if (key == K_DEL) {
        if (!f->len) return FORM_NONE;
        f->text[--f->len] = 0;
        return FORM_CHANGED;
    }
    ch = number_char(key);
    if (!ch || f->len >= FIELD_CHARS) return FORM_NONE;
    if (ch == '-' && f->len) return FORM_NONE;                      /* a minus sign only at the start */
    if (ch == '.' && strchr(f->text, '.')) return FORM_NONE;        /* one dot */
    f->text[f->len++] = ch;
    f->text[f->len] = 0;
    return FORM_CHANGED;
}

bool form_value(const struct form *fm, int i, float *v)
{
    return i >= 0 && i < fm->n && fm->f[i].len && parse_num(fm->f[i].text, v);
}

int form_filled(const struct form *fm)
{
    int i, n = 0;
    for (i = 0; i < fm->n; i++) n += fm->f[i].len > 0;
    return n;
}
