#ifndef CALC_H
#define CALC_H

#include <stdbool.h>
#include <stddef.h>

/* If `input` is arithmetic ("2+2", "what is twenty times 3"), write the answer
 * into `result` and return true. Otherwise return false and leave it alone. */
bool calc_try(const char *input, char *result, size_t cap);

#endif
