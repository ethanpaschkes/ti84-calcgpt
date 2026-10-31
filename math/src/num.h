#ifndef NUM_H
#define NUM_H

#include <stdbool.h>

/* Numbers as text for the Math app: rounding to the chosen number of decimals, reading what was typed, a tiny printf for the
 * worked steps, and word wrapping. No drawing, no floating-point printf (the calculator's is big and slow). Tested on a computer. */

#define NUM_CHARS 16   /* room for one formatted number, with its 0 byte */
#define WRAP_W 40      /* longest wrapped line, with its 0 byte */

/* how many decimals the answers show (the Decimals row on the home screen; 0 to 4) */
extern int num_decimals;

/* v rounded to dec decimals, trailing zeros dropped ("5", "3.14", "-0.5"); "undefined" for NaN, "huge" past 4e9 */
void fmt_num(float v, int dec, char *out);

/* reads "12", "-3.5", ".5"; false for anything else (empty, "-", "1.2.3", letters) */
bool parse_num(const char *s, float *v);

/* say() writes into this buffer from now on (it is cut off, never overrun) */
void say_to(char *buf, int cap);
/* %s a string, %n a float with num_decimals, %d an int, %% a percent sign */
void say(const char *fmt, ...);

/* splits text on '\n' and wraps long lines at spaces, at most cols characters each; returns how many lines */
int wrap_text(const char *text, int cols, char out[][WRAP_W], int max_lines);

#endif
