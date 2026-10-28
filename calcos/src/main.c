/* CalcOS: a shell for the TI-84 Plus CE. A home screen with app tiles, a status bar (date, time, battery), a few built-in apps
 * (Clock, Notes, System) and every program on the calculator, which it launches and comes back from.
 * It runs on top of the calculator's own operating system (it does not replace it).
 *
 * Keys: arrows move over the tiles, Enter opens, Clear leaves CalcOS. In the apps, Clear goes back. */
#include <fileioc.h>
#include <graphx.h>
#include <keypadc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/power.h>
#include <sys/rtc.h>
#include <sys/timers.h>
#include <ti/info.h>
#include <ti/vars.h>
#include "notesmodel.h"
#include "registry.h"
#include "timefmt.h"
#include "typing.h"

#define SCREEN_W 320
#define HEAD_H 14
#define TILE_W 72
#define TILE_H 56
#define GRID_X 12
#define GRID_Y 40
#define COLS 4
#define VISIBLE_ROWS 3

/* palette numbers */
enum {
    C_BLACK = 0, C_WHITE_TEXT = 1, C_DIM = 2, C_BAR = 3, C_BLUE = 4, C_PURPLE = 5, C_GREEN = 6, C_ORANGE = 7, C_GRAY = 8,
    C_YELLOW = 9, C_TEAL = 10, C_DARK = 11, C_RED = 12, C_PAPER = 13, C_SEL = 14, C_LINE = 15,
    C_BG0 = 16 /* 16 steps of the background gradient: 16 to 31 */, C_WHITE = 255
};

/* keys: number = group * 8 + bit, as in typing_tables.h */
#define K(g, b) ((g) * 8 + (b))
#define K_GRAPH K(1, 0)
#define K_ZOOM K(1, 2)
#define K_WINDOW K(1, 3)
#define K_DEL K(1, 7)
#define K_ENTER K(6, 0)
#define K_CLEAR K(6, 6)
#define K_DOWN K(7, 0)
#define K_LEFT K(7, 1)
#define K_RIGHT K(7, 2)
#define K_UP K(7, 3)

static struct reg_app apps[REG_MAX];
static int n_apps;
static bool use24;

/* ---------------------------------------------------------------- keys */

static uint8_t prev_keys[8];

/* the next key that went down since the last call, as group * 8 + bit, or -1 */
static int next_key(void)
{
    int g, b;
    kb_Scan();
    for (g = 1; g <= 7; g++) {
        uint8_t now = kb_Data[g], pressed = now & (uint8_t)~prev_keys[g];
        for (b = 0; b < 8; b++) {
            if (pressed & (1 << b)) {
                prev_keys[g] |= (uint8_t)(1 << b);   /* any other new key is returned by the next call */
                return K(g, b);
            }
        }
        prev_keys[g] = now;
    }
    return -1;
}

static void forget_keys(void)
{
    int g;
    kb_Scan();
    for (g = 1; g <= 7; g++) prev_keys[g] = kb_Data[g];
}

/* ---------------------------------------------------------------- the programs on the calculator */

static int program_exists(const char *name)
{
    void *vat = NULL;
    char *found;
    while ((found = ti_DetectVar(&vat, name, OS_TYPE_PRGM)) != NULL) if (!strcmp(found, name)) return 1;
    vat = NULL;
    while ((found = ti_DetectVar(&vat, name, OS_TYPE_PROT_PRGM)) != NULL) if (!strcmp(found, name)) return 1;
    return 0;
}

struct scan {
    void *vat;
    int kind;   /* 0: unprotected programs, then 1: protected ones */
};

static int next_program(void *state, char name[REG_NAME])
{
    struct scan *s = state;
    for (;;) {
        char *found = ti_DetectVar(&s->vat, NULL, s->kind == 0 ? OS_TYPE_PRGM : OS_TYPE_PROT_PRGM);
        if (found) {
            strncpy(name, found, REG_NAME - 1);
            name[REG_NAME - 1] = 0;
            return 1;
        }
        if (s->kind == 1) return 0;
        s->kind = 1;
        s->vat = NULL;
    }
}

static void build_apps(void)
{
    struct scan s = {NULL, 0};
    n_apps = reg_build(apps, REG_MAX, program_exists, next_program, &s);
}

/* ---------------------------------------------------------------- drawing helpers */

