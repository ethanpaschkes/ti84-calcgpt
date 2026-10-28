#ifndef PAGEMODEL_H
#define PAGEMODEL_H

#include <stdint.h>

/* The calculator's copy of what the Mac sends (see browser/mac/browserd.py for the messages): the home page, a page of text
 * rows with link spans, and the scrolling and link selection on it. No drawing in here, so it can be tested on a computer. */

#define PM_MAX_ROWS 400
#define PM_COLS 38
#define PM_MAX_SPANS 3
#define PM_MAX_BOOKMARKS 8
#define PM_VIEW_ROWS 19                    /* rows of text that fit on the calculator screen */

enum { PM_NORMAL, PM_BIG, PM_HEADING, PM_RESULT_TITLE, PM_RESULT_URL, PM_SNIPPET, PM_NOTE };
enum { PM_HOME, PM_RESULTS, PM_PAGE, PM_MESSAGE };

struct pm_span {
    uint16_t id;
    uint8_t start, end;  /* the link covers columns start up to but not including end */
};

struct pm_row {
    uint8_t style, nspans;
    char text[PM_COLS + 1];
    struct pm_span spans[PM_MAX_SPANS];
};

struct pm {
    int screen;                        /* PM_HOME, PM_RESULTS, PM_PAGE or PM_MESSAGE */
    char title[PM_COLS + 1];
    int nrows;
    struct pm_row rows[PM_MAX_ROWS];
    int nbookmarks;
    char bookmarks[PM_MAX_BOOKMARKS][25];
    char status[PM_COLS + 1];          /* "Loading..." while a page is on its way */
    int loading;
    int top;                           /* first row on screen */
    int sel_row, sel_span;             /* the selected link: its row and which span in that row; sel_row -1 for none */
    /* the message parser */
    int state, have, want, count, which;
    uint8_t tmp[PM_COLS + 8];
    int changed;                       /* set whenever something new arrived, so the screen can be redrawn */
};

void pm_init(struct pm *pm);

/* hand over bytes as they arrive from the USB link, in any chunk sizes */
void pm_feed(struct pm *pm, const uint8_t *data, int length);

/* the link number of the selected link, or 0 */
int pm_selected_id(const struct pm *pm);

/* scroll by whole rows; `view` is how many rows fit on the screen. Keeps a link selected if one is on screen. */
void pm_scroll(struct pm *pm, int delta, int view);

/* move to the next or previous link, scrolling so it is on screen. Returns 1 if the selection moved. */
int pm_next_link(struct pm *pm, int view);
int pm_prev_link(struct pm *pm, int view);

#endif
