#ifndef NOTESMODEL_H
#define NOTESMODEL_H

/* A small text editor's contents and cursor, wrapped to the screen width. No drawing here, so it can be tested on a computer. */

#define NOTES_MAX 1500
#define NOTES_COLS 36
#define NOTES_MAX_ROWS 1500

struct notes {
    char text[NOTES_MAX + 1];
    int len, cur;
    int goal;   /* the column to aim for while moving up and down, -1 when not set */
};

void notes_init(struct notes *n);
void notes_set(struct notes *n, const char *data, int length);   /* replaces the text; the cursor goes to the end */
int notes_insert(struct notes *n, char c);                       /* returns 1 if there was room */
void notes_backspace(struct notes *n);
void notes_left(struct notes *n);
void notes_right(struct notes *n);
void notes_up(struct notes *n);
void notes_down(struct notes *n);

/* the rows as the screen shows them: starts[i] is where row i begins in the text. A row ends at a newline or after NOTES_COLS
 * characters. Returns how many rows there are (at least 1). */
int notes_rows(const struct notes *n, int *starts, int max);

/* which row and column the cursor is at */
void notes_cursor(const struct notes *n, int *row, int *col);

#endif