static void set_palette(void)
{
    int i;
    gfx_palette[C_BLACK] = gfx_RGBTo1555(25, 25, 25);
    gfx_palette[C_WHITE_TEXT] = gfx_RGBTo1555(245, 248, 255);
    gfx_palette[C_DIM] = gfx_RGBTo1555(190, 205, 235);
    gfx_palette[C_BAR] = gfx_RGBTo1555(10, 18, 45);
    gfx_palette[C_BLUE] = gfx_RGBTo1555(66, 133, 244);
    gfx_palette[C_PURPLE] = gfx_RGBTo1555(150, 88, 210);
    gfx_palette[C_GREEN] = gfx_RGBTo1555(52, 168, 83);
    gfx_palette[C_ORANGE] = gfx_RGBTo1555(245, 130, 20);
    gfx_palette[C_GRAY] = gfx_RGBTo1555(105, 118, 140);
    gfx_palette[C_YELLOW] = gfx_RGBTo1555(251, 188, 5);
    gfx_palette[C_TEAL] = gfx_RGBTo1555(0, 150, 170);
    gfx_palette[C_DARK] = gfx_RGBTo1555(70, 82, 105);
    gfx_palette[C_RED] = gfx_RGBTo1555(234, 67, 53);
    gfx_palette[C_PAPER] = gfx_RGBTo1555(250, 250, 244);
    gfx_palette[C_SEL] = gfx_RGBTo1555(255, 255, 255);
    gfx_palette[C_LINE] = gfx_RGBTo1555(60, 80, 130);
    for (i = 0; i < 16; i++) gfx_palette[C_BG0 + i] = gfx_RGBTo1555(18 + i * 3, 36 + i * 4, 88 + i * 6);
    gfx_palette[C_WHITE] = gfx_RGBTo1555(255, 255, 255);
}

static void text_at(const char *s, int x, int y, uint8_t color)
{
    gfx_SetTextFGColor(color);
    gfx_PrintStringXY(s, x, y);
}

static void text_centered(const char *s, int x, int w, int y, uint8_t color)
{
    text_at(s, x + (w - 8 * (int)strlen(s)) / 2, y, color);
}

/* a vertical gradient wallpaper */
static void wallpaper(void)
{
    int y;
    for (y = 0; y < 240; y++) {
        gfx_SetColor((uint8_t)(C_BG0 + y * 16 / 240));
        gfx_HorizLine(0, y, SCREEN_W);
    }
}

static void round_rect(int x, int y, int w, int h, uint8_t color)
{
    gfx_SetColor(color);
    gfx_FillRectangle(x + 4, y, w - 8, h);
    gfx_FillRectangle(x, y + 4, w, h - 8);
    gfx_FillCircle(x + 4, y + 4, 4);
    gfx_FillCircle(x + w - 5, y + 4, 4);
    gfx_FillCircle(x + 4, y + h - 5, 4);
    gfx_FillCircle(x + w - 5, y + h - 5, 4);
}

static void top_bar(const char *title)
{
    gfx_SetColor(C_BAR);
    gfx_FillRectangle(0, 0, SCREEN_W, HEAD_H);
    text_at(title, 6, 3, C_WHITE_TEXT);
    text_at("made by Ethan Paschkes", SCREEN_W - 6 - 22 * 8, 3, C_DIM);
}

/* ---------------------------------------------------------------- app icons, drawn in white inside a coloured tile */

