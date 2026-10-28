/* GPTDEMO: an offline "CalcGPT" demo. It looks like an AI chat app and runs entirely on the calculator with no cable and no Mac.
 * Every reply is scripted, and the "photos" are pictures stored in the program (see photos.h) (see script.c): nothing here is a real language model.
 *
 * Keys: type a question and press Enter, Y= imports a photo to "solve", Up/Down scroll, Graph starts a new chat,
 * Clear erases the line, then quits. */
#include <graphx.h>
#include <keypadc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/power.h>
#include <sys/timers.h>
#include "photos.h"
#include "script.h"
#include "typing.h"

#define SCREEN_W 320
#define HEAD_H 14
#define CHAT_TOP 30
#define CHAT_BOTTOM 196
#define LINE_H 10
#define MAX_MSG 20
#define INPUT_MAX 60
#define TICKS_PER_SECOND 32768u

/* palette numbers: a dark chat theme; the photos use PHOTO_BASE..PHOTO_BASE+15 */
enum {
    C_BG, C_BAR, C_USER, C_ASSIST, C_TEXT, C_DIM, C_GREEN, C_RED, C_PILL, C_PILL_EDGE,
    C_SCAN, C_BLACK, PHOTO_BASE = 32,
    C_WHITE = 255
};

#define K(g, b) ((g) * 8 + (b))
#define K_GRAPH K(1, 0)
#define K_YEQU K(1, 4)
#define K_DEL K(1, 7)
#define K_ENTER K(6, 0)
#define K_CLEAR K(6, 6)
#define K_DOWN K(7, 0)
#define K_LEFT K(7, 1)
#define K_RIGHT K(7, 2)
#define K_UP K(7, 3)

enum { ROLE_USER, ROLE_AI, ROLE_PHOTO };

struct msg {
    uint8_t role;
    int photo;            /* ROLE_PHOTO: which photo */
    const char *text;     /* ROLE_USER: points into buf; ROLE_AI: a scripted string */
    char buf[INPUT_MAX + 1];
    int shown;            /* ROLE_AI: how many characters have been typed out so far */
};

static struct msg msgs[MAX_MSG];
static int n_msgs;
static struct typing ty;
static char input[INPUT_MAX + 1];
static int input_len;
static int scroll_px;                 /* how far the reader has scrolled up, in pixels */
static int streaming = -1;            /* the assistant message being typed out, or -1 */
static uint32_t stream_clock;         /* the counter value up to which characters have been counted */
static uint32_t think_until;          /* the assistant "thinks" (animated dots) until this counter value */
static int chars_per_second = 45;
static uint8_t prev_keys[8];

/* ---------------------------------------------------------------- keys and time */

