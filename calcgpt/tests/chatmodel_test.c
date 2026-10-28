/* The chat store: pieces arriving, wrapping, scrolling, trimming. */
#include <stdio.h>
#include <string.h>
#include "../src/chatmodel.h"

static int fails;
static struct chat c;

#define CHECK(what, got, want) do { int g_ = (got), w_ = (want); if (g_ != w_) { printf("FAIL %s: got %d want %d\n", what, g_, w_); fails++; } } while (0)

static void feed_a(const char *text)
{
    uint8_t buf[300];
    int n = (int)strlen(text);
    buf[0] = 'A';
    buf[1] = (uint8_t)n;
    memcpy(buf + 2, text, (size_t)n);
    chat_feed(&c, buf, 2 + n);
}

/* is `needle` anywhere in the chat text? (messages are separated by 0 bytes, so strstr would stop at the first one) */
static int has_text(const char *needle)
{
    int n = (int)strlen(needle), i;
    for (i = 0; i + n <= c.len; i++) if (memcmp(c.buf + i, needle, (size_t)n) == 0) return 1;
    return 0;
}

static void row_text(int i, char *out)
{
    memcpy(out, c.buf + c.rows[i].start, c.rows[i].len);
    out[c.rows[i].len] = 0;
}

int main(void)
{
    char t[64];
    int i;
    chat_init(&c);
    chat_layout(&c, CHAT_VIEW_ROWS);
    CHECK("empty chat has no rows", c.nrows, 0);

    chat_user(&c, "what is sin 30 degrees");
    CHECK("busy after asking", c.busy, 1);
    feed_a("The sine of 30 degrees is ");
    feed_a("exactly 1/2.");
    chat_layout(&c, CHAT_VIEW_ROWS);
    CHECK("two messages", c.nrows, 2);
    row_text(0, t); CHECK("user row", strcmp(t, "what is sin 30 degrees"), 0);
    CHECK("user role", c.rows[0].role, ROLE_USER);
    row_text(1, t); CHECK("assistant pieces joined", strcmp(t, "The sine of 30 degrees is exactly 1/2."), 0 + (strlen("The sine of 30 degrees is exactly 1/2.") > CHAT_COLS ? 1 : 0) * 0);
    {
        const uint8_t done[] = {'D'};
        chat_feed(&c, done, 1);
        CHECK("not busy after D", c.busy, 0);
    }

    /* wrapping at word boundaries */
    chat_clear(&c);
    chat_user(&c, "q");
    feed_a("aaaa bbbb cccc dddd eeee ffff gggg hhhh iiii jjjj");
    chat_layout(&c, CHAT_VIEW_ROWS);
    CHECK("wrapped into two assistant rows", c.nrows, 3);
    for (i = 1; i < c.nrows; i++) { row_text(i, t); CHECK("row fits", strlen(t) <= CHAT_COLS, 1); }
    row_text(1, t); CHECK("breaks at a space (eight words would be 39 wide)", strcmp(t, "aaaa bbbb cccc dddd eeee ffff gggg"), 0);
    row_text(2, t); CHECK("rest on the next row", strcmp(t, "hhhh iiii jjjj"), 0);
    CHECK("only the first row of a message is marked first", c.rows[1].first * 10 + c.rows[2].first, 10);

    /* a long word, newlines and a blank line */
    chat_clear(&c);
    chat_user(&c, "x");
    feed_a("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\nsecond line\n\nafter blank");
    chat_layout(&c, CHAT_VIEW_ROWS);
    row_text(1, t); CHECK("long word cut at the width", (int)strlen(t), CHAT_COLS);
    row_text(2, t); CHECK("rest of the long word", strcmp(t, "mnopqrstuvwxyz"), 0);
    CHECK("newline starts a row", (row_text(3, t), strcmp(t, "second line")), 0);
    CHECK("blank line is a row", c.rows[4].len, 0);
    CHECK("text after the blank line", (row_text(5, t), strcmp(t, "after blank")), 0);
    CHECK("six rows plus the question", c.nrows, 6);

    /* user messages wrap narrower (a margin is drawn) */
    chat_clear(&c);
    chat_user(&c, "aaaaaaaaa bbbbbbbbb ccccccccc ddddddddd");
    chat_layout(&c, CHAT_VIEW_ROWS);
    row_text(0, t); CHECK("user row is at most 36 wide", (int)strlen(t) <= CHAT_USER_COLS, 1);
    CHECK("user message wrapped", c.nrows, 2);

    /* scrolling and following */
    chat_clear(&c);
    chat_user(&c, "q");
    for (i = 0; i < 60; i++) feed_a("line of answer text\n");
    chat_layout(&c, CHAT_VIEW_ROWS);
    CHECK("rows (a trailing newline leaves a blank last row)", c.nrows, 1 + 60 + 1);
    CHECK("follows the newest text", c.top, c.nrows - CHAT_VIEW_ROWS);
    chat_scroll(&c, -5, CHAT_VIEW_ROWS);
    CHECK("scrolled up", c.top, c.nrows - CHAT_VIEW_ROWS - 5);
    CHECK("no longer following", c.follow, 0);
    feed_a("more text arrives\n");
    chat_layout(&c, CHAT_VIEW_ROWS);
    CHECK("view stays where it was while reading", c.top, 62 - CHAT_VIEW_ROWS - 5);
    chat_scroll(&c, 1000, CHAT_VIEW_ROWS);
    CHECK("scrolled to the end", c.top, c.nrows - CHAT_VIEW_ROWS);
    CHECK("following again", c.follow, 1);
    chat_scroll(&c, -1000, CHAT_VIEW_ROWS);
    CHECK("cannot scroll above the top", c.top, 0);
    chat_user(&c, "next question");
    chat_layout(&c, CHAT_VIEW_ROWS);
    CHECK("asking jumps to the newest text", c.top, c.nrows - CHAT_VIEW_ROWS);

    /* status and clear */
    {
        const uint8_t st[] = {'T', 11, 'T', 'h', 'i', 'n', 'k', 'i', 'n', 'g', '.', '.', '.'};
        chat_feed(&c, st, sizeof(st));
        CHECK("status text", strcmp(c.status, "Thinking..."), 0);
        {
            const uint8_t clr[] = {'T', 0};
            chat_feed(&c, clr, 2);
            CHECK("status cleared", c.status[0], 0);
        }
        {
            const uint8_t wipe[] = {'C'};
            chat_feed(&c, wipe, 1);
            chat_layout(&c, CHAT_VIEW_ROWS);
            CHECK("conversation cleared", c.nrows, 0);
        }
    }

    /* the buffer fills up: the oldest messages are forgotten, the newest stay, nothing overruns */
    chat_clear(&c);
    for (i = 0; i < 80; i++) {
        char q[40];
        sprintf(q, "question number %d", i);
        chat_user(&c, q);
        feed_a("answer text that is fairly long so the buffer fills up quickly, and quickly, and quickly again and again");
        feed_a("answer text that is fairly long so the buffer fills up quickly, and quickly, and quickly again and again");
    }
    chat_layout(&c, CHAT_VIEW_ROWS);
    CHECK("buffer stays inside its size", c.len < CHAT_BUF, 1);
    CHECK("the newest question is still there", has_text("question number 79"), 1);
    CHECK("the oldest question is gone", has_text("question number 0") == 0 && has_text("question number 1") == 0, 1);
    CHECK("rows within the limit", c.nrows <= CHAT_MAX_ROWS, 1);
    CHECK("parser still in step", c.state, 0);
    /* one answer bigger than the whole buffer is cut, not overrun */
    chat_clear(&c);
    chat_user(&c, "big");
    for (i = 0; i < 100; i++) feed_a("0123456789012345678901234567890123456789012345678901234567890123456789");
    chat_layout(&c, CHAT_VIEW_ROWS);
    CHECK("an oversized answer fits the buffer", c.len < CHAT_BUF, 1);
    CHECK("an oversized answer leaves a valid first message", c.buf[0] == ROLE_USER || c.buf[0] == ROLE_AI, 1);

    /* the list of models: sent whole, in odd pieces, again, and with too many or too long names */
    {
        const uint8_t list[] = {'K', 'L', 5, 'L', 'o', 'c', 'a', 'l', 'L', 9, 'G', 'P', 'T', ' ', 'f', 'a', 's', 't', '!', 'U', 1};
        int j;
        chat_init(&c);
        for (j = 0; j < (int)sizeof(list); j++) chat_feed(&c, list + j, 1);
        CHECK("two models", c.nmodels, 2);
        CHECK("first name", strcmp(c.models[0], "Local"), 0);
        CHECK("second name", strcmp(c.models[1], "GPT fast!"), 0);
        CHECK("the second is selected", c.model_sel, 1);
        chat_user(&c, "hi");
        feed_a("hello");
        chat_clear(&c);
        CHECK("clearing the chat keeps the model list", c.nmodels, 2);
        chat_feed(&c, (const uint8_t *)"K", 1);
        CHECK("K empties the list", c.nmodels, 0);
        CHECK("K resets the choice", c.model_sel, 0);
        {
            uint8_t many[300];
            int n = 0, k;
            many[n++] = 'K';
            for (k = 0; k < CHAT_MODELS + 3; k++) {
                int m;
                many[n++] = 'L';
                many[n++] = 25;
                for (m = 0; m < 25; m++) many[n++] = (uint8_t)('a' + k);
            }
            chat_feed(&c, many, n);
        }
        CHECK("no more models than fit", c.nmodels, CHAT_MODELS);
        CHECK("a long name is cut", (int)strlen(c.models[0]), CHAT_MODEL_LABEL);
        chat_feed(&c, (const uint8_t *)"U\xff", 2);
        CHECK("an out-of-range choice falls back to the first", c.model_sel, 0);
        feed_a("after");
        CHECK("text still works after the list", has_text("after"), 1);
    }

    printf(fails ? "%d FAILED\n" : "chatmodel: all passed\n", fails);
    return fails != 0;
}