static void icon(int kind, int cx, int cy, uint8_t tile)
{
    int i;
    gfx_SetColor(C_WHITE);
    switch (kind) {
    case ICON_MATH:   /* a calculator */
        gfx_FillRectangle(cx - 10, cy - 14, 20, 28);
        gfx_SetColor(tile);
        gfx_FillRectangle(cx - 7, cy - 11, 14, 6);
        for (i = 0; i < 9; i++) gfx_FillRectangle(cx - 7 + (i % 3) * 5, cy - 2 + (i / 3) * 5, 4, 3);
        break;
    case ICON_AI:     /* a speech bubble with three dots */
        gfx_FillRectangle(cx - 14, cy - 12, 28, 18);
        gfx_FillTriangle(cx - 8, cy + 6, cx - 2, cy + 6, cx - 10, cy + 13);
        gfx_SetColor(tile);
        for (i = -1; i <= 1; i++) gfx_FillCircle(cx + i * 7, cy - 3, 2);
        break;
    case ICON_WEB:    /* a globe */
        gfx_Circle(cx, cy, 13);
        gfx_Circle(cx, cy, 12);
        gfx_Line(cx - 13, cy, cx + 13, cy);
        gfx_Line(cx, cy - 13, cx, cy + 13);
        gfx_Circle(cx, cy, 6);
        break;
    case ICON_SCREEN: /* a monitor with a play triangle */
        gfx_FillRectangle(cx - 15, cy - 12, 30, 20);
        gfx_FillRectangle(cx - 5, cy + 8, 10, 3);
        gfx_FillRectangle(cx - 9, cy + 11, 18, 2);
        gfx_SetColor(tile);
        gfx_FillTriangle(cx - 4, cy - 7, cx - 4, cy + 3, cx + 6, cy - 2);
        break;
    case ICON_CLOCK:
        gfx_FillCircle(cx, cy, 14);
        gfx_SetColor(tile);
        gfx_Line(cx, cy, cx, cy - 9);
        gfx_Line(cx, cy, cx + 6, cy + 3);
        break;
    case ICON_NOTES:  /* a sheet of paper with lines */
        gfx_FillRectangle(cx - 10, cy - 14, 20, 28);
        gfx_SetColor(tile);
        for (i = 0; i < 4; i++) gfx_HorizLine(cx - 7, cy - 8 + i * 6, 14);
        break;
    case ICON_INFO:   /* a circle with an i */
        gfx_FillCircle(cx, cy, 14);
        gfx_SetColor(tile);
        gfx_FillCircle(cx, cy - 7, 2);
        gfx_FillRectangle(cx - 2, cy - 2, 4, 11);
        break;
    default:          /* any other program: four squares */
        for (i = 0; i < 4; i++) gfx_FillRectangle(cx - 12 + (i % 2) * 13, cy - 12 + (i / 2) * 13, 11, 11);
        break;
    }
}

static uint8_t tile_color(int kind)
{
    switch (kind) {
    case ICON_MATH: return C_BLUE;
    case ICON_AI: return C_PURPLE;
    case ICON_WEB: return C_GREEN;
    case ICON_SCREEN: return C_ORANGE;
    case ICON_CLOCK: return C_TEAL;
    case ICON_NOTES: return C_YELLOW;
    case ICON_INFO: return C_GRAY;
    default: return C_DARK;
    }
}

/* ---------------------------------------------------------------- status line: date, time, battery */

static void read_clock(int *h, int *m, int *s)
{
    uint8_t hh, mm, ss;
    boot_GetTime(&ss, &mm, &hh);
    *h = hh;
    *m = mm;
    *s = ss;
}

static void battery_icon(int x, int y)
{
    int level = boot_GetBatteryStatus(), i;
    if (level > 4) level = 4;
    gfx_SetColor(C_DIM);
    gfx_Rectangle(x, y, 22, 10);
    gfx_FillRectangle(x + 22, y + 3, 2, 4);
    for (i = 0; i < level; i++) {
        gfx_SetColor(level <= 1 ? C_RED : C_GREEN);
        gfx_FillRectangle(x + 2 + i * 5, y + 2, 4, 6);
    }
    if (boot_BatteryCharging()) text_at("+", x - 10, y + 1, C_YELLOW);
}

static void status_line(void)
{
    char buf[20];
    uint8_t day, month;
    uint16_t year;
    int h, m, s;
    boot_GetDate(&day, &month, &year);
    fmt_date(day, month, year, buf);
    text_at(buf, 8, 20, C_DIM);
    read_clock(&h, &m, &s);
    fmt_clock(h, m, use24, buf);
    text_at(buf, 222 - 8 * (int)strlen(buf), 20, C_WHITE_TEXT);
    battery_icon(280, 19);
}

/* ---------------------------------------------------------------- the home screen */

static void draw_home(int sel, int top_row)
{
    int i, rows = (n_apps + COLS - 1) / COLS;
    wallpaper();
    top_bar("CalcOS");
    status_line();
    for (i = top_row * COLS; i < n_apps && i < (top_row + VISIBLE_ROWS) * COLS; i++) {
        int col = i % COLS, row = i / COLS - top_row;
        int x = GRID_X + col * (TILE_W + 4), y = GRID_Y + row * (TILE_H + 8);
        uint8_t color = tile_color(apps[i].icon);
        if (i == sel) {
            round_rect(x - 3, y - 3, TILE_W + 6, TILE_H + 6, C_SEL);
        }
        round_rect(x, y, TILE_W, TILE_H, color);
        icon(apps[i].icon, x + TILE_W / 2, y + 22, color);
        text_centered(apps[i].title, x, TILE_W, y + TILE_H - 12, C_WHITE_TEXT);
    }
    if (rows > VISIBLE_ROWS) {   /* page dots */
        int r;
        for (r = 0; r < rows; r++) {
            gfx_SetColor(r == top_row ? C_WHITE : C_LINE);
            gfx_FillCircle(SCREEN_W - 8, GRID_Y + 6 + r * 9, 2);
        }
    }
    text_centered("Enter: open    Clear: leave CalcOS", 0, SCREEN_W, 226, C_DIM);
    gfx_SwapDraw();
}

