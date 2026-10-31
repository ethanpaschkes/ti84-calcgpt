/* ChatGPT's calculator tools (src/tools.c) and the tool message in the chat store, on a computer. Run by test_calcgpt.py. */
#include <stdio.h>
#include <string.h>
#include "../src/chatmodel.h"
#include "../src/tools.h"
#include "../../shared/keys.h"
#include "num.h"

static int fails;
static struct tool_ui t;
static struct outbuf out;
static const struct calc_status st = {9, 5, 3, true};

#define CHECK(what, cond) do { if (!(cond)) { printf("FAIL %s\n", what); fails++; } } while (0)
#define KEY_1 K(3, 1)
#define KEY_3 K(5, 1)
#define KEY_4 K(3, 2)

static int open_tool(const char *payload)
{
    out.len = 0;
    return tool_open(&t, payload, (int)strlen(payload), &out, &st);
}

/* the last reply sent: the text after Q, len, byte 1 */
static const char *replied(void)
{
    static char text[300];
    int n;
    if (out.len < 3 || out.buf[0] != 'Q' || out.buf[2] != 1) return "(nothing)";
    n = out.buf[1] - 1;
    memcpy(text, out.buf + 3, (size_t)n);
    text[n] = 0;
    return text;
}

static void test_form_and_choice(void)
{
    tools_init(&t);
    CHECK("form opens", open_tool("form|Right triangle|Fill 2|a|b|c") == TOOL_FORM && t.open);
    CHECK("form: nothing sent yet", out.len == 0);
    CHECK("form: title and three fields", !strcmp(t.form.title, "Right triangle") && t.form.n == 3 && !strcmp(t.form.f[2].label, "c"));
    tool_key(&t, KEY_3, &out);
    tool_key(&t, K_ENTER, &out);
    tool_key(&t, KEY_4, &out);
    tool_key(&t, K_ENTER, &out);
    tool_key(&t, K_ENTER, &out);
    CHECK("form: still open on the Done row", t.open && t.form.cur == 3 && out.len == 0);
    tool_key(&t, K_ENTER, &out);
    CHECK("form: Enter on Done replies with the filled fields", !strcmp(replied(), "form|a=3|b=4"));
    CHECK("form: the card closed", !t.open);
    open_tool("form|T|h|x");
    tool_key(&t, K_CLEAR, &out);
    CHECK("form: Clear cancels", !strcmp(replied(), "form|cancel") && !t.open);
    CHECK("form without fields is an error", open_tool("form|T|h") == TOOL_NONE && !strncmp(replied(), "error|", 6));
    CHECK("form: at most 10 fields", open_tool("form|T|h|1|2|3|4|5|6|7|8|9|10|11|12") == TOOL_FORM && t.form.n == 10);

    CHECK("choice opens", open_tool("choice|What kind?|Right triangle|Any triangle") == TOOL_CHOICE && t.noptions == 2);
    tool_key(&t, K_DOWN, &out);
    tool_key(&t, K_DOWN, &out);
    CHECK("choice: Down stops at the last", t.sel == 1);
    tool_key(&t, K_ENTER, &out);
    CHECK("choice: Enter replies with the option", !strcmp(replied(), "choice|Any triangle") && !t.open);
    open_tool("choice|Q|a|b");
    tool_key(&t, K_CLEAR, &out);
    CHECK("choice: Clear cancels", !strcmp(replied(), "choice|cancel"));
    CHECK("help names the keys", strstr(tool_help(&t), "Enter") != NULL);
}

