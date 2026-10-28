/* BROWSER (CalcSearch): a text web browser that runs on the calculator. The Mac runs mac/browserd.py (no window), fetches the
 * pages and sends them as rows of text; everything you see is drawn here. The messages are described at the top of browserd.py.
 *
 * Keys: type to search (Alpha switches to letters, 2nd is a one-shot shift), Enter searches or opens, Up/Down scroll,
 * Left/Right jump between links, Window/Zoom page up/down, Del back, Graph or Y= home, Clear quits. */
#include <graphx.h>
#include <keypadc.h>
#include <srldrvce.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/timers.h>
#include "pagemodel.h"
#include "typing.h"

#define SCREEN_W 320
#define HEAD_H 14
#define TITLE_Y 17
#define BODY_Y 30
#define ROW_H 10
#define MARGIN 8
#define FOOT_Y 228
#define INPUT_MAX 36

/* colours, as palette numbers */
enum { C_BLACK, C_LINK, C_GRAY, C_RED, C_GREEN, C_LINE, C_SEL, C_HEAD, C_GBLUE, C_GRED, C_GYELLOW, C_GGREEN, C_WHITE = 255 };

/* keys: number = group * 8 + bit, as in typing_tables.h */
#define K(g, b) ((g) * 8 + (b))
#define K_GRAPH K(1, 0)
#define K_ZOOM K(1, 2)
#define K_WINDOW K(1, 3)
#define K_YEQU K(1, 4)
#define K_DEL K(1, 7)
#define K_ENTER K(6, 0)
#define K_CLEAR K(6, 6)
#define K_DOWN K(7, 0)
#define K_LEFT K(7, 1)
#define K_RIGHT K(7, 2)
#define K_UP K(7, 3)

static srl_device_t srl;
static bool has_srl;
static uint8_t srl_buf[4096];
static uint8_t in_buf[2048];

static struct pm pm;
static struct typing ty;
static char input[INPUT_MAX + 1];
static int input_len;
static int home_sel = -1;   /* the selected bookmark on the home page, -1 for the search box */
static bool dirty = true;
static bool quit;

static usb_error_t handle_usb_event(usb_event_t event, void *event_data, usb_callback_data_t *callback_data)
{
    usb_error_t err = srl_UsbEventCallback(event, event_data, callback_data);
    if (err != USB_SUCCESS) return err;
    if (event == USB_DEVICE_CONNECTED_EVENT && !(usb_GetRole() & USB_ROLE_DEVICE)) usb_ResetDevice(event_data);
    if (event == USB_HOST_CONFIGURE_EVENT || (event == USB_DEVICE_ENABLED_EVENT && !(usb_GetRole() & USB_ROLE_DEVICE))) {
        usb_device_t device;
        if (has_srl) return USB_SUCCESS;
        device = event == USB_HOST_CONFIGURE_EVENT ? usb_FindDevice(NULL, NULL, USB_SKIP_HUBS) : event_data;
        if (!device) return USB_SUCCESS;
        if (srl_Open(&srl, device, srl_buf, sizeof(srl_buf), SRL_INTERFACE_ANY, 9600) == SRL_SUCCESS) {
            has_srl = true;
            dirty = true;
        }
    }
    if (event == USB_DEVICE_DISCONNECTED_EVENT && (usb_device_t)event_data == srl.dev) {
        srl_Close(&srl);
        has_srl = false;
        dirty = true;
    }
    return USB_SUCCESS;
}

/* ---- requests to the Mac ---- */

static void send_bytes(const uint8_t *data, int n)
{
    if (has_srl) srl_Write(&srl, data, (size_t)n);
}

static void send_text(char cmd, const char *s)
{
    uint8_t msg[2 + PM_COLS + 1];
    int n = (int)strlen(s);
    if (n > PM_COLS) n = PM_COLS;
    msg[0] = (uint8_t)cmd;
    msg[1] = (uint8_t)n;
    memcpy(msg + 2, s, (size_t)n);
    send_bytes(msg, 2 + n);
}

static void send_cmd(char cmd)
{
    uint8_t m = (uint8_t)cmd;
    send_bytes(&m, 1);
}

static void send_link(int id)
{
    uint8_t msg[3] = {'K', (uint8_t)(id & 255), (uint8_t)(id >> 8)};
    send_bytes(msg, 3);
    pm.loading = 1;
    strcpy(pm.status, "Loading...");
    dirty = true;
}

