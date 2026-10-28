#include "registry.h"
#include <string.h>

struct known {
    const char *program, *title;
    uint8_t icon;
};

/* the programs of this project, in the order they appear */
static const struct known KNOWN[] = {
    {"SOLVER", "Math", ICON_MATH},
    {"CALCGPT", "AI Chat", ICON_AI},
    {"GPTDEMO", "AI Demo", ICON_AI},
    {"BROWSER", "Web", ICON_WEB},
    {"REELS", "Screen", ICON_SCREEN},
};
#define N_KNOWN ((int)(sizeof(KNOWN) / sizeof(KNOWN[0])))

static const struct { const char *title; uint8_t icon, builtin; } BUILTINS[] = {
    {"Clock", ICON_CLOCK, BUILTIN_CLOCK},
    {"Notes", ICON_NOTES, BUILTIN_NOTES},
    {"System", ICON_INFO, BUILTIN_SYSTEM},
};
#define N_BUILTIN ((int)(sizeof(BUILTINS) / sizeof(BUILTINS[0])))

static int add(struct reg_app *out, int n, int max, const char *name, const char *title, int icon, int builtin)
{
    if (n >= max) return n;
    memset(&out[n], 0, sizeof(out[n]));
    strncpy(out[n].name, name, REG_NAME - 1);
    strncpy(out[n].title, title, REG_TITLE - 1);
    out[n].icon = (uint8_t)icon;
    out[n].builtin = (uint8_t)builtin;
    return n + 1;
}

/* is this the name of an app that is already shown, or one that must not be shown? */
static int hidden(const char *name, const struct reg_app *list, int n)
{
    int i;
    if (!strcmp(name, "CALCOS")) return 1;   /* this shell itself */
    if (name[0] == 0 || (unsigned char)name[0] < 'A') return 1;  /* names that start with a symbol are the system's hidden ones */
    for (i = 0; i < N_KNOWN; i++) if (!strcmp(name, KNOWN[i].program)) return 1;
    for (i = 0; i < n; i++) if (!strcmp(name, list[i].name)) return 1;
    return 0;
}

int reg_build(struct reg_app *out, int max, reg_exists_fn exists, reg_next_fn next, void *state)
{
    int n = 0, i, first_other, j;
    char name[REG_NAME];
    for (i = 0; i < N_KNOWN; i++) if (exists(KNOWN[i].program)) n = add(out, n, max, KNOWN[i].program, KNOWN[i].title, KNOWN[i].icon, 0);
    for (i = 0; i < N_BUILTIN; i++) n = add(out, n, max, "", BUILTINS[i].title, BUILTINS[i].icon, BUILTINS[i].builtin);
    first_other = n;
    while (n < max && next(state, name)) {
        name[REG_NAME - 1] = 0;
        if (!hidden(name, out, n)) n = add(out, n, max, name, name, ICON_APP, 0);
    }
    /* the other programs in alphabetical order (a small insertion sort: there are few of them) */
    for (i = first_other + 1; i < n; i++) {
        struct reg_app t = out[i];
        for (j = i - 1; j >= first_other && strcmp(out[j].name, t.name) > 0; j--) out[j + 1] = out[j];
        out[j + 1] = t;
    }
    return n;
}
