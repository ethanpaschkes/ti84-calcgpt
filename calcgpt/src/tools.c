#include "tools.h"
#include <string.h>
#include "keys.h"
#include "num.h"

void tools_init(struct tool_ui *t)
{
    memset(t, 0, sizeof(*t));
    t->ans.rule = "";
}

/* the calculator's answer to a tool: Q len, byte 1, text */
static void reply(struct outbuf *out, const char *text)
{
    char buf[256];
    int n = (int)strlen(text);
    if (n > 254) n = 254;
    buf[0] = 1;
    memcpy(buf + 1, text, (size_t)n);
    out_msg(out, 'Q', buf, n + 1);
}

static void reply_error(struct outbuf *out, const char *what)
{
    char buf[80];
    say_to(buf, sizeof(buf));
    say("error|%s", what);
    reply(out, buf);
}

/* part i as a number; false (and an error reply) when it is missing or not a number */
static bool number(struct tool_ui *t, int i, float *v, struct outbuf *out)
{
    if (i < t->nparts && parse_num(t->part[i], v)) return true;
    reply_error(out, "a number is missing or not a number");
    return false;
}

static void split(struct tool_ui *t, const char *payload, int len)
{
    int i;
    if (len > CHAT_TOOL_MAX) len = CHAT_TOOL_MAX;
    memcpy(t->text, payload, (size_t)len);
    t->text[len] = 0;
    t->nparts = 0;
    t->part[t->nparts++] = t->text;
    for (i = 0; i < len && t->nparts < TOOL_PARTS; i++) {
        if (t->text[i] == '|') {
            t->text[i] = 0;
            t->part[t->nparts++] = t->text + i + 1;
        }
    }
}

static const char *part_or(const struct tool_ui *t, int i, const char *fallback)
{
    return i < t->nparts && t->part[i][0] ? t->part[i] : fallback;
}

static void add_line(struct tool_ui *t, const char *text)
{
    if (t->ans.n >= ANS_LINES) return;
    strncpy(t->ans.line[t->ans.n], text, ANS_COLS - 1);
    t->ans.line[t->ans.n][ANS_COLS - 1] = 0;
    t->ans.n++;
}

static void tri_lines(struct tool_ui *t)
{
    static const char *const names[6] = {"a", "b", "c", "A", "B", "C"};
    int i;
    t->ans.n = 0;
    for (i = 0; i < 6; i++) {
        say_to(t->ans.line[t->ans.n++], ANS_COLS);
        say("%s = %n", names[i], i < 3 ? t->tri.s[i] : t->tri.a[i - 3]);
    }
    say_to(t->ans.line[t->ans.n++], ANS_COLS);
    say("Area = %n", tri_area(&t->tri));
}

/* a card that needs no answer from the user: it is shown, and "ok" goes back at once */
static int show(struct tool_ui *t, int kind, struct outbuf *out)
{
    t->kind = kind;
    t->open = true;
    reply(out, "ok");
    return kind;
}

