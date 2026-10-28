/* Scrolling and link selection in the page model, with hand-built pages. */
#include <stdio.h>
#include <string.h>
#include "../src/pagemodel.h"

static int fails;
static struct pm pm;
static uint8_t buf[8192];
static int len;

#define CHECK(what, got, want) do { int g_ = (got), w_ = (want); if (g_ != w_) { printf("FAIL %s: got %d want %d\n", what, g_, w_); fails++; } } while (0)

static void start_page(const char *kind, const char *title)
{
    len = 0;
    buf[len++] = 'B';
    buf[len++] = (uint8_t)kind[0];
    buf[len++] = (uint8_t)strlen(title);
    memcpy(buf + len, title, strlen(title));
    len += (int)strlen(title);
}

/* a row with up to three spans given as id,start,end triples */
static void row(int style, const char *text, int nspans, const int *spans)
{
    int i;
    buf[len++] = 'W';
    buf[len++] = (uint8_t)style;
    buf[len++] = (uint8_t)nspans;
    buf[len++] = (uint8_t)strlen(text);
    memcpy(buf + len, text, strlen(text));
    len += (int)strlen(text);
    for (i = 0; i < nspans; i++) {
        buf[len++] = (uint8_t)(spans[i * 3] & 255);
        buf[len++] = (uint8_t)(spans[i * 3] >> 8);
        buf[len++] = (uint8_t)spans[i * 3 + 1];
        buf[len++] = (uint8_t)spans[i * 3 + 2];
    }
}

static void end_page(int chunk)
{
    int i;
    buf[len++] = 'E';
    pm_init(&pm);
    for (i = 0; i < len; i += chunk) pm_feed(&pm, buf + i, i + chunk > len ? len - i : chunk);
}

