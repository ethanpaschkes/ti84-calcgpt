#include "pagemodel.h"
#include <string.h>

enum {
    S_CMD, S_H_COUNT, S_H_LEN, S_H_TEXT,
    S_B_KIND, S_B_LEN, S_B_TEXT,
    S_W_STYLE, S_W_NSPANS, S_W_LEN, S_W_TEXT, S_W_SPAN,
    S_T_LEN, S_T_TEXT
};

void pm_init(struct pm *pm)
{
    memset(pm, 0, sizeof(*pm));
    pm->sel_row = -1;
    pm->screen = PM_HOME;
}

/* choose the first link that is on screen, if any */
static void select_first_visible(struct pm *pm, int view)
{
    int r;
    pm->sel_row = -1;
    pm->sel_span = 0;
    for (r = pm->top; r < pm->nrows && r < pm->top + view; r++) {
        if (pm->rows[r].nspans) {
            pm->sel_row = r;
            return;
        }
    }
}

int pm_selected_id(const struct pm *pm)
{
    if (pm->sel_row < 0 || pm->sel_row >= pm->nrows || pm->sel_span >= pm->rows[pm->sel_row].nspans) return 0;
    return pm->rows[pm->sel_row].spans[pm->sel_span].id;
}

static void finish_row(struct pm *pm)
{
    if (pm->nrows < PM_MAX_ROWS) pm->nrows++;
}

static void byte_in(struct pm *pm, uint8_t b)
{
    struct pm_row *row = &pm->rows[pm->nrows < PM_MAX_ROWS ? pm->nrows : PM_MAX_ROWS - 1];
    int writable = pm->nrows < PM_MAX_ROWS;
    switch (pm->state) {
    case S_CMD:
        switch (b) {
        case 'H': pm->state = S_H_COUNT; break;
        case 'B': pm->state = S_B_KIND; break;
        case 'W': pm->state = S_W_STYLE; break;
        case 'T': pm->state = S_T_LEN; break;
        case 'E':
            pm->loading = 0;
            pm->status[0] = 0;
            pm->top = 0;
            select_first_visible(pm, PM_VIEW_ROWS);
            pm->changed = 1;
            break;
        default: break;
        }
        break;
    /* home page: a count, then that many names */
    case S_H_COUNT:
        pm->screen = PM_HOME;
        pm->nrows = 0;
        pm->nbookmarks = 0;
        pm->count = b > PM_MAX_BOOKMARKS ? PM_MAX_BOOKMARKS : b;
        pm->loading = 0;
        pm->status[0] = 0;
        pm->sel_row = -1;
        pm->changed = 1;
        pm->state = pm->count ? S_H_LEN : S_CMD;
        break;
    case S_H_LEN:
        pm->want = b;
        pm->have = 0;
        pm->state = b ? S_H_TEXT : S_CMD;
        if (!b) {
            pm->bookmarks[pm->nbookmarks][0] = 0;
            if (++pm->nbookmarks < pm->count) pm->state = S_H_LEN;
        }
        break;
    case S_H_TEXT:
        if (pm->have < 24) pm->bookmarks[pm->nbookmarks][pm->have] = (char)b;
        pm->have++;
        if (pm->have == pm->want) {
            pm->bookmarks[pm->nbookmarks][pm->have < 24 ? pm->have : 24] = 0;
            pm->nbookmarks++;
            pm->state = pm->nbookmarks < pm->count ? S_H_LEN : S_CMD;
        }
        break;
    /* a page starts: kind, then the title */
    case S_B_KIND:
        pm->screen = b == 'R' ? PM_RESULTS : b == 'M' ? PM_MESSAGE : PM_PAGE;
        pm->nrows = 0;
        pm->title[0] = 0;
        pm->sel_row = -1;
        pm->top = 0;
        pm->loading = 1;
        pm->state = S_B_LEN;
        break;
    case S_B_LEN:
        pm->want = b;
        pm->have = 0;
        pm->state = b ? S_B_TEXT : S_CMD;
        break;
    case S_B_TEXT:
        if (pm->have < PM_COLS) pm->title[pm->have] = (char)b;
        pm->have++;
        if (pm->have == pm->want) {
            pm->title[pm->have < PM_COLS ? pm->have : PM_COLS] = 0;
            pm->state = S_CMD;
        }
        break;
    /* a row: style, number of link spans, text length, text, then the spans */
    case S_W_STYLE:
        if (writable) {
            memset(row, 0, sizeof(*row));
            row->style = b;
        }
        pm->state = S_W_NSPANS;
        break;
    case S_W_NSPANS:
        pm->count = b;
        pm->state = S_W_LEN;
        break;
    case S_W_LEN:
        pm->want = b;
        pm->have = 0;
        pm->which = 0;
        if (b) {
            pm->state = S_W_TEXT;
        } else if (pm->count) {
            pm->state = S_W_SPAN;
            pm->have = 0;
        } else {
            if (writable) finish_row(pm);
            pm->state = S_CMD;
        }
        break;
    case S_W_TEXT:
        if (writable && pm->have < PM_COLS) row->text[pm->have] = (char)b;
        pm->have++;
        if (pm->have == pm->want) {
            if (writable) row->text[pm->have < PM_COLS ? pm->have : PM_COLS] = 0;
            pm->have = 0;
            if (pm->count) {
                pm->state = S_W_SPAN;
            } else {
                if (writable) finish_row(pm);
                pm->state = S_CMD;
            }
        }
        break;
    case S_W_SPAN:
        pm->tmp[pm->have++] = b;
        if (pm->have == 4) {
            if (writable && pm->which < PM_MAX_SPANS) {
                row->spans[pm->which].id = (uint16_t)(pm->tmp[0] | (pm->tmp[1] << 8));
                row->spans[pm->which].start = pm->tmp[2];
                row->spans[pm->which].end = pm->tmp[3];
                row->nspans = (uint8_t)(pm->which + 1);
            }
            pm->which++;
            pm->have = 0;
            if (pm->which == pm->count) {
                if (writable) finish_row(pm);
                pm->state = S_CMD;
            }
        }
        break;
    /* a status line */
    case S_T_LEN:
        pm->want = b;
        pm->have = 0;
        pm->loading = 1;
        pm->status[0] = 0;
        pm->changed = 1;
        pm->state = b ? S_T_TEXT : S_CMD;
        break;
    case S_T_TEXT:
        if (pm->have < PM_COLS) pm->status[pm->have] = (char)b;
        pm->have++;
        if (pm->have == pm->want) {
            pm->status[pm->have < PM_COLS ? pm->have : PM_COLS] = 0;
            pm->state = S_CMD;
        }
        break;
    default:
        pm->state = S_CMD;
        break;
    }
}

