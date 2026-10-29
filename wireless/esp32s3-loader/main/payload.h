#ifndef PAYLOAD_H
#define PAYLOAD_H

/* The loader's pure parts: reading the files built into the firmware (.8xp / .8xv / .8xg variable files, .8ek flash apps) and
 * deciding, from the calculator's OS version, how C programs get to run on it. No ESP-IDF code, so tests/test_payload.c compiles
 * this on a computer. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VAR_NAME_MAX 9   /* 8 characters and the terminating 0 */
#define TYPE_APP 0x24    /* a flash app, in the calculator's directory and in .8ek files */

typedef struct {
    const uint8_t *start, *end;
} file_t;

/* one variable in a TI-83+/84+ variable file */
typedef struct {
    char name[VAR_NAME_MAX];
    uint8_t type, version;
    bool archived;
    const uint8_t *data;
    uint32_t size;
} var_t;

/* the variable at *pos in the file (a group file has several); false at the end.
 * File: "**TI83F*" ... data section length (2, little-endian) at 0x35, variables from 0x37, a 2-byte checksum after.
 * Variable: header length (2, 0x0B or 0x0D) | data length (2) | type | name (8, 0-padded) | [version | flags (0x80 = archived)] |
 *           data length (2) | data */
bool file_next_var(const file_t *f, size_t *pos, var_t *v);

/* a flash app file (.8ek): "**TIFL**", the app's name at 0x12 (8 bytes), device 0x73 at 0x30, type 0x24 at 0x31,
 * data length (4, little-endian) at 0x4A, the signed app data from 0x4E. False if the file is not one. */
bool file_flash_app(const file_t *f, var_t *v);

/* how C programs run on this OS */
enum {
    RUN_NATIVE,     /* OS 5.4 and older: from the prgm menu as they are */
    RUN_ARTIFICE2,  /* OS 5.5 to 5.8.4: send arTIfiCE v2.1 (a TI-BASIC program, "A"); the user runs prgmA and picks the program */
    RUN_ARTIFICE3,  /* OS 5.8.5: Inequalz app + arTIfiCE v3 program + trigger variable installs the AsmHook2 app, once */
    RUN_UNSUPPORTED /* newer OS: no known way yet */
};
int run_plan(const uint8_t os[3]);

/* a line to show the user for a plan (static text) */
const char *run_plan_text(int plan);

#endif
