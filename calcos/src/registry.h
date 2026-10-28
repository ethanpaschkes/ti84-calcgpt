#ifndef REGISTRY_H
#define REGISTRY_H

#include <stdint.h>

/* The list of apps on CalcOS's home screen: the known apps that are installed, the built-in apps, then every other program on the
 * calculator. No calculator-specific code here, so it can be tested on a computer. */

#define REG_MAX 60
#define REG_NAME 9    /* a program name is at most 8 characters, plus the end 0 */
#define REG_TITLE 9

enum { ICON_MATH, ICON_AI, ICON_WEB, ICON_SCREEN, ICON_CLOCK, ICON_NOTES, ICON_INFO, ICON_APP };
enum { BUILTIN_NONE, BUILTIN_CLOCK, BUILTIN_NOTES, BUILTIN_SYSTEM };

struct reg_app {
    char name[REG_NAME];    /* the program to run (empty for a built-in app) */
    char title[REG_TITLE];  /* what the tile says */
    uint8_t icon;
    uint8_t builtin;        /* BUILTIN_*, or 0 for a program */
};

/* does a program with exactly this name exist? */
typedef int (*reg_exists_fn)(const char *name);
/* hand out the names of all programs on the calculator one by one: returns 1 and fills `name`, or 0 when there are no more */
typedef int (*reg_next_fn)(void *state, char name[REG_NAME]);

/* builds the list into `out` (at most `max` apps) and returns how many there are */
int reg_build(struct reg_app *out, int max, reg_exists_fn exists, reg_next_fn next, void *state);

#endif
