#include "link.h"
#include <srldrvce.h>

static srl_device_t srl;
static bool has_srl;
static bool lost;
static uint8_t srl_buf[4096];

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
        lost = true;
    }
    return USB_SUCCESS;
}

bool link_init(void)
{
    has_srl = false;
    lost = false;
    if (usb_Init(handle_usb_event, NULL, srl_GetCDCStandardDescriptors(), USB_DEFAULT_INIT_FLAGS) != USB_SUCCESS) {
        usb_Cleanup();
        return false;
    }
    return true;
}

void link_poll(void)
{
    usb_HandleEvents();
}

bool link_up(void)
{
    return has_srl;
}

bool link_lost(void)
{
    bool was = lost;
    lost = false;
    return was;
}

void link_send(const uint8_t *data, int n)
{
    if (has_srl && n > 0) srl_Write(&srl, data, (size_t)n);
}

int link_read(uint8_t *buf, int n)
{
    if (!has_srl) return 0;
    return (int)srl_Read(&srl, buf, (size_t)n);
}

void link_close(void)
{
    int i;
    for (i = 0; i < 400; i++) usb_HandleEvents();
    usb_Cleanup();
    has_srl = false;
}