/* ---------------------------------------------------------------- built-in app: Clock, Stopwatch, Timer */

static void big_text(const char *s, int y, uint8_t color)
{
    gfx_SetTextScale(4, 4);
    gfx_SetTextFGColor(color);
    gfx_PrintStringXY(s, (SCREEN_W - 32 * (int)strlen(s)) / 2, y);
    gfx_SetTextScale(1, 1);
}

static void app_clock(void)
{
    static const char *const tabs[3] = {"Clock", "Stopwatch", "Timer"};
    int tab = 0, i;
    bool sw_running = false, tm_running = false, tm_done = false;
    uint32_t sw_acc = 0, sw_start = 0, tm_end = 0;
    int tm_seconds = 300, tm_left = 300, last_second = -1;
    bool redraw = true;

    forget_keys();
    for (;;) {
        int key = next_key(), h, m, s;
        uint32_t now = timer_Get(1);
        read_clock(&h, &m, &s);
        if (key == K_CLEAR) return;
        if (key == K_LEFT) { tab = (tab + 2) % 3; redraw = true; }
        if (key == K_RIGHT) { tab = (tab + 1) % 3; redraw = true; }
        if (tab == 0 && key == K_ENTER) { use24 = !use24; redraw = true; }
        if (tab == 1) {
            if (key == K_ENTER) {
                if (sw_running) sw_acc += now - sw_start;
                else sw_start = now;
                sw_running = !sw_running;
            }
            if (key == K_DEL) { sw_running = false; sw_acc = 0; }
        }
        if (tab == 2) {
            if (!tm_running && !tm_done) {   /* the length can only be changed while it is not counting */
                if (key == K_UP) tm_seconds += 60;
                if (key == K_DOWN && tm_seconds > 60) tm_seconds -= 60;
                if (key == K_WINDOW) tm_seconds += 10;
                if (key == K_ZOOM && tm_seconds > 10) tm_seconds -= 10;
                if (key == K_UP || key == K_DOWN || key == K_WINDOW || key == K_ZOOM) tm_left = tm_seconds;
            }
            if (key == K_ENTER) {
                if (tm_done) { tm_done = false; tm_left = tm_seconds; }
                else if (tm_running) { tm_running = false; tm_left = (int)((tm_end - now) / 32768u); }
                else { tm_running = true; tm_end = now + (uint32_t)tm_left * 32768u; }
            }
            if (key == K_DEL) { tm_running = false; tm_done = false; tm_left = tm_seconds; }
            if (tm_running) {
                if ((int32_t)(tm_end - now) <= 0) { tm_running = false; tm_done = true; }
                else tm_left = (int)((tm_end - now + 32767u) / 32768u);
            }
        }
        if (key != -1 || s != last_second || sw_running || tm_running || tm_done) redraw = true;
        last_second = s;
        if (redraw) {
            char buf[20];
            wallpaper();
            if (tab == 2 && tm_done && (now / 8192u) % 2) {   /* flash when the timer ends */
                gfx_SetColor(C_RED);
                gfx_FillRectangle(0, HEAD_H, SCREEN_W, 226 - HEAD_H);
            }
            top_bar("Clock");
            for (i = 0; i < 3; i++) {
                round_rect(14 + i * 100, 20, 92, 18, i == tab ? C_TEAL : C_BAR);
                text_centered(tabs[i], 14 + i * 100, 92, 25, C_WHITE_TEXT);
            }
            if (tab == 0) {
                char sec[4];
                fmt_clock(h, m, use24, buf);
                big_text(buf, 80, C_WHITE_TEXT);
                sec[0] = (char)(':'); sec[1] = (char)('0' + s / 10); sec[2] = (char)('0' + s % 10); sec[3] = 0;
                text_centered(sec, 0, SCREEN_W, 124, C_DIM);
                {
                    uint8_t day, month;
                    uint16_t year;
                    boot_GetDate(&day, &month, &year);
                    fmt_date(day, month, year, buf);
                    text_centered(buf, 0, SCREEN_W, 150, C_WHITE_TEXT);
                }
                text_centered("Enter: 12 / 24 hour", 0, SCREEN_W, 212, C_DIM);
            } else if (tab == 1) {
                fmt_stopwatch(sw_running ? sw_acc + (now - sw_start) : sw_acc, buf);
                big_text(buf, 80, C_WHITE_TEXT);
                text_centered(sw_running ? "running" : "stopped", 0, SCREEN_W, 130, C_DIM);
                text_centered("Enter: start / stop    Del: reset", 0, SCREEN_W, 212, C_DIM);
            } else {
                fmt_countdown(tm_left, buf);
                big_text(buf, 80, C_WHITE_TEXT);
                text_centered(tm_done ? "TIME'S UP" : tm_running ? "running" : "Up/Down: minutes   Window/Zoom: 10 s", 0, SCREEN_W, 130, C_DIM);
                text_centered("Enter: start / pause    Del: reset", 0, SCREEN_W, 212, C_DIM);
            }
            gfx_SwapDraw();
            redraw = false;
        }
    }
}

