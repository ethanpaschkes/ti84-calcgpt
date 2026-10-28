/* CALCGPT: a chat on the calculator. A language model runs on the Mac (mac/calcgptd.py, through Ollama); you type on the
 * calculator and the answer streams onto its screen. The messages are described at the top of calcgptd.py.
 *
 * Keys: type your question (Alpha switches to letters, 2nd is a one-shot shift), Enter sends, Up/Down scroll,
 * Window/Zoom page up/down, Del erases, Graph starts a new chat, Y= stops an answer, Trace picks the AI model,
 * Clear erases the line then quits. */
#include <graphx.h>
#include <keypadc.h>
#include <srldrvce.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/timers.h>
#include "chatmodel.h"
#include "typing.h"

#define SCREEN_W 320
#define HEAD_H 14
#define BODY_Y 18
#define ROW_H 10
#define MARGIN 8
#define FOOT_Y 228

/* colours, as palette numbers */
enum { C_BLACK, C_LINK, C_GRAY, C_RED, C_GREEN, C_LINE, C_SEL, C_GBLUE, C_GRED, C_GYELLOW, C_GGREEN, C_WHITE = 255 };

/* keys: number = group * 8 + bit, as in typing_tables.h */
#define K(g, b) ((g) * 8 + (b))
#define K_GRAPH K(1, 0)
#define K_TRACE K(1, 1)
#define K_ZOOM K(1, 2)
#define K_WINDOW K(1, 3)
#define K_YEQU K(1, 4)
#define K_DEL K(1, 7)
#define K_ENTER K(6, 0)
#define K_CLEAR K(6, 6)
#define K_DOWN K(7, 0)
#define K_UP K(7, 3)

static srl_device_t srl;
static bool has_srl;
static uint8_t srl_buf[4096];
static uint8_t in_buf[2048];

static struct chat chat;
static struct typing ty;
static char input[CHAT_INPUT_MAX + 1];
static int input_len;
static bool dirty = true;
static bool quit;
static bool menu_open;       /* the model menu is showing */
static int menu_sel;
static bool asked_models;    /* the list of models was asked for since the Mac link came up */

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
        asked_models = false;
        dirty = true;
    }
    return USB_SUCCESS;
}

/* ---- requests to the Mac ---- */

static void send_bytes(const uint8_t *data, int n)
{
    if (has_srl) srl_Write(&srl, data, (size_t)n);
}

static void send_cmd(char cmd)
{
    uint8_t m = (uint8_t)cmd;
    send_bytes(&m, 1);
}

