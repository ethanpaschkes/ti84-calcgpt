#ifndef LAUNCH_H
#define LAUNCH_H

#include <stdint.h>

/* How CALC starts CALCGPT or SOLVER and comes back.
 *
 * Running the C program straight with os_RunPrgm crashed (RAM cleared) on OS 5.8.5 the moment a tile was opened, 2026-10-29;
 * another project saw the same when the program being started is bigger than the caller. So CALC goes the way the calculator
 * itself supports on every OS: it writes a tiny TI-BASIC program, CALCRUN, that says `Asm(prgmNAME)`, and runs THAT with
 * os_RunPrgm. The OS parser then starts NAME exactly as the prgm menu does (on OS 5.5+ through arTIfiCE's hook), and when it
 * ends the parser finishes CALCRUN and CALC is called back. */

#define LAUNCH_PRGM "CALCRUN"          /* the one-line TI-BASIC program CALC rewrites before each launch */
#define LAUNCH_TOKENS_MAX (3 + 8)

/* the tokens of `Asm(prgmNAME)`: BB 6A (Asm(), 5F (prgm), then the name's letters and digits, which are their own tokens.
 * Returns the number of bytes written to out (LAUNCH_TOKENS_MAX at most). No drawing, tested on a computer. */
int launch_tokens(uint8_t *out, const char *program);

#endif