/* ---------------------------------------------------------------- built-in app: Notes */

static struct notes note;
static struct typing ty;

static void notes_load(void)
{
    uint8_t h = ti_Open("CALCNOTE", "r");
    notes_init(&note);
    if (h) {
        int size = ti_GetSize(h);
        if (size > NOTES_MAX) size = NOTES_MAX;
        {
            char data[NOTES_MAX];
            if (ti_Read(data, 1, (size_t)size, h) == (size_t)size) notes_set(&note, data, size);
        }
        ti_Close(h);
    }
}

static void notes_save(void)
{
    uint8_t h = ti_Open("CALCNOTE", "w");
    if (h) {
        ti_Write(note.text, 1, (size_t)note.len, h);
        ti_Close(h);
    }
}

static void app_notes(void)
{
    static int starts[NOTES_MAX_ROWS];
    int top = 0, view = 19;
    bool saved_flash = false;

    notes_load();
    memset(&ty, 0, sizeof(ty));
    forget_keys();
    for (;;) {
        int rows, row, col, i, key;
        rows = notes_rows(&note, starts, NOTES_MAX_ROWS);
        notes_cursor(&note, &row, &col);
        if (row < top) top = row;
        if (row >= top + view) top = row - view + 1;
        wallpaper();
        gfx_SetColor(C_PAPER);
        gfx_FillRectangle(0, HEAD_H, SCREEN_W, 226 - HEAD_H);
        top_bar("Notes");
        for (i = 0; i < view && top + i < rows; i++) {
            char line[NOTES_COLS + 1];
            int a = starts[top + i], b = top + i + 1 < rows ? starts[top + i + 1] : note.len, n;
            if (b > a && note.text[b - 1] == '\n') b--;
            n = b - a;
            if (n > NOTES_COLS) n = NOTES_COLS;
            memcpy(line, note.text + a, (size_t)n);
            line[n] = 0;
            text_at(line, 16, 20 + i * 10, C_BLACK);
        }
        gfx_SetColor(C_BLUE);
        gfx_VertLine(16 + col * 8, 19 + (row - top) * 10, 10);
        gfx_SetColor(C_LINE);
        gfx_HorizLine(0, 226, SCREEN_W);
        text_at(ty.alpha ? (ty.shift ? "ABC" : "abc") : (ty.shift ? "^" : "123"), 4, 230, C_DIM);
        text_at(saved_flash ? "saved" : "Graph: save   Clear: save + exit", 40, 230, C_DIM);
        gfx_SwapDraw();
        saved_flash = false;
        do {
            key = next_key();
        } while (key < 0);
        if (key == K_CLEAR) { notes_save(); return; }
        if (key == K_GRAPH) { notes_save(); saved_flash = true; }
        else if (key == K_ENTER) { notes_insert(&note, '\n'); typing_used_other_key(&ty); }
        else if (key == K_DEL) { notes_backspace(&note); typing_used_other_key(&ty); }
        else if (key == K_LEFT) notes_left(&note);
        else if (key == K_RIGHT) notes_right(&note);
        else if (key == K_UP) notes_up(&note);
        else if (key == K_DOWN) notes_down(&note);
        else {
            char ch = 0;
            if (typing_key(&ty, key, &ch) == TY_CHAR) notes_insert(&note, ch);
        }
    }
}

/* ---------------------------------------------------------------- built-in app: System */

