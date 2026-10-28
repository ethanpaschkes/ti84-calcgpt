#include "edit.h"
#include <string.h>

void ed_clear(struct editor *e)
{
    e->s[0] = 0;
    e->cur = 0;
}

void ed_set(struct editor *e, const char *text)
{
    strncpy(e->s, text, ED_MAX);
    e->s[ED_MAX] = 0;
    e->cur = (int)strlen(e->s);
}

static void erase_at(char *s, int idx)
{
    memmove(s + idx, s + idx + 1, strlen(s + idx));
}

void ed_insert(struct editor *e, const char *t)
{
    int n = (int)strlen(e->s);
    int m = (int)strlen(t);
    bool paren = m && t[m - 1] == '(';
    int total = m + (paren ? 1 : 0);

    if (!m || n + total > ED_MAX) return;
    memmove(e->s + e->cur + total, e->s + e->cur, n - e->cur + 1);
    memcpy(e->s + e->cur, t, m);
    if (paren) e->s[e->cur + m] = ')';
    e->cur += m;
}

void ed_close_paren(struct editor *e)
{
    if (e->s[e->cur] == ')') e->cur++;
    else ed_insert(e, ")");
}

static bool inside_fraction(const char *s, int c)
{
    int depth = 0, i;
    for (i = 0; i < c; i++) {
        if (s[i] == '[') depth++;
        if (s[i] == ']') depth--;
    }
    return depth > 0;
}

void ed_fraction(struct editor *e)
{
    char tmp[ED_MAX + 4];
    char *s = e->s;
    int n = (int)strlen(s);
    int c = e->cur;
    int i = c;
    int depth = 0;

    if (inside_fraction(s, c)) {
        ed_insert(e, "/");
        return;
    }
    if (n + 3 > ED_MAX) return;
    while (i > 0) {
        char ch = s[i - 1];
        if (ch == ')' || ch == ']') {
            depth++;
        } else if (ch == '(' || ch == '[') {
            if (depth == 0) break;
            depth--;
        } else if (depth == 0 && (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '|' || ch == '=' || ch == ',')) {
            break;
        }
        i--;
    }
    memcpy(tmp, s, i);
    tmp[i] = '[';
    memcpy(tmp + i + 1, s + i, c - i);
    tmp[c + 1] = '|';
    tmp[c + 2] = ']';
    strcpy(tmp + c + 3, s + c);
    strcpy(s, tmp);
    e->cur = (c == i) ? i + 1 : c + 2;
}

void ed_backspace(struct editor *e)
{
    char *s = e->s;
    int *c = &e->cur;
    int n = (int)strlen(s);
    char ch;

    if (*c == 0) return;
    ch = s[*c - 1];
    if (ch == ')' || ch == ']') {
        (*c)--;
        return;
    }
    if (ch == '(' && s[*c] == ')') {
        int st = *c - 1;
        while (st > 0 && s[st - 1] >= 'a' && s[st - 1] <= 'z') st--;
        memmove(s + st, s + *c + 1, n - (*c + 1) + 1);
        *c = st;
        return;
    }
    if (ch == '[') {
        if (s[*c] == '|' && s[*c + 1] == ']') {
            memmove(s + *c - 1, s + *c + 2, n - (*c + 2) + 1);
        }
        (*c)--;
        return;
    }
    if (ch == '|') {
        if (s[*c] == ']') {
            int st = *c - 2;
            while (st >= 0 && s[st] != '[') st--;
            if (st >= 0) {
                erase_at(s, *c);
                erase_at(s, *c - 1);
                erase_at(s, st);
                *c -= 2;
                return;
            }
        }
        (*c)--;
        return;
    }
    erase_at(s, *c - 1);
    (*c)--;
}

void ed_left(struct editor *e)
{
    if (e->cur > 0) e->cur--;
}

void ed_right(struct editor *e)
{
    if (e->s[e->cur]) e->cur++;
}

bool ed_has_equals(const struct editor *e)
{
    return strchr(e->s, '=') != NULL;
}

void ed_append_equals(struct editor *e)
{
    int n = (int)strlen(e->s);
    if (n + 1 > ED_MAX) return;
    e->s[n] = '=';
    e->s[n + 1] = 0;
    e->cur = n + 1;
}

void ed_split(const char *s, char *lhs, char *rhs, int cap)
{
    const char *eq = strchr(s, '=');
    int n;
    if (!eq) {
        strncpy(lhs, s, cap - 1);
        lhs[cap - 1] = 0;
        rhs[0] = 0;
        return;
    }
    n = (int)(eq - s);
    if (n > cap - 1) n = cap - 1;
    memcpy(lhs, s, n);
    lhs[n] = 0;
    strncpy(rhs, eq + 1, cap - 1);
    rhs[cap - 1] = 0;
}
