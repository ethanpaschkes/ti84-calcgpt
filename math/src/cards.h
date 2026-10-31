#ifndef CARDS_H
#define CARDS_H

#include <stdbool.h>
#include "form.h"
#include "probs.h"
#include "tri.h"

/* The Math look, drawn with graphx (calculator only): the dark gradient wallpaper, the status bar, white cards with a form, a
 * choice list, answer lines with a diagram, or worked steps, and the diagrams themselves (a to-scale triangle, a parabola, two
 * lines, the unit circle, a dot plot). Used by MATH's screens and by CALCGPT when ChatGPT asks the calculator to show something.
 * Call cards_palette() first (CALCGPT switches back to its own palette afterwards). */

#define CARD_X 8
#define CARD_Y 28
#define CARD_W 304
#define CARD_H 180

void cards_palette(void);
/* the wallpaper and the status bar (title, time, Online/Offline, battery) */
void cards_background(const char *title, bool online);
/* the help line at the bottom */
void cards_help(const char *text);
/* a red strip at the bottom with a reason in it (replaces the help line) */
void cards_error(const char *text);
/* the white card the pieces below draw on */
void cards_card(void);

/* a form popup; cur is the chosen row (fm->n = the Solve row); button is the text of that row */
void card_form(const struct form *fm, const char *button);
/* a title and a list of options with the chosen one highlighted; subtitle may be NULL */
void card_choice(const char *title, const char *subtitle, const char *const *options, int n, int sel);
/* "Answers are:" lines on the left, the rule at the bottom; the caller draws the diagram on the right afterwards */
void card_answers(const char *title, const char *const *lines, int n, const char *rule, int highlight);
/* a title and wrapped text */
void card_steps(const char *title, const char *text);

/* the diagrams, drawn on the right part of an answers card */
void diagram_triangle(const struct tri_sol *t);
void diagram_quadratic(float a, float b, float c, const struct quad_out *q);
void diagram_lines(float a, float b, float c, float d, int kind, float x);
void diagram_unit_circle(const struct trig_out *t);
void diagram_dots(const struct stats_out *s);

#endif
