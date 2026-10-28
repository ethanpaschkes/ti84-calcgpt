/* Answer matching for CalcGPT: finds the entry whose key words best fit the
 * message. See kb.h for the entry format. */
#include "kb.h"
#include "eliza.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENTRIES 400

static uint8_t rot[MAX_ENTRIES];
static unsigned fallback_next;
static const char *last_next;

const char *kb_next(void)
{
    return last_next;
}

/* Score for one key set: the number of key groups that matched (0 if any group
 * is missing). */
static int score_set(const char *k, const char *end, char words[][ELIZA_WORD_LEN], int nwords)
{
    int groups = 0;

    while (k < end) {
        bool found = false;
        char alt[ELIZA_WORD_LEN];
        int n = 0;
        for (;; k++) {
            if (k >= end || *k == '|' || *k == ' ') {
                int w;
                alt[n] = 0;
                for (w = 0; w < nwords; w++) {
                    if (!strcmp(words[w], alt)) {
                        found = true;
                    }
                }
                n = 0;
                if (k >= end || *k != '|') {
                    break;
                }
            } else if (n < ELIZA_WORD_LEN - 1) {
                alt[n++] = *k;
            }
        }
        if (!found) {
            return 0;
        }
        groups++;
        if (k < end && *k == ' ') {
            k++;
        }
    }
    return groups;
}

/* Score for an entry: the best of its key sets (separated by '/'), plus an
 * optional bonus written as a leading "N:", so a specific entry can beat a
 * broader one that matches the same words. */
static int score(const char *keys, char words[][ELIZA_WORD_LEN], int nwords)
{
    int bonus = 0, best = 0;
    const char *k = keys;

    if (k[0] >= '0' && k[0] <= '9' && k[1] == ':') {
        bonus = k[0] - '0';
        k += 2;
    }
    for (;;) {
        const char *e = k;
        int s;
        while (*e && *e != '/') {
            e++;
        }
        s = score_set(k, e, words, nwords);
        if (s > best) {
            best = s;
        }
        if (!*e) {
            break;
        }
        k = e + 1;
    }
    return best ? best + bonus : 0;
}

static void replace_special(char *s, size_t cap)
{
    if (!strcmp(s, "@coin")) {
        strncpy(s, (rand() & 1) ? "Heads." : "Tails.", cap - 1);
    } else if (!strcmp(s, "@die")) {
        s[0] = 'Y'; strcpy(s + 1, "ou rolled a 0.");
        s[13] = (char)('1' + rand() % 6);
    } else if (!strcmp(s, "@num")) {
        int v = 1 + rand() % 100;
        strcpy(s, "How about ");
        if (v >= 100) {
            strcat(s, "100.");
        } else if (v >= 10) {
            s[10] = (char)('0' + v / 10);
            s[11] = (char)('0' + v % 10);
            s[12] = '.';
            s[13] = 0;
        } else {
            s[10] = (char)('0' + v);
            s[11] = '.';
            s[12] = 0;
        }
    }
}

bool kb_answer(const char *input, bool detailed, char *out, size_t cap)
{
    char words[40][ELIZA_WORD_LEN];
    int nwords = eliza_tokens(input, words, 40);
    int best = 0, best_slot = -1, best_t = -1, slot = 0;
    const struct entry *best_e = NULL;
    const char *ans;
    char *tilde;
    int alts = 1, pick, t, i;
    const char *p;

    for (t = 0; t < app_table_count; t++) {
        for (i = 0; app_tables[t].entries[i].keys && slot < MAX_ENTRIES; i++, slot++) {
            int s = score(app_tables[t].entries[i].keys, words, nwords);
            if (s > 0 && s >= best) {
                best = s;
                best_slot = slot;
                best_t = t;
                best_e = &app_tables[t].entries[i];
            }
        }
    }
    last_next = NULL;
    if (!best_e) {
        return false;
    }
    if (app_tables[best_t].follow) {
        const struct kb_follow *f;
        for (f = app_tables[best_t].follow; f->keys; f++) {
            if (!strcmp(f->keys, best_e->keys)) {
                last_next = f->next;
                break;
            }
        }
    }

    ans = best_e->answer;
    for (p = ans; *p; p++) {
        if (*p == '~') {
            alts++;
        }
    }
    pick = rot[best_slot]++ % alts;
    for (p = ans; pick > 0; p++) {
        if (*p == '~') {
            pick--;
        }
    }
    strncpy(out, p, cap - 1);
    out[cap - 1] = 0;
    tilde = strchr(out, '~');
    if (tilde) {
        *tilde = 0;
    }
    if (detailed && best_e->more[0] && strlen(out) + strlen(best_e->more) + 2 < cap) {
        strcat(out, " ");
        strcat(out, best_e->more);
    }
    replace_special(out, cap);
    return true;
}

const char *kb_fallback(void)
{
    static const char *const f[] = {
        "I'm a small program, so I might not know that one. Try 'help' to see what I can do.",
        "I don't have an answer for that yet. Ask me about trig, flying, or try some math.",
        "Hmm, I'm not sure. Try a shorter question, like 'what is sine'.",
        "I can't answer that on this calculator. Try 'joke', 'fun fact' or '12*3'.",
    };
    return f[fallback_next++ % (sizeof(f) / sizeof(f[0]))];
}
