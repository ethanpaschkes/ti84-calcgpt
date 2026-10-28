#ifndef CHATMODEL_H
#define CHATMODEL_H

#include <stdint.h>

/* The calculator's copy of a chat: what you asked, what the Mac's model answered (arriving in pieces), word-wrapped into rows,
 * with scrolling. No drawing in here, so it can be tested on a computer.
 *
 * Messages, Mac to calculator (every length is one byte):
 *   'A' len text   more of the assistant's answer         'D'  the answer is complete
 *   'T' len text   a status such as "Thinking..." (length 0 clears it)        'C'  clear the conversation
 *   'K'            the list of AI models is about to be sent again          'L' len text   one model's name, added to the list
 *   'U' index      which model in the list is the one in use (index is one byte, not text) */

#define CHAT_BUF 6000
#define CHAT_COLS 38
#define CHAT_USER_COLS 36   /* user rows are drawn after a two-character "> " margin */
#define CHAT_MAX_ROWS 500
#define CHAT_VIEW_ROWS 18
#define CHAT_INPUT_MAX 200
#define CHAT_MODELS 8
#define CHAT_MODEL_LABEL 20

enum { ROLE_USER = 'U', ROLE_AI = 'A' };

struct chat_row {
    uint16_t start;   /* where the row's text begins in buf */
    uint8_t len;
    uint8_t role;
    uint8_t first;    /* the first row of a message */
};

struct chat {
    char buf[CHAT_BUF];            /* messages one after another: role letter, text, then a 0 byte */
    int len;                       /* where the last message's text ends */
    char status[CHAT_COLS + 1];
    int busy;                      /* a question was sent and the answer is not complete yet */
    struct chat_row rows[CHAT_MAX_ROWS];
    int nrows;
    int top;                       /* first row on screen */
    int follow;                    /* keep the view at the newest text */
    int changed;                   /* something new arrived, so the screen should be redrawn */
    int state, want, have;         /* the message parser */
    int tag;
    char models[CHAT_MODELS][CHAT_MODEL_LABEL + 1];   /* the AI models the Mac offers, by name */
    int nmodels;
    int model_sel;                 /* the one in use */
};

void chat_init(struct chat *c);
void chat_clear(struct chat *c);

/* you sent a question: adds it to the chat, marks the chat busy and follows the new text */
void chat_user(struct chat *c, const char *text);

/* hand over bytes as they arrive from the USB link, in any chunk sizes */
void chat_feed(struct chat *c, const uint8_t *data, int length);

/* scroll by whole rows; scrolling up stops following the newest text, scrolling to the end resumes it */
void chat_scroll(struct chat *c, int delta, int view);

/* work out the rows (call after anything changed, with how many rows fit on screen) */
void chat_layout(struct chat *c, int view);

#endif
