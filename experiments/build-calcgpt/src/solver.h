#ifndef SOLVER_H
#define SOLVER_H

#include <stdbool.h>
#include <stddef.h>

/* Guided equation solver. Say "solve boyle" (or just "solve") and it asks for
 * each value one at a time; type a number, or ? (or x) for the unknown. When
 * everything is entered it solves and shows the steps.
 *
 * solver_handle returns true if it handled the message (starting a solve, or
 * answering the question it asked) and writes the reply into `out`. */
bool solver_handle(const char *input, char *out, size_t cap);

/* True while it is waiting for the next value. */
bool solver_active(void);

/* Follow-up prompts ("a|b|c") for the last reply, or NULL for none. */
const char *solver_next(void);

/* Forget any solve in progress. */
void solver_cancel(void);

/* Which law families this app offers (defined by each app). */
extern const int app_solver_gas;
extern const int app_solver_trig;

#endif