int tool_open(struct tool_ui *t, const char *payload, int len, struct outbuf *out, const struct calc_status *st)
{
    const char *name;
    float v[10];
    int i, n;
    t->open = false;
    t->kind = TOOL_NONE;
    t->ans.n = 0;
    t->ans.err[0] = 0;
    t->ans.steps[0] = 0;
    t->ans.rule = "";
    split(t, payload, len);
    name = t->part[0];
    t->title = part_or(t, 1, "ChatGPT");

    if (!strcmp(name, "form")) {
        const char *labels[FORM_FIELDS];
        for (n = 0; n < FORM_FIELDS && 3 + n < t->nparts; n++) labels[n] = t->part[3 + n];
        if (n == 0) {
            reply_error(out, "a form needs at least one field");
            return TOOL_NONE;
        }
        form_init(&t->form, t->title, part_or(t, 2, "Fill what you know, Enter moves on"), labels, n);
        t->kind = TOOL_FORM;
        t->open = true;
        return TOOL_FORM;
    }
    if (!strcmp(name, "choice")) {
        for (n = 0; n < TOOL_OPTIONS && 2 + n < t->nparts; n++) t->options[n] = t->part[2 + n];
        if (n == 0) {
            reply_error(out, "a choice needs options");
            return TOOL_NONE;
        }
        t->noptions = n;
        t->sel = 0;
        t->kind = TOOL_CHOICE;
        t->open = true;
        return TOOL_CHOICE;
    }
    if (!strcmp(name, "answers")) {
        t->ans.rule = part_or(t, 2, "");
        for (i = 3; i < t->nparts; i++) add_line(t, t->part[i]);
        return show(t, TOOL_ANSWERS, out);
    }
    if (!strcmp(name, "steps")) {
        say_to(t->ans.steps, STEPS_MAX);
        for (i = 2; i < t->nparts; i++) say(i > 2 ? "\n%s" : "%s", t->part[i]);
        return show(t, TOOL_STEPS, out);
    }
    if (!strcmp(name, "triangle")) {
        for (i = 0; i < 6; i++) if (!number(t, 1 + i, &v[i], out)) return TOOL_NONE;
        for (i = 0; i < 3; i++) {
            t->tri.s[i] = v[i];
            t->tri.a[i] = v[3 + i];
        }
        t->title = "Your triangle";
        tri_lines(t);
        return show(t, TOOL_TRIANGLE, out);
    }
    if (!strcmp(name, "parabola")) {
        for (i = 0; i < 3; i++) if (!number(t, 1 + i, &v[i], out)) return TOOL_NONE;
        t->qa = v[0];
        t->qb = v[1];
        t->qc = v[2];
        t->title = "y = ax^2 + bx + c";
        if (!quad_solve(v[0], v[1], v[2], &t->quad, &t->ans)) {
            reply_error(out, t->ans.err);
            return TOOL_NONE;
        }
        return show(t, TOOL_PARABOLA, out);
    }
    if (!strcmp(name, "lines")) {
        for (i = 0; i < 4; i++) if (!number(t, 1 + i, &v[i], out)) return TOOL_NONE;
        t->la = v[0];
        t->lb = v[1];
        t->lc = v[2];
        t->ld = v[3];
        t->title = "ax + b = cx + d";
        lin_solve(v[0], v[1], v[2], v[3], &t->lkind, &t->lx, &t->ans);
        return show(t, TOOL_LINES, out);
    }
    if (!strcmp(name, "circle")) {
        if (!number(t, 1, &v[0], out)) return TOOL_NONE;
        t->title = "Unit circle";
        trig_values(v[0], &t->trig, &t->ans);
        return show(t, TOOL_CIRCLE, out);
    }
    if (!strcmp(name, "dots")) {
        for (n = 0; n < STATS_MAX && 1 + n < t->nparts; n++) if (!number(t, 1 + n, &v[n], out)) return TOOL_NONE;
        t->title = "Your numbers";
        if (!stats_solve(v, n, &t->stats, &t->ans)) {
            reply_error(out, t->ans.err);
            return TOOL_NONE;
        }
        return show(t, TOOL_DOTS, out);
    }
    if (!strcmp(name, "decimals")) {
        if (!number(t, 1, &v[0], out)) return TOOL_NONE;
        num_decimals = v[0] < 0 ? 0 : v[0] > 4 ? 4 : (int)v[0];
        reply(out, "ok");
        return TOOL_NONE;
    }
    if (!strcmp(name, "status")) {
        char buf[80];
        say_to(buf, sizeof(buf));
        say("status|time=%d:%s%d|battery=%d of 4|online=%s", st->hh, st->mm < 10 ? "0" : "", st->mm, st->battery, st->online ? "yes" : "no");
        reply(out, buf);
        return TOOL_NONE;
    }
    reply_error(out, "unknown tool");
    return TOOL_NONE;
}

static void close_card(struct tool_ui *t)
{
    t->open = false;
    t->kind = TOOL_NONE;
}

void tool_key(struct tool_ui *t, int key, struct outbuf *out)
{
    if (t->kind == TOOL_FORM) {
        int did = form_key(&t->form, key);
        if (did == FORM_BACK) {
            reply(out, "form|cancel");
            close_card(t);
        } else if (did == FORM_SOLVE) {
            char buf[256];
            int i;
            say_to(buf, sizeof(buf));
            say("form");
            for (i = 0; i < t->form.n; i++) {
                if (!t->form.f[i].len) continue;
                say("|%s=%s", t->form.f[i].label, t->form.f[i].text);
            }
            reply(out, buf);
            close_card(t);
        }
    } else if (t->kind == TOOL_CHOICE) {
        if (key == K_UP && t->sel > 0) t->sel--;
        else if (key == K_DOWN && t->sel < t->noptions - 1) t->sel++;
        else if (key == K_ENTER) {
            char buf[256];
            say_to(buf, sizeof(buf));
            say("choice|%s", t->options[t->sel]);
            reply(out, buf);
            close_card(t);
        } else if (key == K_CLEAR) {
            reply(out, "choice|cancel");
            close_card(t);
        }
    } else if (key == K_ENTER || key == K_CLEAR) {
        close_card(t);
    }
}

const char *tool_help(const struct tool_ui *t)
{
    if (t->kind == TOOL_FORM) return t->form.cur == t->form.n ? "Enter: send  Up: fields  Clear: cancel" : "0-9 . (-) type  Del erase  Enter next";
    if (t->kind == TOOL_CHOICE) return "Up/Down pick  Enter choose  Clear cancel";
    return "Enter or Clear: back to the chat";
}
