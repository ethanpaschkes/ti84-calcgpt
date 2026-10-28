/* Tests for the pure parts of CalcOS: the app list, time and date text, and the notes editor. */
#include <stdio.h>
#include <string.h>
#include "../src/notesmodel.h"
#include "../src/registry.h"
#include "../src/timefmt.h"

static int fails;
#define CHECK(what, got, want) do { int g_ = (got), w_ = (want); if (g_ != w_) { printf("FAIL %s: got %d want %d\n", what, g_, w_); fails++; } } while (0)
#define CHECKS(what, got, want) do { if (strcmp((got), (want)) != 0) { printf("FAIL %s: got '%s' want '%s'\n", what, (got), (want)); fails++; } } while (0)

/* ---- the app list ---- */
static const char *installed[32];
static int n_installed;
static int exists(const char *name)
{
    int i;
    for (i = 0; i < n_installed; i++) if (!strcmp(installed[i], name)) return 1;
    return 0;
}
static int next_name(void *state, char name[REG_NAME])
{
    int *i = state;
    if (*i >= n_installed) return 0;
    strcpy(name, installed[(*i)++]);
    return 1;
}
static int build(struct reg_app *out, int max)
{
    int i = 0;
    return reg_build(out, max, exists, next_name, &i);
}

int main(void)
{
    struct reg_app apps[REG_MAX];
    char t[32];
    int n, i;

    n_installed = 0;
    n = build(apps, REG_MAX);
    CHECK("nothing installed: only the three built-in apps", n, 3);
    CHECKS("built-in 1", apps[0].title, "Clock");
    CHECK("built-in kinds", apps[0].builtin * 100 + apps[1].builtin * 10 + apps[2].builtin, 123);

    {
        const char *names[] = {"ZEBRA", "CALCGPT", "SOLVER", "ALPHA", "CALCOS", "BROWSER", "!HIDDEN", "REELS", "MIDDLE", "SOLVER", "USBTEST"};
        n_installed = (int)(sizeof(names) / sizeof(names[0]));
        for (i = 0; i < n_installed; i++) installed[i] = names[i];
    }
    n = build(apps, REG_MAX);
    CHECKS("known apps first: Math", apps[0].title, "Math");
    CHECKS("known apps keep their order: AI Chat", apps[1].title, "AI Chat");
    CHECKS("Web", apps[2].title, "Web");
    CHECKS("Screen", apps[3].title, "Screen");
    CHECKS("the program of the Math tile", apps[0].name, "SOLVER");
    CHECKS("then the built-in apps", apps[4].title, "Clock");
    CHECKS("then the rest in alphabetical order (first)", apps[7].name, "ALPHA");
    CHECKS("(second)", apps[8].name, "MIDDLE");
    CHECKS("(third)", apps[9].name, "USBTEST");
    CHECKS("(last)", apps[10].name, "ZEBRA");
    CHECK("count: 4 known + 3 built in + 4 others", n, 11);
    CHECK("CalcOS itself, hidden programs and duplicates are not listed", n, 11);
    CHECK("other programs get the generic icon and their own name as title", apps[8].icon == ICON_APP && !strcmp(apps[8].title, "MIDDLE"), 1);

    n_installed = 0;
    {
        static char pool[100][REG_NAME];
        const char *names[100];
        for (i = 0; i < 100; i++) { sprintf(pool[i], "APP%03d", i); names[i] = pool[i]; installed[i < 32 ? i : 0] = names[i]; }
    }
    /* more programs than the list can hold: it stops at the limit and nothing overruns */
    {
        static char pool[100][REG_NAME];
        static const char *many[100];
        int j = 0, count;
        for (i = 0; i < 100; i++) { sprintf(pool[i], "APP%03d", i); many[i] = pool[i]; }
        {
            struct list_state { int i; } st = {0};
            (void)st;
        }
        for (i = 0; i < 32; i++) installed[i] = many[i];
        n_installed = 32;
        count = reg_build(apps, 10, exists, next_name, &j);
        CHECK("the list respects its limit", count, 10);
    }

    /* ---- time and date text ---- */
    fmt_clock(15, 7, 0, t); CHECKS("3:07 PM", t, "3:07 PM");
    fmt_clock(0, 5, 0, t);  CHECKS("midnight is 12:05 AM", t, "12:05 AM");
    fmt_clock(12, 0, 0, t); CHECKS("noon is 12:00 PM", t, "12:00 PM");
    fmt_clock(9, 30, 0, t); CHECKS("9:30 AM", t, "9:30 AM");
    fmt_clock(15, 7, 1, t); CHECKS("24 hour: 15:07", t, "15:07");
    fmt_clock(7, 5, 1, t);  CHECKS("24 hour: 07:05", t, "07:05");
    CHECK("24 October 2026 is a Saturday", weekday(24, 10, 2026), 6);
    CHECK("1 January 2000 was a Saturday", weekday(1, 1, 2000), 6);
    CHECK("29 February 2024 was a Thursday", weekday(29, 2, 2024), 4);
    CHECK("4 July 1976 was a Sunday", weekday(4, 7, 1976), 0);
    fmt_date(24, 10, 2026, t); CHECKS("date", t, "Sat Oct 24 2026");
    fmt_date(5, 3, 2027, t);   CHECKS("date with a one digit day", t, "Fri Mar 5 2027");
    fmt_stopwatch(0, t);        CHECKS("stopwatch at 0", t, "00:00.0");
    fmt_stopwatch(32768, t);    CHECKS("one second", t, "00:01.0");
    fmt_stopwatch(32768 * 75 + 16384, t); CHECKS("1 minute 15.5 seconds", t, "01:15.5");
    fmt_stopwatch(32768u * 3600u, t);     CHECKS("an hour wraps the minutes at 100", t, "60:00.0");
    fmt_stopwatch(4000000000u, t);        CHECK("a huge value stays well formed", t[2] == ':' && t[5] == '.' && t[7] == 0, 1);
    fmt_countdown(125, t); CHECKS("countdown", t, "02:05");
    fmt_countdown(-3, t);  CHECKS("a negative countdown shows zero", t, "00:00");

    /* ---- the notes editor ---- */
    {
        static struct notes nt;
        static int starts[NOTES_MAX_ROWS];
        int row, col, rows;

        notes_init(&nt);
        CHECK("an empty note has one row", notes_rows(&nt, starts, NOTES_MAX_ROWS), 1);
        for (i = 0; i < 5; i++) notes_insert(&nt, (char)('a' + i));
        CHECKS("typed text", nt.text, "abcde");
        CHECK("cursor at the end", nt.cur, 5);
        notes_left(&nt); notes_left(&nt);
        notes_insert(&nt, 'X');
        CHECKS("typing in the middle", nt.text, "abcXde");
        notes_backspace(&nt);
        CHECKS("backspace", nt.text, "abcde");
        notes_left(&nt); notes_left(&nt); notes_left(&nt); notes_left(&nt); notes_left(&nt); notes_left(&nt);
        notes_backspace(&nt);
        CHECKS("backspace at the start does nothing", nt.text, "abcde");
        notes_right(&nt);
        CHECK("right", nt.cur, 1);

        notes_set(&nt, "first line\nsecond\n\nfourth line", 30);
        rows = notes_rows(&nt, starts, NOTES_MAX_ROWS);
        CHECK("newlines make rows", rows, 4);
        CHECK("row starts", starts[0] * 1000 + starts[1] * 10 + 0, 0 * 1000 + 11 * 10);
        CHECK("starts of rows 2 and 3", starts[2] * 100 + starts[3], 18 * 100 + 19);
        notes_cursor(&nt, &row, &col);
        CHECK("cursor at the end of the text is on the last row", row * 100 + col, 3 * 100 + 11);
        notes_up(&nt);
        notes_cursor(&nt, &row, &col);
        CHECK("up goes to the empty row, at its end", row * 100 + col, 2 * 100 + 0);
        notes_up(&nt);
        notes_cursor(&nt, &row, &col);
        CHECK("up again: as much of the remembered column as fits", row * 100 + col, 1 * 100 + 6);
        notes_up(&nt);
        notes_cursor(&nt, &row, &col);
        CHECK("up to the first row: the remembered column fits again (10)", row * 100 + col, 0 * 100 + 10);
        notes_up(&nt);
        notes_cursor(&nt, &row, &col);
        CHECK("up from the first row does nothing", row * 100 + col, 10);
        notes_left(&nt);
        notes_down(&nt);
        notes_cursor(&nt, &row, &col);
        CHECK("moving sideways forgets the remembered column (9 then down)", row * 100 + col, 1 * 100 + 6);
        notes_down(&nt); notes_down(&nt); notes_down(&nt);
        notes_cursor(&nt, &row, &col);
        CHECK("down to the last row", row, 3);
        notes_down(&nt);
        notes_cursor(&nt, &row, &col);
        CHECK("down from the last row does nothing", row, 3);

        /* a line longer than the screen wraps at 36 characters */
        notes_init(&nt);
        for (i = 0; i < 80; i++) notes_insert(&nt, (char)('a' + i % 26));
        rows = notes_rows(&nt, starts, NOTES_MAX_ROWS);
        CHECK("80 characters make 3 rows", rows, 3);
        CHECK("rows start every 36 characters", starts[1] * 100 + starts[2], 36 * 100 + 72);
        notes_cursor(&nt, &row, &col);
        CHECK("cursor on the last row", row * 100 + col, 2 * 100 + 8);
        notes_up(&nt);
        notes_cursor(&nt, &row, &col);
        CHECK("up from a wrapped row keeps the column", row * 100 + col, 1 * 100 + 8);

        /* limits */
        notes_init(&nt);
        for (i = 0; i < NOTES_MAX + 50; i++) notes_insert(&nt, 'z');
        CHECK("the note stops at its limit", nt.len, NOTES_MAX);
        CHECK("inserting when full is refused", notes_insert(&nt, 'q'), 0);
        notes_set(&nt, "hello", 5);
        CHECK("set puts the cursor at the end", nt.cur, 5);
        notes_set(&nt, "x", 99999);
        notes_set(&nt, "abc", 3);
        CHECKS("set replaces", nt.text, "abc");
    }
    printf(fails ? "%d FAILED\n" : "calcos (pure parts): all passed\n", fails);
    return fails != 0;
}
