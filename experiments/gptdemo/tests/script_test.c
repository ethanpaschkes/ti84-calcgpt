#include <stdio.h>
#include <string.h>
#include "../src/script.h"

static int fails;
#define CHECK(what, cond) do { if (!(cond)) { printf("FAIL %s\n", what); fails++; } } while (0)

static void line(const char *text, int width, int i, char *out)
{
    int starts[40], lens[40], n = wrap_text(text, width, starts, lens, 40);
    if (i >= n) { strcpy(out, "<none>"); return; }
    memcpy(out, text + starts[i], (size_t)lens[i]);
    out[lens[i]] = 0;
}

int main(void)
{
    char t[100];
    int starts[40], lens[40], i, n;
    CHECK("sin 30 answer", strstr(script_reply("what is SIN 30"), "1/2") != NULL);
    CHECK("law of sines", strstr(script_reply("explain the Law of Sines"), "sin A") != NULL);
    CHECK("quadratic", strstr(script_reply("quadratic formula?"), "discriminant") != NULL);
    CHECK("who are you", strstr(script_reply("who are you"), "no internet") != NULL || strstr(script_reply("who are you"), "No internet") != NULL);
    CHECK("greeting", strstr(script_reply("hello"), "CalcGPT") != NULL);
    CHECK("very short input is a greeting", strstr(script_reply("x"), "Hi!") != NULL);
    CHECK("unknown question gets the on-device fallback", strstr(script_reply("what is the capital of france"), "on-device") != NULL);
    CHECK("never NULL", script_reply("") != NULL && script_reply("zzzzzz zzzz") != NULL);
    CHECK("three photos", N_PHOTOS == 3);
    for (i = 0; i < N_PHOTOS; i++) {
        CHECK("every photo has all four texts", PHOTOS[i].caption && PHOTOS[i].ocr && PHOTOS[i].answer && PHOTOS[i].taken);
        CHECK("the answer starts with what was read", strncmp(PHOTOS[i].answer, "I read: ", 8) == 0);
    }
    /* the scripted maths is right: 2x+3=11 -> x=4; x^2-5x+6 -> 2 and 3; 2 sin x = 1 -> 30, 150 */
    CHECK("photo 1 answer x = 4", strstr(PHOTOS[0].answer, "x = 4") != NULL && 2 * 4 + 3 == 11);
    CHECK("photo 2 roots", strstr(PHOTOS[1].answer, "x = 2 or x = 3") != NULL && (2 * 2 - 5 * 2 + 6) == 0 && (3 * 3 - 5 * 3 + 6) == 0);
    CHECK("photo 3 angles", strstr(PHOTOS[2].answer, "x = 30 deg or x = 150 deg") != NULL);

    line("hello world this is a test", 11, 0, t); CHECK("wrap 1", !strcmp(t, "hello world"));
    line("hello world this is a test", 11, 1, t); CHECK("wrap 2", !strcmp(t, "this is a"));
    line("hello world this is a test", 11, 2, t); CHECK("wrap 3", !strcmp(t, "test"));
    line("aaa\nbbb", 20, 1, t); CHECK("newline starts a line", !strcmp(t, "bbb"));
    line("aaa\n\nbbb", 20, 1, t); CHECK("blank line", !strcmp(t, ""));
    line("abcdefghij", 4, 0, t); CHECK("a long word is cut", !strcmp(t, "abcd"));
    line("abcdefghij", 4, 2, t); CHECK("cut word end", !strcmp(t, "ij"));
    n = wrap_text("", 10, starts, lens, 40); CHECK("empty text is one empty line", n == 1 && lens[0] == 0);
    {
        int total_ok = 1;
        for (i = 0; i < N_PHOTOS; i++) {
            int k;
            n = wrap_text(PHOTOS[i].answer, 28, starts, lens, 40);
            for (k = 0; k < n; k++) if (lens[k] > 28) total_ok = 0;
            if (n >= 40) total_ok = 0;
        }
        CHECK("every photo answer wraps within 28 columns and fits 40 lines", total_ok);
    }
    n = wrap_text("a b c d e f g h", 3, starts, lens, 2);
    CHECK("the line limit is respected", n == 2);
    printf(fails ? "%d FAILED\n" : "gptdemo script: all passed\n", fails);
    return fails != 0;
}