static void number_text(unsigned v, char *out)
{
    char tmp[12];
    int n = 0, i = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) out[i++] = tmp[--n];
    out[i] = 0;
}

static void app_system(void)
{
    char line[40], num[12];
    const system_info_t *info = os_GetSystemInfo();
    void *free_ram;
    int y;
    unsigned kb = (unsigned)(os_MemChk(&free_ram) / 1024u);

    forget_keys();
    wallpaper();
    top_bar("System");
    round_rect(10, 20, 300, 196, C_BAR);
    y = 30;
    text_at("CalcOS 0.1  (a shell on top of TI-OS)", 20, y, C_WHITE_TEXT); y += 20;
    strcpy(line, "Calculator OS: ");
    number_text(info->osMajorVersion, num); strcat(line, num); strcat(line, ".");
    number_text(info->osMinorVersion, num); strcat(line, num); strcat(line, ".");
    number_text(info->osRevisionVersion, num); strcat(line, num);
    text_at(line, 20, y, C_DIM); y += 14;
    strcpy(line, "Free RAM: "); number_text(kb, num); strcat(line, num); strcat(line, " KB");
    text_at(line, 20, y, C_DIM); y += 14;
    strcpy(line, "Battery: "); number_text((unsigned)boot_GetBatteryStatus(), num); strcat(line, num); strcat(line, " of 4");
    if (boot_BatteryCharging()) strcat(line, ", charging");
    text_at(line, 20, y, C_DIM); y += 14;
    strcpy(line, "Programs on this calculator: "); number_text((unsigned)n_apps, num); strcat(line, num);
    text_at(line, 20, y, C_DIM); y += 24;
    text_at("Project programs it can open:", 20, y, C_WHITE_TEXT); y += 14;
    text_at("SOLVER  CALCGPT  BROWSER  REELS", 20, y, C_DIM); y += 24;
    text_at("Made by Ethan Paschkes", 20, y, C_WHITE_TEXT);
    text_centered("Clear: back", 0, SCREEN_W, 226, C_DIM);
    gfx_SwapDraw();
    for (;;) {
        int key = next_key();
        if (key == K_CLEAR || key == K_ENTER) return;
    }
}

/* ---------------------------------------------------------------- the launcher */

static int launcher(int sel);

/* called by the calculator when a program started from here has finished: rebuild everything and show the home screen again */
static int came_back(void *data, int retval)
{
    int sel = data ? *(int *)data : 0;
    (void)retval;
    return launcher(sel);
}

static void run_builtin(int which)
{
    if (which == BUILTIN_CLOCK) app_clock();
    else if (which == BUILTIN_NOTES) app_notes();
    else app_system();
}

static int launcher(int sel)
{
    int top_row = 0;
    bool redraw = true;
    int last_second = -1;

    gfx_Begin();
    set_palette();
    gfx_SetDrawBuffer();
    gfx_SetTextBGColor(C_WHITE);
    gfx_SetTextTransparentColor(C_WHITE);
    timer_Enable(1, TIMER_32K, TIMER_NOINT, TIMER_UP);
    build_apps();
    if (sel >= n_apps) sel = 0;
    forget_keys();
    for (;;) {
        int key = next_key(), h, m, s;
        read_clock(&h, &m, &s);
        if (key == K_CLEAR) break;
        if (key == K_LEFT && sel > 0) { sel--; redraw = true; }
        if (key == K_RIGHT && sel < n_apps - 1) { sel++; redraw = true; }
        if (key == K_UP && sel >= COLS) { sel -= COLS; redraw = true; }
        if (key == K_DOWN) { sel = sel + COLS < n_apps ? sel + COLS : n_apps - 1; redraw = true; }
        if (key == K_ENTER && n_apps > 0) {
            if (apps[sel].builtin) {
                run_builtin(apps[sel].builtin);
                forget_keys();
                redraw = true;
            } else {
                int chosen = sel;
                gfx_End();
                os_RunPrgm(apps[chosen].name, &chosen, sizeof(chosen), came_back);
                return 1;   /* only reached if the program could not be started */
            }
        }
        if (s != last_second) { last_second = s; redraw = true; }   /* the clock moves */
        while (sel / COLS < top_row) top_row--;
        while (sel / COLS >= top_row + VISIBLE_ROWS) top_row++;
        if (redraw) {
            draw_home(sel, top_row);
            redraw = false;
        }
    }
    gfx_End();
    return 0;
}

int main(void)
{
    return launcher(0);
}
