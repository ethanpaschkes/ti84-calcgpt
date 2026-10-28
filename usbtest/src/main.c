/* USBTEST: measures how fast the computer can send data to the calculator over the USB cable,
 * and checks the calculator can send key presses back. This is the first step towards showing
 * a computer's screen (or reels) on the calculator.
 *
 *   1. Run this program on the calculator.
 *   2. Plug the calculator into the Mac with the usual cable, close TI Connect CE.
 *   3. Run mac/usb_speed_test.py on the Mac.
 * Up / Down / Left / Right / Enter send a letter back to the Mac. Clear quits. */
#include <graphx.h>
#include <keypadc.h>
#include <srldrvce.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/timers.h>

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

static void draw(unsigned total, unsigned rate, unsigned best, char last_key, unsigned loops, unsigned reads)
{
    char k[2];
    gfx_FillScreen(255);
    gfx_SetTextFGColor(0);
    gfx_PrintStringXY("USB LINK TEST", 10, 10);
    gfx_SetTextFGColor(has_srl ? 4 : 3);
    gfx_PrintStringXY(has_srl ? "connected" : "waiting for the Mac script...", 10, 30);
    gfx_SetTextFGColor(0);
    gfx_SetTextXY(10, 60);
    gfx_PrintString("received now:  ");
    gfx_PrintUInt(rate, 1);
    gfx_PrintString(" bytes/s");
    gfx_SetTextXY(10, 78);
    gfx_PrintString("best:          ");
    gfx_PrintUInt(best, 1);
    gfx_PrintString(" bytes/s");
    gfx_SetTextXY(10, 96);
    gfx_PrintString("total:         ");
    gfx_PrintUInt(total, 1);
    gfx_PrintString(" bytes");
    gfx_SetTextXY(10, 114);
    gfx_PrintString("loops/s: ");
    gfx_PrintUInt(loops, 1);
    gfx_PrintString("   reads/s: ");
    gfx_PrintUInt(reads, 1);
    gfx_SetTextXY(10, 128);
    gfx_PrintString("bytes per read: ");
    gfx_PrintUInt(reads ? rate / reads : 0, 1);
    gfx_SetTextXY(10, 146);
    gfx_PrintString("320x240 8-bit frames/s: ");
    gfx_PrintUInt(rate / 768, 1);
    gfx_PrintString(".");
    gfx_PrintUInt(rate / 77 % 10, 1);
    k[0] = last_key ? last_key : '-';
    k[1] = 0;
    gfx_PrintStringXY("last key sent to Mac:", 10, 178);
    gfx_PrintStringXY(k, 180, 178);
    gfx_PrintStringXY("Up/Down/Left/Right/Enter send a letter.  Clear quits.", 4, 226);
}

int main(void)
{
    unsigned total = 0, window = 0, rate = 0, best = 0, loops = 0, reads = 0, loops_now = 0, reads_now = 0, spin = 0;
    char last_key = 0;
    bool pu = false, pd = false, pl = false, pr = false, pe = false;
    usb_error_t e;

    gfx_Begin();
    gfx_SetTextBGColor(255);
    gfx_SetTextTransparentColor(255);
    gfx_palette[3] = gfx_RGBTo1555(200, 40, 40);
    gfx_palette[4] = gfx_RGBTo1555(20, 135, 70);
    timer_Enable(1, TIMER_32K, TIMER_NOINT, TIMER_UP);
    timer_Set(1, 0);

    e = usb_Init(handle_usb_event, NULL, srl_GetCDCStandardDescriptors(), USB_DEFAULT_INIT_FLAGS);
    if (e) {
        usb_Cleanup();
        gfx_End();
        return 1;
    }
    draw(0, 0, 0, 0, 0, 0);
    do {
        bool u, d, l, r, en;
        char c = 0;

        usb_HandleEvents();
        /* the keyboard is checked only now and then: scanning it on every pass slows the USB reading */
        if ((++spin & 31) == 0) {
            kb_Scan();
            u = kb_IsDown(kb_KeyUp);
            d = kb_IsDown(kb_KeyDown);
            l = kb_IsDown(kb_KeyLeft);
            r = kb_IsDown(kb_KeyRight);
            en = kb_IsDown(kb_KeyEnter);
            if (u && !pu) c = 'U';
            else if (d && !pd) c = 'D';
            else if (l && !pl) c = 'L';
            else if (r && !pr) c = 'R';
            else if (en && !pe) c = 'E';
            pu = u; pd = d; pl = l; pr = r; pe = en;
        }
        loops_now++;
        if (c) {
            last_key = c;
            if (has_srl) srl_Write(&srl, &c, 1);
        }
        if (has_srl) {
            int n = srl_Read(&srl, in_buf, sizeof(in_buf));
            if (n > 0) {
                total += (unsigned)n;
                window += (unsigned)n;
                reads_now++;
            }
        }
        if (timer_Get(1) >= 32768) {
            timer_Set(1, 0);
            rate = window;
            window = 0;
            loops = loops_now;
            reads = reads_now;
            loops_now = reads_now = 0;
            if (rate > best) best = rate;
            draw(total, rate, best, last_key, loops, reads);
        } else if (c) {
            draw(total, rate, best, last_key, loops, reads);
        }
        if ((spin & 31) == 0 && kb_IsDown(kb_KeyClear)) break;
    } while (1);

    usb_Cleanup();
    gfx_End();
    return 0;
}
