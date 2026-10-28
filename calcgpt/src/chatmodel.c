#include "chatmodel.h"
#include <string.h>

enum { S_CMD, S_A_LEN, S_A_TEXT, S_T_LEN, S_T_TEXT, S_L_LEN, S_L_TEXT, S_U_INDEX };

void chat_clear(struct chat *c)
{
    c->len = 0;
    c->buf[0] = 0;
    c->nrows = 0;
    c->top = 0;
    c->follow = 1;
    c->busy = 0;
    c->changed = 1;
}

void chat_init(struct chat *c)
{
    memset(c, 0, sizeof(*c));
    c->follow = 1;
}

/* drop the oldest message, if there is a newer one; returns 1 if it did */
static int drop_oldest(struct chat *c)
{
    int end = 1;
    while (end < c->len && c->buf[end] != 0) end++;
    if (end >= c->len) return 0;
    memmove(c->buf, c->buf + end + 1, (size_t)(c->len - end - 1));
    c->len -= end + 1;
    c->buf[c->len] = 0;
    return 1;
}

/* make space for n more bytes by forgetting the oldest messages */
static void make_room(struct chat *c, int n)
{
    while (c->len + n + 2 >= CHAT_BUF && drop_oldest(c)) {
    }
}

static void start_message(struct chat *c, char role)
{
    make_room(c, 2);
    if (c->len > 0) c->len++;   /* step over the 0 that ends the message before */
    c->buf[c->len++] = role;
    c->buf[c->len] = 0;
}

static void append_text(struct chat *c, const char *text, int n)
{
    int room;
    make_room(c, n);
    room = CHAT_BUF - c->len - 2;
    if (n > room) n = room > 0 ? room : 0;   /* one message longer than the whole buffer is cut */
    memcpy(c->buf + c->len, text, (size_t)n);
    c->len += n;
    c->buf[c->len] = 0;
}

void chat_user(struct chat *c, const char *text)
{
    start_message(c, ROLE_USER);
    append_text(c, text, (int)strlen(text));
    c->busy = 1;
    c->status[0] = 0;
    c->follow = 1;
    c->changed = 1;
}

static int last_role(const struct chat *c)
{
    int i = c->len - 1;
    while (i > 0 && c->buf[i - 1] != 0) i--;
    return c->len > 0 ? c->buf[i] : 0;
}

static void byte_in(struct chat *c, uint8_t b)
{
    switch (c->state) {
    case S_CMD:
        switch (b) {
        case 'A': c->state = S_A_LEN; break;
        case 'T': c->state = S_T_LEN; break;
        case 'L': c->state = S_L_LEN; break;
        case 'U': c->state = S_U_INDEX; break;
        case 'K':
            c->nmodels = 0;
            c->model_sel = 0;
            c->changed = 1;
            break;
        case 'D':
            c->busy = 0;
            c->status[0] = 0;
            c->changed = 1;
            break;
        case 'C':
            chat_clear(c);
            c->status[0] = 0;
            break;
        default: break;
        }
        break;
    case S_A_LEN:
        c->want = b;
        c->have = 0;
        c->state = b ? S_A_TEXT : S_CMD;
        if (b && last_role(c) != ROLE_AI) start_message(c, ROLE_AI);
        break;
    case S_A_TEXT: {
        char ch = (char)b;
        append_text(c, &ch, 1);
        if (++c->have == c->want) {
            c->state = S_CMD;
            c->changed = 1;
        }
        break;
    }
    case S_T_LEN:
        c->want = b;
        c->have = 0;
        c->status[0] = 0;
        c->changed = 1;
        c->state = b ? S_T_TEXT : S_CMD;
        break;
    case S_T_TEXT:
        if (c->have < CHAT_COLS) c->status[c->have] = (char)b;
        c->have++;
        if (c->have == c->want) {
            c->status[c->have < CHAT_COLS ? c->have : CHAT_COLS] = 0;
            c->state = S_CMD;
        }
        break;
    case S_L_LEN:
        c->want = b;
        c->have = 0;
        c->state = b ? S_L_TEXT : S_CMD;
        if (c->nmodels < CHAT_MODELS) c->models[c->nmodels][0] = 0;
        break;
    case S_L_TEXT:
        if (c->nmodels < CHAT_MODELS && c->have < CHAT_MODEL_LABEL) {
            c->models[c->nmodels][c->have] = (char)b;
            c->models[c->nmodels][c->have + 1] = 0;
        }
        if (++c->have == c->want) {
            if (c->nmodels < CHAT_MODELS) c->nmodels++;
            c->changed = 1;
            c->state = S_CMD;
        }
        break;
    case S_U_INDEX:
        c->model_sel = b < CHAT_MODELS ? b : 0;
        c->changed = 1;
        c->state = S_CMD;
        break;
    default:
        c->state = S_CMD;
        break;
    }
}