/* ---- drawing ---- */

static void set_palette(void)
{
    gfx_palette[C_BLACK] = gfx_RGBTo1555(25, 25, 25);
    gfx_palette[C_LINK] = gfx_RGBTo1555(25, 85, 200);
    gfx_palette[C_GRAY] = gfx_RGBTo1555(115, 115, 115);
    gfx_palette[C_RED] = gfx_RGBTo1555(200, 40, 40);
    gfx_palette[C_GREEN] = gfx_RGBTo1555(20, 135, 70);
    gfx_palette[C_LINE] = gfx_RGBTo1555(205, 205, 205);
    gfx_palette[C_SEL] = gfx_RGBTo1555(226, 236, 255);
    gfx_palette[C_HEAD] = gfx_RGBTo1555(10, 40, 120);
    gfx_palette[C_GBLUE] = gfx_RGBTo1555(66, 133, 244);
    gfx_palette[C_GRED] = gfx_RGBTo1555(234, 67, 53);
    gfx_palette[C_GYELLOW] = gfx_RGBTo1555(251, 188, 5);
    gfx_palette[C_GGREEN] = gfx_RGBTo1555(52, 168, 83);
    gfx_palette[C_WHITE] = gfx_RGBTo1555(255, 255, 255);
}

static void text_at(const char *s, int x, int y, uint8_t color)
{
    gfx_SetTextFGColor(color);
    gfx_PrintStringXY(s, x, y);
}

/* the characters from..to-1 of s, drawn from column `from` of a row that starts at the left margin */
static void print_part(const char *s, int from, int to, int y, uint8_t color)
{
    char buf[PM_COLS + 1];
    int n = to - from;
    if (n <= 0) return;
    memcpy(buf, s + from, (size_t)n);
    buf[n] = 0;
    text_at(buf, MARGIN + from * 8, y, color);
}

static void draw_header(void)
{
    gfx_SetColor(C_LINE);
    gfx_HorizLine(0, HEAD_H, SCREEN_W);
    text_at("CalcSearch", 6, 3, C_BLACK);
    text_at("made by Ethan Paschkes", SCREEN_W - 6 - 22 * 8, 3, C_GRAY);
}

static void draw_footer(const char *hint)
{
    gfx_SetColor(C_LINE);
    gfx_HorizLine(0, FOOT_Y - 3, SCREEN_W);
    text_at(pm.loading && pm.status[0] ? pm.status : hint, 4, FOOT_Y, pm.loading ? C_RED : C_GRAY);
}

/* a rounded search box */
static void pill(int x0, int y0, int x1, int h, uint8_t border)
{
    int r = h / 2;
    gfx_SetColor(border);
    gfx_FillCircle(x0 + r, y0 + r, r);
    gfx_FillCircle(x1 - r, y0 + r, r);
    gfx_FillRectangle(x0 + r, y0, x1 - x0 - 2 * r, h);
    gfx_SetColor(C_WHITE);
    gfx_FillCircle(x0 + r, y0 + r, r - 1);
    gfx_FillCircle(x1 - r, y0 + r, r - 1);
    gfx_FillRectangle(x0 + r, y0 + 1, x1 - x0 - 2 * r, h - 2);
}

static void draw_home(void)
{
    static const uint8_t letter_color[10] = {C_GBLUE, C_GRED, C_GYELLOW, C_GBLUE, C_GGREEN, C_GRED, C_GBLUE, C_GRED, C_GYELLOW, C_GGREEN};
    static const char name[] = "CalcSearch";
    char one[2] = {0, 0};
    int i, shown;
    const char *tail;

    gfx_SetTextScale(3, 3);
    for (i = 0; i < 10; i++) {
        one[0] = name[i];
        text_at(one, 40 + i * 24, 40, letter_color[i]);
    }
    gfx_SetTextScale(1, 1);
    text_at("search the web from your calculator", 36, 72, C_GRAY);

    pill(40, 92, 280, 24, home_sel < 0 ? C_GBLUE : C_LINE);
    shown = input_len > 24 ? input_len - 24 : 0;
    tail = input + shown;
    text_at(tail, 54, 100, C_BLACK);
    if (home_sel < 0) {
        gfx_SetColor(C_BLACK);
        gfx_VertLine(54 + (input_len - shown) * 8, 98, 11);
    }
    text_at(ty.alpha ? (ty.shift ? "ABC" : "abc") : (ty.shift ? "^" : "123"), 252, 100, C_GRAY);

    for (i = 0; i < pm.nbookmarks && i < 6; i++) {
        if (i == home_sel) {
            gfx_SetColor(C_SEL);
            gfx_FillRectangle(40, 128 + i * 14 - 2, 240, 13);
        }
        text_at(pm.bookmarks[i], 54, 128 + i * 14, i == home_sel ? C_LINK : C_BLACK);
    }
    if (!has_srl) text_at("Not connected: run browserd.py on the Mac", 14, 214, C_RED);
    draw_footer("Enter: search or open   Up/Down: bookmarks");
}

