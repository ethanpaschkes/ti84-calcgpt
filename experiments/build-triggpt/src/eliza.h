#ifndef ELIZA_H
#define ELIZA_H

/* Reply to one line of user text. The result is valid until the next call. */
const char *eliza_reply(const char *input);

#define ELIZA_WORD_LEN 16

/* Clean up a message (slang, typos, "2" as "to") and copy its words, followed
 * by topic tags, into `out`. Returns how many were written. */
int eliza_tokens(const char *input, char out[][ELIZA_WORD_LEN], int max);

#endif
