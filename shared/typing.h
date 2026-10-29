#ifndef TYPING_H
#define TYPING_H

#include <stdbool.h>

/* Which character a calculator key types. Keys are numbered group * 8 + bit (see typing_tables.h). The Alpha key switches
 * to letters (the green ones) and back; the 2nd key shifts the next key only (capitals, or the symbol on the key). */

#define KEY_2ND 13    /* group 1, bit 5 */
#define KEY_ALPHA 23  /* group 2, bit 7 */

struct typing {
    bool alpha, shift;
};

enum { TY_NOTHING, TY_CHAR, TY_LAYER };  /* TY_LAYER: Alpha or 2nd was pressed, nothing is typed */

/* A key went down. For TY_CHAR the character is stored in *ch. */
int typing_key(struct typing *t, int key, char *ch);

/* A key that is not a character (Enter, Del, arrows) was used: the 2nd key only applies to one key press. */
void typing_used_other_key(struct typing *t);

#endif