static int next_key(void)
{
    int g, b;
    kb_Scan();
    for (g = 1; g <= 7; g++) {
        uint8_t now = kb_Data[g], pressed = now & (uint8_t)~prev_keys[g];
        for (b = 0; b < 8; b++) {
            if (pressed & (1 << b)) {
                prev_keys[g] |= (uint8_t)(1 << b);
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

/* has the counter passed `when`? (works across the counter wrapping round) */
static bool reached(uint32_t when)
{
    return (int32_t)(timer_Get(1) - when) >= 0;
}

/* ---------------------------------------------------------------- drawing basics */

static void set_palette(void)
{
    int i;
    gfx_palette[C_BG] = gfx_RGBTo1555(24, 25, 29);
    gfx_palette[C_BAR] = gfx_RGBTo1555(36, 38, 46);
    gfx_palette[C_USER] = gfx_RGBTo1555(48, 108, 240);
    gfx_palette[C_ASSIST] = gfx_RGBTo1555(50, 53, 62);
    gfx_palette[C_TEXT] = gfx_RGBTo1555(238, 240, 245);
    gfx_palette[C_DIM] = gfx_RGBTo1555(145, 150, 168);
    gfx_palette[C_GREEN] = gfx_RGBTo1555(16, 163, 127);
    gfx_palette[C_RED] = gfx_RGBTo1555(224, 90, 80);
    gfx_palette[C_PILL] = gfx_RGBTo1555(44, 46, 55);
    gfx_palette[C_PILL_EDGE] = gfx_RGBTo1555(84, 88, 102);
    gfx_palette[C_SCAN] = gfx_RGBTo1555(70, 255, 150);
    gfx_palette[C_BLACK] = gfx_RGBTo1555(10, 10, 12);
    gfx_palette[C_WHITE] = gfx_RGBTo1555(255, 255, 255);
    for (i = 0; i < 16; i++) gfx_palette[PHOTO_BASE + i] = gfx_RGBTo1555(PHOTO_PALETTE[i][0], PHOTO_PALETTE[i][1], PHOTO_PALETTE[i][2]);
}

static void text_at(const char *s, int x, int y, uint8_t color)
{
    gfx_SetTextFGColor(color);
    gfx_PrintStringXY(s, x, y);
}

static void text_centered(const char *s, int y, uint8_t color)
{
    text_at(s, (SCREEN_W - 8 * (int)strlen(s)) / 2, y, color);
}

static void round_rect(int x, int y, int w, int h, int r, uint8_t color)
{
    gfx_SetColor(color);
    gfx_FillRectangle(x + r, y, w - 2 * r, h);
    gfx_FillRectangle(x, y + r, w, h - 2 * r);
    gfx_FillCircle(x + r, y + r, r);
    gfx_FillCircle(x + w - r - 1, y + r, r);
    gfx_FillCircle(x + r, y + h - r - 1, r);
    gfx_FillCircle(x + w - r - 1, y + h - r - 1, r);
}

static void print_n(const char *s, int n, int x, int y, uint8_t color)
{
    char buf[40];
    if (n > 38) n = 38;
    memcpy(buf, s, (size_t)n);
    buf[n] = 0;
    text_at(buf, x, y, color);
}

static void top_bar(void)
{
    gfx_SetColor(C_BAR);
    gfx_FillRectangle(0, 0, SCREEN_W, HEAD_H);
    text_at("CalcGPT", 6, 3, C_TEXT);
    text_at("made by Ethan Paschkes", SCREEN_W - 6 - 22 * 8, 3, C_DIM);
}

static void battery(int x, int y)
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
}

/* the strip under the title: the model name, that it is offline, and the battery */
static void model_strip(bool busy)
{
    char chip[32] = "gpt-calc-5   0.3B q4   offline";
    gfx_SetColor(C_BG);
    gfx_FillRectangle(0, HEAD_H, SCREEN_W, 16);
    gfx_SetColor(C_GREEN);
    gfx_FillCircle(10, 23, 3);
    text_at(chip, 20, 19, C_DIM);
    if (busy) {
        char tps[16] = "41 tok/s";
        uint32_t t = timer_Get(1) / 3000u;
        tps[0] = (char)('3' + t % 3);
        tps[1] = (char)('0' + (t / 3) % 10);
        text_at(tps, 214, 19, C_GREEN);
    }
    battery(290, 18);
}

/* ---------------------------------------------------------------- the photos */

/* the picture lives in the program as 16 colours, two pixels per byte; it is spread into a sprite at the size asked for */
static struct { uint8_t w, h; uint8_t data[PHOTO_W * PHOTO_H]; } photo_sprite;
static int sprite_idx = -1, sprite_w, sprite_h;

static const gfx_sprite_t *photo_at_size(int idx, int w, int h)
{
    int x, y;
    if (idx != sprite_idx || w != sprite_w || h != sprite_h) {
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                int sx = x * PHOTO_W / w, sy = y * PHOTO_H / h, i = sy * PHOTO_W + sx;
                uint8_t two = PHOTO_DATA[idx][i / 2];
                photo_sprite.data[y * w + x] = (uint8_t)(PHOTO_BASE + ((i & 1) ? (two & 15) : (two >> 4)));
            }
        }
        photo_sprite.w = (uint8_t)w;
        photo_sprite.h = (uint8_t)h;
        sprite_idx = idx;
        sprite_w = w;
        sprite_h = h;
    }
    return (const gfx_sprite_t *)&photo_sprite;
}

/* the photo's height when it is drawn w wide */
static int photo_height(int w)
{
    return w * PHOTO_H / PHOTO_W;
}

static void draw_photo(int idx, int x, int y, int w)
{
    gfx_Sprite(photo_at_size(idx, w, photo_height(w)), x, y);
}

/* where the equation is inside a photo drawn w wide at (x, y) */
struct photo_box {
    int text_x, text_y, text_w, text_h;
};

static struct photo_box photo_layout(int idx, int x, int y, int w)
{
    struct photo_box b;
    b.text_x = x + PHOTO_BOX[idx][0] * w / PHOTO_W;
    b.text_y = y + PHOTO_BOX[idx][1] * w / PHOTO_W;
    b.text_w = (PHOTO_BOX[idx][2] - PHOTO_BOX[idx][0]) * w / PHOTO_W;
    b.text_h = (PHOTO_BOX[idx][3] - PHOTO_BOX[idx][1]) * w / PHOTO_W;
    return b;
}

/* corner marks like a camera viewfinder */
static void viewfinder(int x, int y, int w, int h, uint8_t color)
{
    gfx_SetColor(color);
    gfx_HorizLine(x, y, 14); gfx_VertLine(x, y, 14);
    gfx_HorizLine(x + w - 14, y, 14); gfx_VertLine(x + w - 1, y, 14);
    gfx_HorizLine(x, y + h - 1, 14); gfx_VertLine(x, y + h - 14, 14);
    gfx_HorizLine(x + w - 14, y + h - 1, 14); gfx_VertLine(x + w - 1, y + h - 14, 14);
}

/* ---------------------------------------------------------------- the chat */

static void add_message(int role, const char *text, int photo)
{
    struct msg *m;
    if (n_msgs == MAX_MSG) {
        memmove(&msgs[0], &msgs[1], sizeof(msgs[0]) * (MAX_MSG - 1));
        n_msgs--;
        if (streaming >= 0) streaming--;
    }
    m = &msgs[n_msgs++];
    memset(m, 0, sizeof(*m));
    m->role = (uint8_t)role;
    m->photo = photo;
    if (role == ROLE_USER) {
        strncpy(m->buf, text, INPUT_MAX);
        m->text = m->buf;
    } else {
        m->text = text;
    }
    scroll_px = 0;
}

/* the assistant starts "thinking" and then types out `text` */
static void assistant_says(const char *text, int think_ms, int cps)
{
    add_message(ROLE_AI, text, 0);
    streaming = n_msgs - 1;
    chars_per_second = cps;
    think_until = timer_Get(1) + (uint32_t)think_ms * TICKS_PER_SECOND / 1000u;
    stream_clock = think_until;
}

static void advance_stream(void)
{
    struct msg *m;
    uint32_t now, elapsed;
    int add, len;
    if (streaming < 0 || !reached(think_until)) return;
    m = &msgs[streaming];
    now = timer_Get(1);
    elapsed = now - stream_clock;
    add = (int)(elapsed * (uint32_t)chars_per_second / TICKS_PER_SECOND);
    if (add <= 0) return;
    stream_clock += (uint32_t)add * TICKS_PER_SECOND / (uint32_t)chars_per_second;
    len = (int)strlen(m->text);
    m->shown += add;
    if (m->shown >= len) {
        m->shown = len;
        streaming = -1;
    }
}

#define BUBBLE_COLS_USER 28
#define BUBBLE_COLS_AI 30
#define MAX_LINES 40

static int message_height(const struct msg *m)
{
    int starts[MAX_LINES], lens[MAX_LINES], n;
    char part[500];
    if (m->role == ROLE_PHOTO) return 6 + 62 + 4 + LINE_H + 6 + 6;
    if (m->role == ROLE_USER) n = wrap_text(m->text, BUBBLE_COLS_USER, starts, lens, MAX_LINES);
    else {
        int len = m->shown < 499 ? m->shown : 499;
        memcpy(part, m->text, (size_t)len);
        part[len] = 0;
        n = wrap_text(part, BUBBLE_COLS_AI, starts, lens, MAX_LINES);
    }
    return n * LINE_H + 12 + 6;
}

/* draws one message with its top at y; returns nothing (heights come from message_height) */
static void draw_message(const struct msg *m, int y, bool thinking)
{
    int starts[MAX_LINES], lens[MAX_LINES], n, i;
    char part[500];
    if (m->role == ROLE_PHOTO) {
        int w = 96, x = SCREEN_W - 8 - w - 12;
        round_rect(x, y, w + 12, 62 + 4 + LINE_H + 12, 8, C_USER);
        draw_photo(m->photo, x + 6, y + 6 + (62 - photo_height(w)) / 2, w);
        text_at("Solve this equation", x + 6, y + 6 + 62 + 4, C_TEXT);
        return;
    }
    if (m->role == ROLE_USER) {
        int w, longest = 0;
        n = wrap_text(m->text, BUBBLE_COLS_USER, starts, lens, MAX_LINES);
        for (i = 0; i < n; i++) if (lens[i] > longest) longest = lens[i];
        w = longest * 8 + 14;
        round_rect(SCREEN_W - 8 - w - 4, y, w + 4, n * LINE_H + 12, 8, C_USER);
        for (i = 0; i < n; i++) print_n(m->text + starts[i], lens[i], SCREEN_W - 8 - w + 3, y + 6 + i * LINE_H, C_TEXT);
        return;
    }
    {
        int len = m->shown < 499 ? m->shown : 499, longest = 0, w;
        memcpy(part, m->text, (size_t)len);
        part[len] = 0;
        n = wrap_text(part, BUBBLE_COLS_AI, starts, lens, MAX_LINES);
        for (i = 0; i < n; i++) if (lens[i] > longest) longest = lens[i];
        if (thinking) longest = 3;
        w = longest * 8 + 14;
        gfx_SetColor(C_GREEN);
        gfx_FillCircle(14, y + 9, 7);
        text_at("G", 11, y + 5, C_BLACK);
        round_rect(26, y, w + 4, (thinking ? 1 : n) * LINE_H + 12, 8, C_ASSIST);
        if (thinking) {
            int dot = (int)((timer_Get(1) / 6000u) % 3);
            for (i = 0; i < 3; i++) {
                gfx_SetColor(i == dot ? C_TEXT : C_DIM);
                gfx_FillCircle(40 + i * 10, y + 11, 2);
            }
        } else {
            for (i = 0; i < n; i++) print_n(part + starts[i], lens[i], 33, y + 6 + i * LINE_H, C_TEXT);
            if (streaming >= 0 && m == &msgs[streaming] && (timer_Get(1) / 8192u) % 2) {   /* a blinking cursor while it types */
                int last = n - 1;
                gfx_SetColor(C_TEXT);
                gfx_FillRectangle(33 + lens[last] * 8, y + 6 + last * LINE_H, 5, 9);
            }
        }
    }
}

static void draw_welcome(void)
{
    static const char name[] = "CalcGPT";
    char one[2] = {0, 0};
    int i;
    gfx_SetTextScale(3, 3);
    for (i = 0; i < 7; i++) {
        one[0] = name[i];
        text_at(one, 76 + i * 24, 60, i % 2 ? C_GREEN : C_TEXT);
    }
    gfx_SetTextScale(1, 1);
    text_centered("Runs on this calculator. No internet.", 100, C_DIM);
    text_centered("Try  solve 2x + 3 = 11  or press Y=", 128, C_TEXT);
    text_centered("to import a photo of a problem.", 140, C_TEXT);
}

static void draw_input(void)
{
    int shown = input_len > 26 ? input_len - 26 : 0;
    gfx_SetColor(C_BG);
    gfx_FillRectangle(0, 198, SCREEN_W, 42);
    round_rect(6, 202, 308, 22, 11, C_PILL_EDGE);
    round_rect(7, 203, 306, 20, 10, C_PILL);
    /* a camera icon: the Y= key */
    gfx_SetColor(C_DIM);
    gfx_FillRectangle(15, 210, 14, 8);
    gfx_FillRectangle(19, 207, 6, 3);
    gfx_SetColor(C_PILL);
    gfx_FillCircle(22, 214, 2);
    if (input_len == 0) {
        text_at("Message CalcGPT...", 38, 209, C_DIM);
    } else {
        text_at(input + shown, 38, 209, C_TEXT);
        gfx_SetColor(C_TEXT);
        gfx_VertLine(38 + (input_len - shown) * 8, 207, 11);
    }
    text_at(ty.alpha ? (ty.shift ? "ABC" : "abc") : (ty.shift ? "^" : "123"), 280, 209, C_DIM);
    text_at("Y=: photo  Enter: send  Graph: new", 4, 229, C_DIM);
}

static void draw_chat(void)
{
    int i, first = n_msgs, total = 0, y;
    bool thinking = streaming >= 0 && !reached(think_until);
    gfx_SetColor(C_BG);
    gfx_FillRectangle(0, 0, SCREEN_W, 240);
    if (n_msgs == 0) {
        draw_welcome();
    } else {
        /* find the first message that can be seen, working up from the newest */
        while (first > 0 && total < (CHAT_BOTTOM - CHAT_TOP) + scroll_px + 60) {
            first--;
            total += (first == streaming && thinking ? LINE_H + 12 + 6 : message_height(&msgs[first]));
        }
        y = CHAT_BOTTOM + scroll_px - total;
        for (i = first; i < n_msgs; i++) {
            bool think_this = i == streaming && thinking;
            draw_message(&msgs[i], y, think_this);
            y += think_this ? LINE_H + 12 + 6 : message_height(&msgs[i]);
        }
    }
    model_strip(streaming >= 0 && !thinking);
    top_bar();
    draw_input();
    gfx_SwapDraw();
}

/* ---------------------------------------------------------------- the splash screen */

static void splash(void)
{
    static const char *const steps[3] = {"Mapping weights (0.3B, 4-bit)", "Warming up the eZ80 at 48 MHz", "Ready. Running offline."};
    uint32_t start = timer_Get(1);
    for (;;) {
        uint32_t elapsed = timer_Get(1) - start;
        int pct = (int)(elapsed * 100u / (2 * TICKS_PER_SECOND));
        int step = pct < 40 ? 0 : pct < 85 ? 1 : 2;
        static const char name[] = "CalcGPT";
        char one[2] = {0, 0};
        int i;
        if (pct > 100) pct = 100;
        gfx_SetColor(C_BG);
        gfx_FillRectangle(0, 0, SCREEN_W, 240);
        gfx_SetTextScale(3, 3);
        for (i = 0; i < 7; i++) {
            one[0] = name[i];
            text_at(one, 76 + i * 24, 70, i % 2 ? C_GREEN : C_TEXT);
        }
        gfx_SetTextScale(1, 1);
        round_rect(60, 130, 200, 8, 4, C_PILL);
        if (pct > 0) round_rect(60, 130, 8 + pct * 192 / 100, 8, 4, C_GREEN);
        text_centered(steps[step], 148, C_DIM);
        text_centered("made by Ethan Paschkes", 224, C_DIM);
        gfx_SwapDraw();
        if (pct >= 100 && elapsed > 2 * TICKS_PER_SECOND + TICKS_PER_SECOND / 3) return;
        if (next_key() == K_ENTER) return;
    }
}

/* ---------------------------------------------------------------- photo import: gallery, then the "model" reads the photo */

/* returns the chosen photo, or -1 if cancelled */
static int gallery(void)
{
    int sel = 0, i;
    forget_keys();
    for (;;) {
        int key;
        gfx_SetColor(C_BG);
        gfx_FillRectangle(0, 0, SCREEN_W, 240);
        text_centered("Photos", 24, C_TEXT);
        text_centered("Choose a photo to send to CalcGPT", 38, C_DIM);
        for (i = 0; i < N_PHOTOS; i++) {
            int x = 14 + i * 102;
            if (i == sel) {
                gfx_SetColor(C_GREEN);
                gfx_FillRectangle(x - 3, 66, 96, 70);
            }
            draw_photo(i, x, 69 + (64 - photo_height(90)) / 2, 90);
            text_at(PHOTOS[i].taken, x + (90 - 8 * (int)strlen(PHOTOS[i].taken)) / 2, 142, i == sel ? C_TEXT : C_DIM);
        }
        top_bar();
        text_centered("Arrows pick  Enter import  Clear back", 226, C_DIM);
        gfx_SwapDraw();
        do {
            key = next_key();
        } while (key < 0);
        if (key == K_CLEAR) return -1;
        if (key == K_LEFT && sel > 0) sel--;
        if (key == K_RIGHT && sel < N_PHOTOS - 1) sel++;
        if (key == K_ENTER) return sel;
    }
}

/* a scan sweeps over the photo, then a box appears around the equation with a label */
static void analyze(int idx)
{
    const int x = 16, y = 24, w = 288, h = 172;
    struct photo_box b = photo_layout(idx, x, y, w);
    uint32_t start = timer_Get(1);
    const uint32_t sweep = TICKS_PER_SECOND * 3 / 2, total = sweep + TICKS_PER_SECOND * 5 / 4;
    char label[40];
    for (;;) {
        uint32_t t = timer_Get(1) - start;
        int pct = t >= sweep ? 100 : (int)(t * 100u / sweep), scan_y = y + (h * pct) / 100, i;
        gfx_SetColor(C_BG);
        gfx_FillRectangle(0, 0, SCREEN_W, 240);
        gfx_ScaledSprite_NoClip(photo_at_size(idx, PHOTO_W, PHOTO_H), x, y, 2, 2);
        viewfinder(x - 4, y - 4, w + 8, h + 8, C_TEXT);
        if (t < sweep) {
            for (i = 0; i < 12; i++) {   /* the scan line with a fading trail behind it */
                gfx_SetColor(i < 2 ? C_SCAN : C_GREEN);
                if (i < 2 || i % 3 == 0) gfx_HorizLine(x, scan_y - i, w);
            }
            text_centered("Analyzing image...", 204, C_DIM);
        } else {
            int bx = b.text_x - 6, by = b.text_y - 6, bw = b.text_w + 12, bh = b.text_h + 12;
            gfx_SetColor(C_SCAN);
            gfx_Rectangle(bx, by, bw, bh);
            gfx_Rectangle(bx - 1, by - 1, bw + 2, bh + 2);
            gfx_FillRectangle(bx - 1, by - 14, 104, 13);
            text_at("equation  98%", bx + 1, by - 12, C_BLACK);
            strcpy(label, "Recognized: ");
            strncat(label, PHOTOS[idx].ocr, 20);
            text_centered(label, 204, C_TEXT);
        }
        top_bar();
        gfx_SwapDraw();
        if (t >= total) return;
        next_key();
    }
}

static void import_photo(void)
{
    int idx = gallery();
    if (idx < 0) return;
    analyze(idx);
    add_message(ROLE_PHOTO, "", idx);
    assistant_says(PHOTOS[idx].answer, 1100, 70);
}

/* ---------------------------------------------------------------- the main loop */

static void new_chat(void)
{
    n_msgs = 0;
    streaming = -1;
    scroll_px = 0;
    input_len = 0;
    input[0] = 0;
}

int main(void)
{
    bool quit = false, dirty = true;
    uint32_t last_draw = 0;

    gfx_Begin();
    set_palette();
    gfx_SetDrawBuffer();
    gfx_SetTextBGColor(C_BG);
    gfx_SetTextTransparentColor(C_BG);
    timer_Enable(1, TIMER_32K, TIMER_NOINT, TIMER_UP);
    timer_Set(1, 0);
    splash();
    forget_keys();
    while (!quit) {
        int key = next_key();
        uint32_t now = timer_Get(1);
        if (key >= 0) {
            dirty = true;
            if (key == K_CLEAR) {
                if (input_len) { input_len = 0; input[0] = 0; }
                else quit = true;
            } else if (key == K_ENTER) {
                if (streaming >= 0) {                         /* a second Enter finishes the answer at once */
                    msgs[streaming].shown = (int)strlen(msgs[streaming].text);
                    streaming = -1;
                } else if (input_len) {
                    add_message(ROLE_USER, input, 0);
                    assistant_says(script_reply(input), 900, 45);
                    input_len = 0;
                    input[0] = 0;
                }
                typing_used_other_key(&ty);
            } else if (key == K_DEL) {
                if (input_len) input[--input_len] = 0;
                typing_used_other_key(&ty);
            } else if (key == K_YEQU) {
                import_photo();
                forget_keys();
            } else if (key == K_GRAPH) {
                new_chat();
            } else if (key == K_UP) {
                scroll_px += 20;
            } else if (key == K_DOWN) {
                scroll_px = scroll_px > 20 ? scroll_px - 20 : 0;
            } else {
                char ch = 0;
                if (typing_key(&ty, key, &ch) == TY_CHAR && input_len < INPUT_MAX) {
                    input[input_len++] = ch;
                    input[input_len] = 0;
                }
            }
        }
        advance_stream();
        if (streaming >= 0 || (now - last_draw) > TICKS_PER_SECOND / 4) dirty = true;   /* animations and the blinking cursor */
        if (dirty) {
            dirty = false;
            last_draw = now;
            draw_chat();
        }
    }
    gfx_End();
    return 0;
}
