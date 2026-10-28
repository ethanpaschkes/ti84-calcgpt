/* REELS: shows the pictures a Mac sends over the USB cable (run mac/reels.py or mac/screenshare.py).
 * The picture messages are understood by viewer.c (the wire format is described in viewer.h).
 *
 * Calculator to Mac:
 *   'S' + 7 bytes         the whole keyboard (kb_Data groups 1 to 7), sent whenever any key goes down or up, and now and then
 *                         as a reminder, so the Mac can follow held keys. Clear quits the program. */
#include <graphx.h>
#include <keypadc.h>
#include <srldrvce.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "viewer.h"

static srl_device_t srl;
static bool has_srl;
static uint8_t srl_buf[8192];
static uint8_t in_buf[4096];

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
        if (srl_Open(&srl, device, srl_buf, sizeof(srl_buf), SRL_INTERFACE_ANY, 9600) == SRL_SUCCESS) has_srl = true;
    }
    if (event == USB_DEVICE_DISCONNECTED_EVENT && (usb_device_t)event_data == srl.dev) {
        srl_Close(&srl);
        has_srl = false;
    }
    return USB_SUCCESS;
}

static void waiting_screen(void)
{
    gfx_FillScreen(255);
    gfx_SetTextFGColor(0);
    gfx_PrintStringXY("REELS", 10, 10);
    gfx_PrintStringXY(has_srl ? "connected. Waiting for video..." : "Plug into the Mac, then run", 10, 40);
    gfx_PrintStringXY("reels.py or screenshare.py", 10, 54);
    gfx_PrintStringXY("Arrows and Enter go to the Mac", 10, 80);
    gfx_PrintStringXY("Clear: quit", 10, 94);
    gfx_PrintStringXY("made by Ethan Paschkes", 10, 224);
}

static void set_palette(const uint8_t *rgb)
{
    int i;
    for (i = 0; i < 16; i++) gfx_palette[i] = gfx_RGBTo1555(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
}

/* a small label over the picture, in colours 250 (black) and 251 (white) that the pictures never use */
static void draw_badge(const char *badge, int badge_len)
{
    char text[16];
    int i;
    if (!badge_len) return;
    memset(text, ' ', 15);
    text[15] = 0;
    for (i = 0; i < badge_len; i++) text[i] = badge[i];
    gfx_SetColor(250);
    gfx_FillRectangle(0, 0, 126, 11);
    gfx_SetTextFGColor(251);
    gfx_SetTextBGColor(250);
    gfx_PrintStringXY(text, 3, 2);
}

int main(void)
{
    static struct viewer_host host = {NULL, set_palette, draw_badge};
    bool quit = false;
    unsigned spin = 0;
    uint8_t last_snap[7] = {0, 0, 0, 0, 0, 0, 0};

    gfx_Begin();
    gfx_SetTextBGColor(255);
    gfx_SetTextTransparentColor(255);
    gfx_palette[250] = gfx_RGBTo1555(0, 0, 0);
    gfx_palette[251] = gfx_RGBTo1555(255, 255, 255);
    if (usb_Init(handle_usb_event, NULL, srl_GetCDCStandardDescriptors(), USB_DEFAULT_INIT_FLAGS) != USB_SUCCESS) {
        usb_Cleanup();
        gfx_End();
        return 1;
    }
    host.vram = gfx_vram;
    viewer_init(&host);
    waiting_screen();

    while (!quit) {
        usb_HandleEvents();
        if (has_srl) {
            int n = srl_Read(&srl, in_buf, sizeof(in_buf));
            if (n > 0) viewer_feed(in_buf, n);
        }
        /* the keyboard is checked every few passes, since scanning it on every pass slows the USB reading */
        if ((++spin & 3) == 0) {
            uint8_t snap[7];
            int i;
            kb_Scan();
            for (i = 0; i < 7; i++) snap[i] = kb_Data[i + 1];
            if (memcmp(snap, last_snap, 7) != 0 || (spin & 0x1FF) == 0) {
                memcpy(last_snap, snap, 7);
                if (has_srl) {
                    uint8_t msg[8];
                    msg[0] = 'S';
                    memcpy(msg + 1, snap, 7);
                    srl_Write(&srl, msg, 8);
                }
            }
            if (snap[5] & kb_Clear) quit = true;
        }
    }
    /* give the last key report time to leave before the USB link is shut down */
    for (spin = 0; spin < 400; spin++) usb_HandleEvents();
    usb_Cleanup();
    gfx_End();
    return 0;
}