static uint8_t style_color(int style)
{
    switch (style) {
    case PM_BIG: case PM_HEADING: return C_HEAD;
    case PM_RESULT_TITLE: return C_LINK;
    case PM_RESULT_URL: return C_GREEN;
    case PM_SNIPPET: case PM_NOTE: return C_GRAY;
    default: return C_BLACK;
    }
}

static void draw_row(int y, const struct pm_row *r, int sel_id)
{
    int len = (int)strlen(r->text), col = 0, k;
    bool result = r->style == PM_RESULT_TITLE || r->style == PM_RESULT_URL;
    if (result && r->nspans && r->spans[0].id == sel_id) {
        gfx_SetColor(C_SEL);
        gfx_FillRectangle(0, y - 1, SCREEN_W - 6, ROW_H);
    }
    while (col < len) {
        int in_span = -1, end = len;
        for (k = 0; k < r->nspans; k++) {
            if (col >= r->spans[k].start && col < r->spans[k].end) in_span = k;
            else if (r->spans[k].start > col && r->spans[k].start < end) end = r->spans[k].start;
        }
        if (in_span >= 0) {
            bool selected = r->spans[in_span].id == sel_id;
            if (r->spans[in_span].end < end) end = r->spans[in_span].end;
            if (end > len) end = len;
            if (selected && !result) {
                gfx_SetColor(C_SEL);
                gfx_FillRectangle(MARGIN + col * 8 - 1, y - 1, (end - col) * 8 + 2, ROW_H);
            }
            print_part(r->text, col, end, y, result ? style_color(r->style) : C_LINK);
            if (!result) {
                gfx_SetColor(C_LINK);
                gfx_HorizLine(MARGIN + col * 8, y + 8, (end - col) * 8);
            }
        } else {
            print_part(r->text, col, end, y, style_color(r->style));
        }
        col = end;
    }
    if (r->style == PM_BIG) {
        gfx_SetColor(C_LINE);
        gfx_HorizLine(MARGIN, y + 9, len * 8);
    }
}

static void draw_view(void)
{
    int i, sel = pm_selected_id(&pm);
    text_at(pm.title, MARGIN, TITLE_Y, C_GRAY);
    for (i = 0; i < PM_VIEW_ROWS && pm.top + i < pm.nrows; i++) draw_row(BODY_Y + i * ROW_H, &pm.rows[pm.top + i], sel);
    if (pm.nrows > PM_VIEW_ROWS) {
        int track = PM_VIEW_ROWS * ROW_H, thumb = track * PM_VIEW_ROWS / pm.nrows;
        int pos_y = (track - thumb) * pm.top / (pm.nrows - PM_VIEW_ROWS);
        gfx_SetColor(C_LINE);
        gfx_FillRectangle(SCREEN_W - 5, BODY_Y, 3, track);
        gfx_SetColor(C_LINK);
        gfx_FillRectangle(SCREEN_W - 5, BODY_Y + pos_y, 3, thumb < 4 ? 4 : thumb);
    }
    draw_footer(pm.screen == PM_RESULTS ? "Left/Right: result  Enter: open  Del: back" : "Up/Down  Left/Right: link  Enter  Del: back");
}

static void redraw(void)
{
    gfx_FillScreen(C_WHITE);
    draw_header();
    if (pm.screen == PM_HOME) draw_home();
    else draw_view();
    gfx_SwapDraw();
}

/* ---- keys ---- */

static void text_key(int key)
{
    char ch = 0;
    int r = typing_key(&ty, key, &ch);
    if (r == TY_CHAR && input_len < INPUT_MAX) {
        input[input_len++] = ch;
        input[input_len] = 0;
        home_sel = -1;
    }
    if (r != TY_NOTHING) dirty = true;
}