int main(void)
{
    int chunk;
    for (chunk = 1; chunk <= 1000; chunk *= 10) {
        int r;
        /* 40 rows; links 1 on row 2; 2 on rows 5-6 (one link over two rows); 3 and 4 together on row 30; 5 on row 39 */
        start_page("P", "Test page");
        for (r = 0; r < 40; r++) {
            char text[40];
            int spans[9], n = 0;
            sprintf(text, "row %d text", r);
            if (r == 2) { spans[0] = 1; spans[1] = 0; spans[2] = 3; n = 1; }
            if (r == 5 || r == 6) { spans[0] = 2; spans[1] = 4; spans[2] = 8; n = 1; }
            if (r == 30) { spans[0] = 3; spans[1] = 0; spans[2] = 3; spans[3] = 4; spans[4] = 4; spans[5] = 7; n = 2; }
            if (r == 39) { spans[0] = 300; spans[1] = 0; spans[2] = 5; n = 1; }
            row(PM_NORMAL, text, n, spans);
        }
        end_page(chunk);
        CHECK("rows", pm.nrows, 40);
        CHECK("screen kind", pm.screen, PM_PAGE);
        CHECK("not loading", pm.loading, 0);
        CHECK("title", strcmp(pm.title, "Test page"), 0);
        CHECK("first link selected on load", pm_selected_id(&pm), 1);
        CHECK("next link", pm_next_link(&pm, 19), 1);
        CHECK("link over two rows is one link", pm_selected_id(&pm), 2);
        CHECK("next skips the second row of the same link", pm_next_link(&pm, 19), 1);
        CHECK("then link 3", pm_selected_id(&pm), 3);
        CHECK("scrolled so the link is on screen", pm.top <= 30 && 30 < pm.top + 19, 1);
        CHECK("next in the same row", pm_next_link(&pm, 19), 1);
        CHECK("link 4", pm_selected_id(&pm), 4);
        CHECK("link number above 255", (pm_next_link(&pm, 19), pm_selected_id(&pm)), 300);
        CHECK("no more links", pm_next_link(&pm, 19), 0);
        CHECK("still on the last", pm_selected_id(&pm), 300);
        CHECK("prev", pm_prev_link(&pm, 19), 1);
        CHECK("prev is 4", pm_selected_id(&pm), 4);
        pm_prev_link(&pm, 19);
        CHECK("prev is 3", pm_selected_id(&pm), 3);
        pm_prev_link(&pm, 19);
        CHECK("prev lands on the first row of a two-row link", pm.sel_row, 5);
        CHECK("prev is 2", pm_selected_id(&pm), 2);
        pm_prev_link(&pm, 19);
        CHECK("prev is 1", pm_selected_id(&pm), 1);
        CHECK("nothing before the first", pm_prev_link(&pm, 19), 0);
        /* scrolling */
        pm.top = 0;
        pm_scroll(&pm, -5, 19);
        CHECK("cannot scroll above the top", pm.top, 0);
        pm_scroll(&pm, 100, 19);
        CHECK("cannot scroll past the end", pm.top, 40 - 19);
        CHECK("link selection follows what is on screen (first visible link)", pm_selected_id(&pm), 3);
        pm_scroll(&pm, -100, 19);
        CHECK("back at the top, first visible link", pm_selected_id(&pm), 1);
        pm_scroll(&pm, 2, 19);
        CHECK("a link still on screen stays selected", pm_selected_id(&pm), 1);
        pm_scroll(&pm, 1, 19);
        CHECK("scrolled past link 1 selects the next visible one", pm_selected_id(&pm), 2);

        /* a page with no links, then the home page */
        start_page("M", "Message");
        row(PM_NORMAL, "Could not open the page.", 0, NULL);
        end_page(chunk);
        CHECK("no links: nothing selected", pm_selected_id(&pm), 0);
        CHECK("next link with none", pm_next_link(&pm, 19), 0);
        CHECK("message screen", pm.screen, PM_MESSAGE);
        {
            const uint8_t home[] = {'H', 2, 4, 'W', 'i', 'k', 'i', 6, 'D', 'e', 's', 'm', 'o', 's'};
            pm_feed(&pm, home, sizeof(home));
            CHECK("home screen", pm.screen, PM_HOME);
            CHECK("two bookmarks", pm.nbookmarks, 2);
            CHECK("bookmark names", strcmp(pm.bookmarks[0], "Wiki") + strcmp(pm.bookmarks[1], "Desmos"), 0);
            CHECK("home has no rows", pm.nrows, 0);
        }
        /* a status line while loading, then a page */
        {
            const uint8_t status[] = {'T', 10, 'L', 'o', 'a', 'd', 'i', 'n', 'g', '.', '.', '.'};
            pm_feed(&pm, status, sizeof(status));
            CHECK("loading", pm.loading, 1);
            CHECK("status text", strcmp(pm.status, "Loading..."), 0);
        }
        /* more rows than fit: the extra are dropped, and nothing breaks */
        start_page("P", "Huge");
        for (r = 0; r < PM_MAX_ROWS + 20; r++) row(PM_NORMAL, "x", 0, NULL);
        end_page(chunk);
        CHECK("rows capped", pm.nrows, PM_MAX_ROWS);
        CHECK("parser still in step after a huge page", pm.state, 0);
        /* over-long text and too many spans are cut, not overrun */
        start_page("P", "Edge");
        {
            int spans[12] = {1, 0, 1, 2, 2, 3, 3, 3, 4, 4, 4, 5};
            char text[100];
            memset(text, 'y', 60);
            text[60] = 0;
            row(PM_NORMAL, text, 4, spans);
        }
        end_page(chunk);
        CHECK("text cut to the screen width", (int)strlen(pm.rows[0].text), PM_COLS);
        CHECK("spans cut to three", pm.rows[0].nspans, PM_MAX_SPANS);
        CHECK("parser in step after odd row", pm.state, 0);
    }
    printf(fails ? "%d FAILED\n" : "pagemodel: all passed\n", fails);
    return fails != 0;
}