void pm_feed(struct pm *pm, const uint8_t *data, int length)
{
    int i;
    for (i = 0; i < length; i++) byte_in(pm, data[i]);
}

/* ---- scrolling and links ---- */

/* is span `sp` of row `r` part of the same link as the one at (r0, s0)? Used to skip along a link that runs over several rows. */
static int same_link(const struct pm *pm, int r, int sp, int id)
{
    return pm->rows[r].spans[sp].id == id;
}

/* the position that comes after (r, s) in reading order, or r = nrows when there is none */
static void step_forward(const struct pm *pm, int *r, int *s)
{
    (*s)++;
    while (*r < pm->nrows && *s >= pm->rows[*r].nspans) {
        (*r)++;
        *s = 0;
    }
}

static void step_back(const struct pm *pm, int *r, int *s)
{
    (*s)--;
    while (*r >= 0 && *s < 0) {
        (*r)--;
        *s = *r >= 0 ? pm->rows[*r].nspans - 1 : 0;
    }
}

static void show_row(struct pm *pm, int row, int view)
{
    if (row < pm->top) pm->top = row;
    else if (row >= pm->top + view) pm->top = row - view + 1;
}

int pm_next_link(struct pm *pm, int view)
{
    int r, s, id;
    if (pm->sel_row < 0) {
        select_first_visible(pm, view);
        return pm->sel_row >= 0;
    }
    id = pm_selected_id(pm);
    r = pm->sel_row;
    s = pm->sel_span;
    do {
        step_forward(pm, &r, &s);
    } while (r < pm->nrows && same_link(pm, r, s, id));
    if (r >= pm->nrows) return 0;
    pm->sel_row = r;
    pm->sel_span = s;
    show_row(pm, r, view);
    return 1;
}

int pm_prev_link(struct pm *pm, int view)
{
    int r, s, id, pr, ps;
    if (pm->sel_row < 0) return 0;
    id = pm_selected_id(pm);
    r = pm->sel_row;
    s = pm->sel_span;
    do {
        step_back(pm, &r, &s);
    } while (r >= 0 && same_link(pm, r, s, id));
    if (r < 0) return 0;
    /* r, s is the last span of the previous link; go back to where that link begins */
    id = pm->rows[r].spans[s].id;
    pr = r;
    ps = s;
    for (;;) {
        step_back(pm, &pr, &ps);
        if (pr < 0 || pm->rows[pr].spans[ps].id != id) break;
        r = pr;
        s = ps;
    }
    pm->sel_row = r;
    pm->sel_span = s;
    show_row(pm, r, view);
    return 1;
}

void pm_scroll(struct pm *pm, int delta, int view)
{
    int max_top = pm->nrows > view ? pm->nrows - view : 0;
    pm->top += delta;
    if (pm->top < 0) pm->top = 0;
    if (pm->top > max_top) pm->top = max_top;
    /* if the selected link scrolled out of sight, take the first link that is on screen instead */
    if (pm->sel_row < pm->top || pm->sel_row >= pm->top + view) select_first_visible(pm, view);
}