void chat_feed(struct chat *c, const uint8_t *data, int length)
{
    int i;
    for (i = 0; i < length; i++) byte_in(c, data[i]);
}

static void add_row(struct chat *c, int start, int len, int role, int first)
{
    if (c->nrows == CHAT_MAX_ROWS) {
        memmove(c->rows, c->rows + 1, sizeof(c->rows[0]) * (CHAT_MAX_ROWS - 1));
        c->nrows--;
    }
    c->rows[c->nrows].start = (uint16_t)start;
    c->rows[c->nrows].len = (uint8_t)len;
    c->rows[c->nrows].role = (uint8_t)role;
    c->rows[c->nrows].first = (uint8_t)first;
    c->nrows++;
}

/* wrap one paragraph (text from a to b, no newlines) into rows */
static void wrap_paragraph(struct chat *c, int a, int b, int role, int width, int *first)
{
    int i = a, row_start = -1, row_len = 0;
    if (a == b) {
        add_row(c, a, 0, role, *first);
        *first = 0;
        return;
    }
    while (i < b) {
        int ws, we, wl;
        while (i < b && c->buf[i] == ' ') i++;
        if (i >= b) break;
        ws = i;
        while (i < b && c->buf[i] != ' ') i++;
        we = i;
        wl = we - ws;
        while (wl > width) {                       /* a word longer than a row is cut */
            if (row_len) { add_row(c, row_start, row_len, role, *first); *first = 0; row_len = 0; row_start = -1; }
            add_row(c, ws, width, role, *first);
            *first = 0;
            ws += width;
            wl -= width;
        }
        if (row_len == 0) {
            row_start = ws;
            row_len = wl;
        } else if (we - row_start <= width) {
            row_len = we - row_start;   /* a row is a slice of the text, so spaces inside it stay as they were written */
        } else {
            add_row(c, row_start, row_len, role, *first);
            *first = 0;
            row_start = ws;
            row_len = wl;
        }
    }
    if (row_len) {
        add_row(c, row_start, row_len, role, *first);
        *first = 0;
    }
}

void chat_layout(struct chat *c, int view)
{
    int i = 0, max_top;
    c->nrows = 0;
    while (i < c->len) {
        int role = c->buf[i], end = i + 1, para = i + 1, first = 1, width = role == ROLE_USER ? CHAT_USER_COLS : CHAT_COLS;
        while (end < c->len && c->buf[end] != 0) end++;
        for (;;) {
            int nl = para;
            while (nl < end && c->buf[nl] != '\n') nl++;
            wrap_paragraph(c, para, nl, role, width, &first);
            if (nl >= end) break;
            para = nl + 1;
        }
        i = end + 1;
    }
    max_top = c->nrows > view ? c->nrows - view : 0;
    if (c->follow || c->top > max_top) c->top = max_top;
    if (c->top < 0) c->top = 0;
}

void chat_scroll(struct chat *c, int delta, int view)
{
    int max_top = c->nrows > view ? c->nrows - view : 0;
    c->top += delta;
    if (c->top < 0) c->top = 0;
    if (c->top > max_top) c->top = max_top;
    c->follow = c->top == max_top;
}
