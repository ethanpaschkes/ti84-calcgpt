#ifndef SCRIPT_H
#define SCRIPT_H

/* Everything the offline CalcGPT demo "says": scripted replies to typed questions, and the three pretend photos with what the
 * "model" reads from them and its (correct) step-by-step answers. Nothing here is generated; it is all written text.
 * No calculator-specific code, so it can be tested on a computer. */

struct photo_script {
    const char *caption;   /* what the handwriting says, as drawn on the pretend photo */
    const char *ocr;       /* what the pretend model reports having read */
    const char *answer;    /* its worked answer */
    const char *taken;     /* the time label in the photo gallery */
};

#define N_PHOTOS 3
extern const struct photo_script PHOTOS[N_PHOTOS];

/* the scripted answer to a typed question (never NULL) */
const char *script_reply(const char *question);

/* word-wraps text to `width` columns. Newlines start a new line. Fills starts[] and lens[] (at most `max`) and returns the count. */
int wrap_text(const char *text, int width, int *starts, int *lens, int max);

#endif