static void handle_key(int key)
{
    dirty = true;
    if (pm.screen == PM_HOME) {
        if (key == K_UP) {
            if (home_sel >= 0) home_sel--;
        } else if (key == K_DOWN) {
            if (home_sel < pm.nbookmarks - 1) home_sel++;
        } else if (key == K_ENTER) {
            if (home_sel >= 0) send_link(home_sel + 1);
            else if (input_len) {
                send_text('Q', input);
                pm.loading = 1;
                strcpy(pm.status, "Searching...");
            }
            typing_used_other_key(&ty);
        } else if (key == K_DEL) {
            if (input_len) input[--input_len] = 0;
            typing_used_other_key(&ty);
        } else if (key == K_CLEAR) {
            if (input_len) {
                input_len = 0;
                input[0] = 0;
            } else {
                quit = true;
            }
        } else {
            text_key(key);
        }
        return;
    }
    if (key == K_UP) pm_scroll(&pm, -1, PM_VIEW_ROWS);
    else if (key == K_DOWN) pm_scroll(&pm, 1, PM_VIEW_ROWS);
    else if (key == K_WINDOW) pm_scroll(&pm, -(PM_VIEW_ROWS - 1), PM_VIEW_ROWS);
    else if (key == K_ZOOM) pm_scroll(&pm, PM_VIEW_ROWS - 1, PM_VIEW_ROWS);
    else if (key == K_LEFT) pm_prev_link(&pm, PM_VIEW_ROWS);
    else if (key == K_RIGHT) pm_next_link(&pm, PM_VIEW_ROWS);
    else if (key == K_ENTER) {
        if (pm_selected_id(&pm)) send_link(pm_selected_id(&pm));
    } else if (key == K_DEL || key == K_CLEAR) {
        send_cmd('X');
    } else if (key == K_GRAPH || key == K_YEQU) {
        send_cmd('G');
    } else {
        dirty = false;
    }
}

static bool repeats(int key)
{
    return key == K_UP || key == K_DOWN || key == K_LEFT || key == K_RIGHT || key == K_DEL;
}

int main(void)
{
    uint8_t prev[8] = {0}, cur[8] = {0};
    int held = -1;
    uint32_t held_since = 0, next_repeat = 0;

    gfx_Begin();
    set_palette();
    gfx_SetDrawBuffer();
    gfx_SetTextBGColor(C_WHITE);
    gfx_SetTextTransparentColor(C_WHITE);
    timer_Enable(1, TIMER_32K, TIMER_NOINT, TIMER_UP);
    timer_Set(1, 0);
    pm_init(&pm);
    if (usb_Init(handle_usb_event, NULL, srl_GetCDCStandardDescriptors(), USB_DEFAULT_INIT_FLAGS) != USB_SUCCESS) {
        usb_Cleanup();
        gfx_End();
        return 1;
    }

    while (!quit) {
        int g, b, n;
        uint32_t now;

        usb_HandleEvents();
        if (has_srl) {
            n = srl_Read(&srl, in_buf, sizeof(in_buf));
            if (n > 0) {
                pm_feed(&pm, in_buf, n);
                if (pm.changed) {
                    pm.changed = 0;
                    dirty = true;
                    if (pm.screen == PM_HOME && home_sel >= pm.nbookmarks) home_sel = -1;
                }
            }
        }
        kb_Scan();
        for (g = 1; g <= 7; g++) cur[g] = kb_Data[g];
        now = timer_Get(1);
        for (g = 1; g <= 7; g++) {
            uint8_t pressed = cur[g] & (uint8_t)~prev[g];
            for (b = 0; b < 8; b++) {
                if (pressed & (1 << b)) {
                    handle_key(K(g, b));
                    held = repeats(K(g, b)) ? K(g, b) : -1;
                    held_since = now;
                    next_repeat = now + 13107;  /* about 0.4 s before a held key repeats */
                }
            }
            prev[g] = cur[g];
        }
        if (held >= 0) {
            if (!(cur[held / 8] & (1 << (held % 8)))) held = -1;
            else if (now >= next_repeat && now - held_since > 13107) {
                handle_key(held);
                next_repeat = now + 2300;      /* then about 14 times a second */
            }
        }
        if (dirty) {
            dirty = false;
            redraw();
        }
    }
    send_cmd('Z');
    {
        int i;
        for (i = 0; i < 400; i++) usb_HandleEvents();  /* let the last message leave before the USB link is shut down */
    }
    usb_Cleanup();
    gfx_End();
    return 0;
}