static void test_cards(void)
{
    CHECK("answers card", open_tool("answers|Answers are:|Pythagoras|c = 5|A = 36.87") == TOOL_ANSWERS && t.open);
    CHECK("answers: ok at once", !strcmp(replied(), "ok"));
    CHECK("answers: the lines", t.ans.n == 2 && !strcmp(t.ans.line[0], "c = 5") && !strcmp(t.ans.rule, "Pythagoras"));
    tool_key(&t, K_ENTER, &out);
    CHECK("answers: Enter closes", !t.open);
    CHECK("steps card", open_tool("steps|Law of Sines|a/sin A = b/sin B|b = 8 sin 30 / sin 20") == TOOL_STEPS);
    CHECK("steps: lines joined", !strcmp(t.ans.steps, "a/sin A = b/sin B\nb = 8 sin 30 / sin 20"));
    tool_key(&t, K_CLEAR, &out);
    CHECK("steps: Clear closes", !t.open);

    CHECK("triangle card", open_tool("triangle|3|4|5|36.87|53.13|90") == TOOL_TRIANGLE && !strcmp(replied(), "ok"));
    CHECK("triangle: lines from the numbers", t.ans.n == 7 && !strcmp(t.ans.line[2], "c = 5") && !strcmp(t.ans.line[6], "Area = 6"));
    CHECK("triangle with a missing number", open_tool("triangle|3|4|5") == TOOL_NONE && !strncmp(replied(), "error|", 6));
    CHECK("triangle with a word", open_tool("triangle|3|4|five|1|2|3") == TOOL_NONE && !t.open);
    CHECK("parabola card", open_tool("parabola|1|-5|6") == TOOL_PARABOLA && t.quad.nreal == 2 && !strcmp(t.ans.line[0], "x = 2"));
    CHECK("lines card", open_tool("lines|2|3|1|7") == TOOL_LINES && t.lkind == 1 && !strcmp(t.ans.line[0], "x = 4"));
    CHECK("circle card", open_tool("circle|150") == TOOL_CIRCLE && t.trig.quadrant == 2 && !strcmp(t.ans.line[0], "sin = 1/2"));
    CHECK("dots card", open_tool("dots|1|3|5|7|9") == TOOL_DOTS && t.stats.n == 5 && !strcmp(t.ans.line[1], "mean = 5"));
    CHECK("dots with nothing", open_tool("dots") == TOOL_NONE && !strncmp(replied(), "error|", 6));
}

static void test_instant(void)
{
    num_decimals = 2;
    CHECK("decimals", open_tool("decimals|3") == TOOL_NONE && num_decimals == 3 && !strcmp(replied(), "ok") && !t.open);
    CHECK("decimals clamp", open_tool("decimals|9") == TOOL_NONE && num_decimals == 4);
    num_decimals = 2;
    CHECK("status", open_tool("status") == TOOL_NONE && !strcmp(replied(), "status|time=9:05|battery=3 of 4|online=yes"));
    CHECK("unknown tool", open_tool("dance|now") == TOOL_NONE && !strcmp(replied(), "error|unknown tool"));
    CHECK("empty request", open_tool("") == TOOL_NONE && !strncmp(replied(), "error|", 6));
    {
        char big[400];
        memset(big, 'x', sizeof(big) - 1);
        big[sizeof(big) - 1] = 0;
        memcpy(big, "answers|T|r|", 12);
        CHECK("a long request is cut, not overrun", open_tool(big) == TOOL_ANSWERS && strlen(t.text) <= CHAT_TOOL_MAX);
    }
}

/* the chat store hands a status that starts with byte 1 to the tool buffer instead of the footer */
static void test_chat_store(void)
{
    static struct chat c;
    static const uint8_t wire[] = {'T', 8, 1, 'f', 'o', 'r', 'm', '|', 'T', '|', 'A', 2, 'h', 'i', 'T', 3, 'o', 'l', 'd'};
    int i;
    chat_init(&c);
    for (i = 0; i < (int)sizeof(wire); i++) chat_feed(&c, wire + i, 1);
    CHECK("tool request arrives", c.tool_ready && c.tool_len == 7 && !strcmp(c.tool, "form|T|"));
    CHECK("it is not shown as a status", strcmp(c.status, "old") == 0);
    CHECK("the parser stayed in step", c.state == 0 && c.len > 0);
    c.tool_ready = 0;
    chat_feed(&c, wire, sizeof(wire));
    CHECK("whole, too", c.tool_ready && !strcmp(c.tool, "form|T|"));
    {
        uint8_t big[260];
        big[0] = 'T';
        big[1] = 255;
        big[2] = 1;
        memset(big + 3, 'a', 254);
        c.tool_ready = 0;
        chat_feed(&c, big, sizeof(big) - 3);
        CHECK("a 254-byte request fits", c.tool_ready && c.tool_len == CHAT_TOOL_MAX);
    }
}

int main(void)
{
    test_form_and_choice();
    test_cards();
    test_instant();
    test_chat_store();
    if (fails) {
        printf("%d FAILED\n", fails);
        return 1;
    }
    printf("tools: all passed\n");
    return 0;
}
