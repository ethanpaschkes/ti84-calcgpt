#ifndef ANSWER_H
#define ANSWER_H

/* What every problem type hands to the results screen: the answer lines ("c = 5"), the rule it used, the worked steps with the
 * numbers plugged in ('\n' between steps), or the reason the input was impossible. */

#define ANS_LINES 8
#define ANS_COLS 20
#define STEPS_MAX 640
#define ERR_MAX 120

struct answer {
    int n;                            /* answer lines */
    char line[ANS_LINES][ANS_COLS];
    const char *rule;                 /* "Law of Cosines (SSS)", "Pythagoras", ... */
    char steps[STEPS_MAX];
    char err[ERR_MAX];                /* why the input can't work (empty when it could) */
};

#endif
