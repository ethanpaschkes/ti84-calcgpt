#ifndef HOME_H
#define HOME_H

/* CALC's home screen: four tiles, Messages / Settings / ChatGPT / Math (runs MATH), and what the keys do to them. No drawing in here, so
 * tests/calc_test.c presses the keys on a computer. Key numbers: ../../shared/keys.h */

enum { TILE_MESSAGES, TILE_SETTINGS, TILE_CHATGPT, TILE_MATH, TILE_COUNT };
enum { HOME_NONE, HOME_OPEN, HOME_QUIT };   /* what a key did */

struct tile {
    const char *title;
    const char *program;   /* the program to run, or "" for a screen inside CALC */
};

extern const struct tile HOME_TILES[TILE_COUNT];

struct home {
    int sel;   /* the chosen tile */
};

/* Left/Right (and Up/Down) move over the tiles, Enter opens the chosen one, Clear leaves CALC */
int home_key(struct home *h, int key);

#endif
