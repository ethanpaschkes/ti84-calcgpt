/* The loader's pure parts (main/payload.c) on a computer: variable files, group files, flash app files, and which jailbreak each
 * OS version gets. Run: python3 test_payload.py */
#include <stdio.h>
#include <string.h>
#include "../main/payload.h"

static int fails;

#define CHECK(what, got, want) do { long g_ = (long)(got), w_ = (long)(want); if (g_ != w_) { printf("FAIL %s: got %ld want %ld\n", what, g_, w_); fails++; } } while (0)

/* builds a variable entry (13-byte header) at out; returns its length */
static size_t put_var(uint8_t *out, const char *name, uint8_t type, uint8_t version, bool archived, const uint8_t *data, uint16_t size)
{
    size_t n = 0;
    out[n++] = 0x0D;
    out[n++] = 0;
    out[n++] = (uint8_t)size;
    out[n++] = (uint8_t)(size >> 8);
    out[n++] = type;
    memset(out + n, 0, 8);
    memcpy(out + n, name, strlen(name));
    n += 8;
    out[n++] = version;
    out[n++] = archived ? 0x80 : 0;
    out[n++] = (uint8_t)size;
    out[n++] = (uint8_t)(size >> 8);
    memcpy(out + n, data, size);
    return n + size;
}

/* a whole .8xp/.8xg file around `entries` bytes of variable entries */
static size_t put_file(uint8_t *out, const uint8_t *entries, size_t n)
{
    memset(out, 0, 0x37);
    memcpy(out, "**TI83F*", 8);
    out[0x35] = (uint8_t)n;
    out[0x36] = (uint8_t)(n >> 8);
    memcpy(out + 0x37, entries, n);
    out[0x37 + n] = 0;   /* checksum, not checked */
    out[0x38 + n] = 0;
    return 0x39 + n;
}

static void test_var_files(void)
{
    static uint8_t entries[600], file[700];
    static const uint8_t prog[] = {0xEF, 0x7B, 0x00, 0xC9}, lib[] = {1, 2, 3};
    size_t n, pos = 0;
    file_t f;
    var_t v;

    n = put_var(entries, "CALC", 0x06, 0, true, prog, sizeof(prog));
    f.start = file;
    f.end = file + put_file(file, entries, n);
    CHECK("one program: found", file_next_var(&f, &pos, &v), 1);
    CHECK("name", strcmp(v.name, "CALC"), 0);
    CHECK("type protected program", v.type, 0x06);
    CHECK("archived", v.archived, 1);
    CHECK("size", v.size, sizeof(prog));
    CHECK("data", memcmp(v.data, prog, sizeof(prog)), 0);
    CHECK("then the end", file_next_var(&f, &pos, &v), 0);

    /* a group with two variables, the second not archived, with a version byte */
    n = put_var(entries, "LibLoad", 0x15, 0, true, lib, sizeof(lib));
    n += put_var(entries + n, "GRAPHX", 0x15, 0x0A, false, prog, sizeof(prog));
    f.end = file + put_file(file, entries, n);
    pos = 0;
    CHECK("group: first", file_next_var(&f, &pos, &v) && strcmp(v.name, "LibLoad") == 0, 1);
    CHECK("group: second", file_next_var(&f, &pos, &v) && strcmp(v.name, "GRAPHX") == 0, 1);
    CHECK("second: version", v.version, 0x0A);
    CHECK("second: not archived", v.archived, 0);
    CHECK("group: end", file_next_var(&f, &pos, &v), 0);

    /* damaged files are refused, not read past their end */
    pos = 0;
    file[0] = 'x';
    CHECK("wrong magic", file_next_var(&f, &pos, &v), 0);
    file[0] = '*';
    file[0x35] = 0xFF;
    file[0x36] = 0x7F;
    CHECK("data section longer than the file", file_next_var(&f, &pos, &v), 0);
    f.end = file + 0x20;
    CHECK("too short", file_next_var(&f, &pos, &v), 0);
}

static void test_flash_app(void)
{
    static uint8_t file[0x4E + 10];
    file_t f = {file, file + sizeof(file)};
    var_t v;
    memset(file, 0, sizeof(file));
    memcpy(file, "**TIFL**", 8);
    memcpy(file + 0x12, "Inequalz", 8);
    file[0x30] = 0x73;
    file[0x31] = 0x24;
    file[0x4A] = 10;
    memset(file + 0x4E, 0xAB, 10);
    CHECK("an app file is read", file_flash_app(&f, &v), 1);
    CHECK("app name", strcmp(v.name, "Inequalz"), 0);
    CHECK("app type", v.type, TYPE_APP);
    CHECK("apps go to the archive", v.archived, 1);
    CHECK("app data size", v.size, 10);
    CHECK("app data", v.data == file + 0x4E, 1);
    file[0x4A] = 11;
    CHECK("data longer than the file", file_flash_app(&f, &v), 0);
    file[0x4A] = 10;
    file[0x31] = 0x23;   /* an OS file */
    CHECK("not an app", file_flash_app(&f, &v), 0);
    file[0x31] = 0x24;
    file[1] = 'x';
    CHECK("wrong magic", file_flash_app(&f, &v), 0);
}

static void test_run_plan(void)
{
    static const struct { uint8_t os[3]; int plan; } cases[] = {
        {{5, 3, 0}, RUN_NATIVE}, {{5, 4, 0}, RUN_NATIVE}, {{4, 2, 0}, RUN_NATIVE},
        {{5, 5, 0}, RUN_ARTIFICE2}, {{5, 5, 1}, RUN_ARTIFICE2}, {{5, 6, 3}, RUN_ARTIFICE2}, {{5, 7, 0}, RUN_ARTIFICE2},
        {{5, 8, 0}, RUN_ARTIFICE2}, {{5, 8, 3}, RUN_ARTIFICE2}, {{5, 8, 4}, RUN_ARTIFICE2},
        {{5, 8, 5}, RUN_ARTIFICE3},
        {{5, 8, 6}, RUN_UNSUPPORTED}, {{5, 9, 0}, RUN_UNSUPPORTED}, {{6, 0, 0}, RUN_UNSUPPORTED},
    };
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int got = run_plan(cases[i].os);
        if (got != cases[i].plan) {
            printf("FAIL OS %u.%u.%u: plan %d, want %d\n", cases[i].os[0], cases[i].os[1], cases[i].os[2], got, cases[i].plan);
            fails++;
        }
    }
    CHECK("every plan has a text", strlen(run_plan_text(RUN_NATIVE)) > 0 && strlen(run_plan_text(RUN_ARTIFICE2)) > 0 &&
                                   strlen(run_plan_text(RUN_ARTIFICE3)) > 0 && strlen(run_plan_text(RUN_UNSUPPORTED)) > 0, 1);
}

int main(void)
{
    test_var_files();
    test_flash_app();
    test_run_plan();
    if (fails) {
        printf("%d FAILED\n", fails);
        return 1;
    }
    printf("loader payload: all passed\n");
    return 0;
}