static void send_question(const char *s)
{
    uint8_t msg[2 + CHAT_INPUT_MAX];
    int n = (int)strlen(s);
    msg[0] = 'Q';
    msg[1] = (uint8_t)n;
    memcpy(msg + 2, s, (size_t)n);
    send_bytes(msg, 2 + n);
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

static void draw_header(void)
{
    gfx_SetColor(C_LINE);
    gfx_HorizLine(0, HEAD_H, SCREEN_W);
    text_at("CalcGPT", 6, 3, C_BLACK);
    if (chat.nmodels > 0) {
        const char *name = chat.models[chat.model_sel];
        text_at(name, SCREEN_W - 6 - 8 * (int)strlen(name), 3, C_GRAY);
    }
}

static void draw_welcome(void)
{
    static const uint8_t letter_color[7] = {C_GBLUE, C_GRED, C_GYELLOW, C_GBLUE, C_GGREEN, C_GRED, C_GBLUE};
    static const char name[] = "CalcGPT";
    char one[2] = {0, 0};
    int i;
    gfx_SetTextScale(3, 3);
    for (i = 0; i < 7; i++) {
        one[0] = name[i];
        text_at(one, 76 + i * 24, 50, letter_color[i]);
    }
    gfx_SetTextScale(1, 1);
    text_at("Ask anything. Trace picks the AI model.", 24, 90, C_GRAY);
    text_at("Try:  what is sin 30?", 24, 112, C_BLACK);
    text_at("      explain the law of sines", 24, 124, C_BLACK);
    text_at("      plan my study week", 24, 136, C_BLACK);
    text_at("Alpha = letters, 2nd = capitals/symbols", 24, 156, C_GRAY);
    text_at("made by Ethan Paschkes", 24, 170, C_GRAY);
    if (!has_srl) text_at("Not connected: run calcgptd.py on the Mac", 18, 184, C_RED);
}

static void draw_chat(void)
{
    int i;
    for (i = 0; i < CHAT_VIEW_ROWS && chat.top + i < chat.nrows; i++) {
        const struct chat_row *r = &chat.rows[chat.top + i];
        char buf[CHAT_COLS + 1];
        int y = BODY_Y + i * ROW_H, x = MARGIN;
        memcpy(buf, chat.buf + r->start, r->len);
        buf[r->len] = 0;
        if (r->role == ROLE_USER) {
            if (r->first) text_at(">", MARGIN, y, C_GBLUE);
            x = MARGIN + 16;
            text_at(buf, x, y, C_LINK);
        } else {
            text_at(buf, x, y, C_BLACK);
        }
    }
    if (chat.nrows > CHAT_VIEW_ROWS) {
        int track = CHAT_VIEW_ROWS * ROW_H, thumb = track * CHAT_VIEW_ROWS / chat.nrows;
        int pos = (track - thumb) * chat.top / (chat.nrows - CHAT_VIEW_ROWS);
        gfx_SetColor(C_LINE);
        gfx_FillRectangle(SCREEN_W - 5, BODY_Y, 3, track);
        gfx_SetColor(C_LINK);
        gfx_FillRectangle(SCREEN_W - 5, BODY_Y + pos, 3, thumb < 4 ? 4 : thumb);
    }
}

/* a rounded box for the line being typed */
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

static void draw_input(void)
{
    int shown = input_len > 30 ? input_len - 30 : 0;
    pill(6, 201, 314, 20, chat.busy ? C_LINE : C_GBLUE);
    text_at(input + shown, 20, 207, C_BLACK);
    if (!chat.busy) {
        gfx_SetColor(C_BLACK);
        gfx_VertLine(20 + (input_len - shown) * 8, 204, 11);
    }
    text_at(ty.alpha ? (ty.shift ? "ABC" : "abc") : (ty.shift ? "^" : "123"), 284, 207, C_GRAY);
}

/* the list of AI models, drawn over the chat */
static void draw_menu(void)
{
    const int x = 30, w = 260, row = 16;
    int n = chat.nmodels > 0 ? chat.nmodels : 1, h = 22 + n * row + 18, y = 34, i;
    gfx_SetColor(C_BLACK);
    gfx_FillRectangle(x - 1, y - 1, w + 2, h + 2);
    gfx_SetColor(C_WHITE);
    gfx_FillRectangle(x, y, w, h);
    text_at("Choose the AI model", x + 8, y + 6, C_BLACK);
    if (chat.nmodels == 0) {
        text_at("none yet: is calcgptd.py on?", x + 8, y + 24, C_RED);
    }
    for (i = 0; i < chat.nmodels; i++) {
        int ry = y + 20 + i * row;
        if (i == menu_sel) {
            gfx_SetColor(C_SEL);
            gfx_FillRectangle(x + 4, ry - 2, w - 8, row);
        }
        text_at(i == menu_sel ? ">" : " ", x + 8, ry, C_GBLUE);
        text_at(chat.models[i], x + 24, ry, C_BLACK);
        if (i == chat.model_sel) text_at("(in use)", x + w - 8 - 8 * 8, ry, C_GREEN);
    }
    text_at("Up/Down  Enter: use  Clear: back", x + 8, y + h - 14, C_GRAY);
}

static void redraw(void)
{
    gfx_FillScreen(C_WHITE);
    draw_header();
    if (chat.nrows == 0 && !chat.busy) draw_welcome();
    else draw_chat();
    draw_input();
    gfx_SetColor(C_LINE);
    gfx_HorizLine(0, FOOT_Y - 3, SCREEN_W);
    if (chat.status[0]) text_at(chat.status, 4, FOOT_Y, C_RED);
    else if (chat.busy) text_at("Answering...   Y=: stop", 4, FOOT_Y, C_RED);
    else text_at("Trace: model  Graph: new  Clear: quit", 4, FOOT_Y, C_GRAY);
    if (menu_open) draw_menu();
    gfx_SwapDraw();
}

/* ---- keys ---- */

static void handle_menu_key(int key)
{
    if (key == K_UP && menu_sel > 0) {
        menu_sel--;
    } else if (key == K_DOWN && menu_sel < chat.nmodels - 1) {
        menu_sel++;
    } else if (key == K_ENTER) {
        if (chat.nmodels > 0 && menu_sel != chat.model_sel) {
            uint8_t m[2] = {'M', (uint8_t)menu_sel};
            send_bytes(m, 2);
        }
        menu_open = false;
    } else if (key == K_CLEAR || key == K_TRACE) {
        menu_open = false;
    }
}

static void handle_key(int key)
{
    dirty = true;
    if (menu_open) {
        handle_menu_key(key);
        return;
    }
    if (key == K_TRACE) {
        if (chat.nmodels == 0) send_cmd('R');
        menu_sel = chat.model_sel;
        menu_open = true;
    } else if (key == K_ENTER) {
        if (input_len && !chat.busy && has_srl) {
            chat_user(&chat, input);
            send_question(input);
            input_len = 0;
            input[0] = 0;
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
    } else if (key == K_GRAPH) {
        send_cmd('N');
        chat_clear(&chat);
    } else if (key == K_YEQU) {
        send_cmd('S');
    } else if (key == K_UP) {
        chat_scroll(&chat, -1, CHAT_VIEW_ROWS);
    } else if (key == K_DOWN) {
        chat_scroll(&chat, 1, CHAT_VIEW_ROWS);
    } else if (key == K_WINDOW) {
        chat_scroll(&chat, -(CHAT_VIEW_ROWS - 1), CHAT_VIEW_ROWS);
    } else if (key == K_ZOOM) {
        chat_scroll(&chat, CHAT_VIEW_ROWS - 1, CHAT_VIEW_ROWS);
    } else {
        char ch = 0;
        int r = typing_key(&ty, key, &ch);
        if (r == TY_CHAR && input_len < CHAT_INPUT_MAX) {
            input[input_len++] = ch;
            input[input_len] = 0;
        }
    }
}

static bool repeats(int key)
{
    return key == K_UP || key == K_DOWN || key == K_DEL;
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
    chat_init(&chat);
    if (usb_Init(handle_usb_event, NULL, srl_GetCDCStandardDescriptors(), USB_DEFAULT_INIT_FLAGS) != USB_SUCCESS) {
        usb_Cleanup();
        gfx_End();
        return 1;
    }

    while (!quit) {
        int g, b, n;
        uint32_t now;

        usb_HandleEvents();
        if (has_srl && !asked_models) {
            send_cmd('R');                 /* the Mac answers with its list of models */
            asked_models = true;
        }
        if (has_srl) {
            n = srl_Read(&srl, in_buf, sizeof(in_buf));
            if (n > 0) chat_feed(&chat, in_buf, n);
        }
        if (chat.changed) {
            chat.changed = 0;
            dirty = true;
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
            chat_layout(&chat, CHAT_VIEW_ROWS);
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
