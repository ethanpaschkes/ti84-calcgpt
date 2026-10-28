#ifndef PRESET_H
#define PRESET_H

#include <stdbool.h>
#include <stddef.h>

/* Guided problem types: each one asks for a few values and writes a worked answer in plain text.
 * No drawing in here, so it can be tested on a computer. */

#define PRESET_COUNT 10
#define PRESET_FIELDS 8
#define FIELD_LEN 15

struct preset_field {
    const char *label;
    const char *unit;                 /* shown after the box, may be "" */
    const char *const *choices;       /* NULL for a typed value, else the options Left/Right step through */
    int nchoices;
};

struct preset {
    const char *name;
    const char *hint;                 /* one line under the title */
    int nfields;
    struct preset_field field[PRESET_FIELDS];
    /* f[i] is the typed text, or the chosen option's text for a choice field.
     * Writes the answer into out; returns false (with the reason in out) if the inputs can not work. */
    bool (*run)(const char *const *f, char *out, size_t cap);
};

extern const struct preset presets[PRESET_COUNT];

#endif
