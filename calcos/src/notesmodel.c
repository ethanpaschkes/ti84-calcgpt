#include "notesmodel.h"
#include <string.h>

void notes_init(struct notes *n)
{
    memset(n, 0, sizeof(*n));
    n->goal = -1;
}

void notes_set(struct notes *n, const char *data, int length)
{
    if (length > NOTES_MAX) length = NOTES_MAX;
    if (length < 0) length = 0;
    memcpy(n->text, data, (size_t)length);
    n->text[length] = 0;
    n->len = n->cur = length;
    n->goal = -1;
}

int notes_insert(struct notes *n, char c)
{
    if (n->len >= NOTES_MAX) return 0;
    memmove(n->text + n->cur + 1, n->text + n->cur, (size_t)(n->len - n->cur));
    n->text[n->cur++] = c;
    n->text[++n->len] = 0;
    n->goal = -1;
    return 1;
}

void notes_backspace(struct notes *n)
{
    if (n->cur == 0) return;
    memmove(n->text + n->cur - 1, n->text + n->cur, (size_t)(n->len - n->cur));
    n->cur--;
    n->text[--n->len] = 0;
    n->goal = -1;
}

void notes_left(struct notes *n)
{
    if (n->cur > 0) n->cur--;
    n->goal = -1;
}

void notes_right(struct notes *n)
{
    if (n->cur < n->len) n->cur++;
    n->goal = -1;
}

int notes_rows(const struct notes *n, int *starts, int max)
{
    int i, rows = 1;
    starts[0] = 0;
    for (i = 0; i < n->len && rows < max; i++) {
        if (n->text[i] == '\n') starts[rows++] = i + 1;
        else if (i + 1 - starts[rows - 1] == NOTES_COLS && i + 1 < n->len) starts[rows++] = i + 1;
    }
    return rows;
}

void notes_cursor(const struct notes *n, int *row, int *col)
{
    static int starts[NOTES_MAX_ROWS];
    int rows = notes_rows(n, starts, NOTES_MAX_ROWS), r = 0;
    while (r + 1 < rows && starts[r + 1] <= n->cur) r++;
    *row = r;
    *col = n->cur - starts[r];
}

/* how many characters row r shows (not counting its newline) */
static int row_length(const struct notes *n, const int *starts, int rows, int r)
{
    int end = r + 1 < rows ? starts[r + 1] : n->len;
    int len = end - starts[r];
    if (r + 1 < rows && len > 0 && n->text[end - 1] == '\n') len--;
    return len;
}

static void move_rows(struct notes *n, int delta)
{
    static int starts[NOTES_MAX_ROWS];
    int rows = notes_rows(n, starts, NOTES_MAX_ROWS), row, col, target, len;
    notes_cursor(n, &row, &col);
    if (n->goal < 0) n->goal = col;        /* remember the column we started from */
    target = row + delta;
    if (target < 0 || target >= rows) return;
    len = row_length(n, starts, rows, target);
    n->cur = starts[target] + (n->goal < len ? n->goal : len);
}

void notes_up(struct notes *n)
{
    move_rows(n, -1);
}

void notes_down(struct notes *n)
{
    move_rows(n, 1);
}
