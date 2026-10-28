#ifndef KB_H
#define KB_H

#include <stdbool.h>
#include <stddef.h>

/* One built-in answer. `keys` lists the words a message must contain: words are
 * separated by spaces and "a|b" means either one. Messages are cleaned up first
 * ("r u ai?" becomes "are you ai"), so keys use the cleaned forms. `answer` may
 * hold several versions separated by '~' that rotate, and '\n' starts a new
 * line. `more` is extra detail added for the larger model. A key may hold
 * alternative phrases separated by '/', e.g. "stp/standard temperature", and
 * may start with "N:" to add N to its score. */
struct entry {
    const char *keys;
    const char *answer;
    const char *more;
};

/* Suggested follow-up questions for an entry: `keys` must equal that entry's
 * keys string, `next` is up to three prompts separated by '|'. */
struct kb_follow {
    const char *keys;
    const char *next;
};

/* A table of entries ending with an all-zero entry, and optionally its
 * follow-ups (also ending with an all-zero entry). */
struct kb_table {
    const struct entry *entries;
    const struct kb_follow *follow;
};

/* Each app defines these (app_*.c). Later tables win ties, so put the subject
 * table after the general one. */
extern const struct kb_table app_tables[];
extern const int app_table_count;
extern const char *const app_chips[];
extern const int app_chip_count;
/* Follow-ups used when an answer has none of its own. */
extern const char *const app_default_next;

extern const struct entry kb_general[];
extern const int kb_general_n;
extern const struct kb_follow kb_gas_follow[];
extern const struct kb_follow kb_trig_follow[];
extern const struct entry kb_gas[];
extern const int kb_gas_n;
extern const struct entry kb_trig[];
extern const int kb_trig_n;

/* Look up a short question in the built-in answers. With `detailed`, entries
 * that have a longer explanation add it. False if nothing matches. */
bool kb_answer(const char *input, bool detailed, char *out, size_t cap);

/* Follow-up prompts ("a|b|c") for the answer kb_answer last returned, or NULL
 * if that answer has none. */
const char *kb_next(void);

/* Shown when nothing matches. */
const char *kb_fallback(void);

#endif
