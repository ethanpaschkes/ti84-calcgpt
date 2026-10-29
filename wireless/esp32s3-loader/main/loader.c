/* The loader: when a TI-84 Plus CE is plugged in at its home screen (its own USB device, 0451:E008), put on it whatever it is
 * missing of CALCGPT, SOLVER and the C libraries they need, using TI's link protocol (dusb.c), like TI Connect CE does.
 *
 * C programs only run on OS 5.5 and later after a jailbreak. On OS 5.8.5 the loader can set up arTIfiCE v3
 * (github.com/YvanTT/arTIfiCE): it sends its program to RAM, then waits for the user to open the Inequalz app (Y= screen) and
 * press the board's BOOT button, and sends the trigger variable. The calculator then installs the AsmHook2 app and restarts;
 * after that, launching AsmHook2 once makes C programs run from the PRGM menu.
 *
 * The files are built into the firmware (main/payload, see CMakeLists.txt). A variable the calculator already has is left alone. */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "dusb.h"
#include "loader.h"

static const char *TAG = "loader";

#define BOOT_BUTTON GPIO_NUM_0
#define BUTTON_WAIT_MS (5 * 60 * 1000)
#define MAX_VARS 128

/* the files, built into the firmware */
#define PAYLOAD(sym) extern const uint8_t _binary_##sym##_start[], _binary_##sym##_end[];
PAYLOAD(CALCGPT_8xp)
PAYLOAD(SOLVER_8xp)
PAYLOAD(clibs_8xg)
PAYLOAD(arTIfiCE_8xp)
PAYLOAD(INEQUVAR_8xv)

typedef struct {
    const uint8_t *start, *end;
} file_t;

static const file_t programs[] = {
    {_binary_CALCGPT_8xp_start, _binary_CALCGPT_8xp_end},
    {_binary_SOLVER_8xp_start, _binary_SOLVER_8xp_end},
    {_binary_clibs_8xg_start, _binary_clibs_8xg_end},   /* LibLoad, GRAPHX, USBDRVCE, SRLDRVCE, ... */
};
static const file_t jailbreak_program = {_binary_arTIfiCE_8xp_start, _binary_arTIfiCE_8xp_end};
static const file_t jailbreak_trigger = {_binary_INEQUVAR_8xv_start, _binary_INEQUVAR_8xv_end};
#define JAILBREAK_APP "AsmHook2"   /* the app arTIfiCE installs */
#define JAILBREAK_OS_MAJOR 5
#define JAILBREAK_OS_MINOR 8
#define JAILBREAK_OS_PATCH 5

static usb_host_client_handle_t client;
static QueueHandle_t new_devices;
static volatile bool device_gone;

/* ---------------------------------------------------------------- .8xp / .8xv / .8xg files */

/* one variable in a TI-83+/84+ variable file */
typedef struct {
    char name[DUSB_NAME_MAX];
    uint8_t type, version;
    bool archived;
    const uint8_t *data;
    uint32_t size;
} var_t;

/* the variable at *pos in the file (a group file has several); false at the end.
 * File: "**TI83F*" ... data section length (2, little-endian) at 0x35, variables from 0x37, a 2-byte checksum after.
 * Variable: header length (2, 0x0D) | data length (2) | type | name (8, 0-padded) | version | flags (0x80 = archived) |
 *           data length (2) | data */
static bool file_next_var(const file_t *f, size_t *pos, var_t *v)
{
    size_t len = f->end - f->start, end, hdr;
    if (len < 0x37 || memcmp(f->start, "**TI83F*", 8) != 0) return false;
    end = 0x37 + (f->start[0x35] | (f->start[0x36] << 8));
    if (end > len) return false;
    if (*pos < 0x37) *pos = 0x37;
    if (*pos + 17 > end) return false;
    hdr = f->start[*pos] | (f->start[*pos + 1] << 8);
    v->size = f->start[*pos + 2] | (f->start[*pos + 3] << 8);
    v->type = f->start[*pos + 4];
    memcpy(v->name, f->start + *pos + 5, 8);
    v->name[8] = 0;
    v->version = hdr >= 0x0D ? f->start[*pos + 13] : 0;
    v->archived = hdr >= 0x0D && (f->start[*pos + 14] & 0x80);
    v->data = f->start + *pos + 2 + hdr + 2;
    if (v->data + v->size > f->start + end) return false;
    *pos = v->data + v->size - f->start;
    return true;
}

/* ---------------------------------------------------------------- what the calculator has */

typedef struct {
    char names[MAX_VARS][DUSB_NAME_MAX];
    int count;
} have_t;

static void note_var(const char *name, uint8_t type, bool archived, void *arg)
{
    have_t *have = arg;
    if (have->count < MAX_VARS) strlcpy(have->names[have->count++], name, DUSB_NAME_MAX);
}

static bool has(const have_t *have, const char *name)
{
    int i;
    for (i = 0; i < have->count; i++)
        if (strcmp(have->names[i], name) == 0) return true;
    return false;
}

/* sends every variable of the file the calculator does not have yet (or all of them, if always) */
static bool send_file(dusb_t *d, const file_t *f, const have_t *have, bool always)
{
    size_t pos = 0;
    var_t v;
    while (file_next_var(f, &pos, &v)) {
        if (!always && has(have, v.name)) continue;
        ESP_LOGI(TAG, "sending %s (%lu bytes%s)", v.name, (unsigned long)v.size, v.archived ? ", to archive" : "");
        if (!dusb_send_var(d, v.name, v.type, v.version, v.archived, v.data, v.size)) {
            ESP_LOGW(TAG, "could not send %s", v.name);
            return false;
        }
    }
    return true;
}

static bool button_pressed(void)
{
    if (gpio_get_level(BOOT_BUTTON) != 0) return false;
    vTaskDelay(pdMS_TO_TICKS(30));
    return gpio_get_level(BOOT_BUTTON) == 0;
}

/* arTIfiCE v3: its program to RAM, then the trigger once Inequalz is open on the Y= screen (the user presses BOOT) */
static void jailbreak(dusb_t *d, const have_t *have)
{
    int waited = 0;
    var_t v;
    size_t pos = 0;

    if (!file_next_var(&jailbreak_program, &pos, &v)) return;
    if (!has(have, v.name) && !send_file(d, &jailbreak_program, have, true)) return;
    ESP_LOGI(TAG, "jailbreak: on the calculator open APPS > Inequalz, stay on the Y= screen, then press BOOT on the board");
    while (!button_pressed()) {
        if (device_gone || waited >= BUTTON_WAIT_MS) {
            ESP_LOGI(TAG, "jailbreak: no BOOT press, skipped (plug in again to retry)");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        waited += 100;
    }
    if (dusb_hello(d) && send_file(d, &jailbreak_trigger, have, true))
        ESP_LOGI(TAG, "jailbreak: sent; the calculator installs AsmHook2 and restarts. Launch AsmHook2 once from APPS");
}

static void load(usb_device_handle_t dev)
{
    static have_t have;
    dusb_t d;
    uint8_t os[3] = {0};
    size_t i;
    bool needs_jailbreak;

    if (!dusb_open(&d, client, dev)) {
        ESP_LOGW(TAG, "could not open the calculator's link");
        return;
    }
    have.count = 0;
    if (!dusb_hello(&d) || !dusb_os_version(&d, os) || !dusb_list(&d, note_var, &have)) {
        ESP_LOGW(TAG, "the calculator did not answer (is it at the home screen?)");
        dusb_close(&d);
        return;
    }
    if (os[0] < 5) {   /* the 84+ SE (OS 2.x) and 84+ C SE (OS 4.x) share the CE's USB id but cannot run CE programs */
        ESP_LOGW(TAG, "OS %u.%u.%u is not a TI-84 Plus CE: nothing sent", os[0], os[1], os[2]);
        dusb_close(&d);
        return;
    }
    ESP_LOGI(TAG, "TI-84 Plus CE with OS %u.%u.%u and %d variables", os[0], os[1], os[2], have.count);
    for (i = 0; i < sizeof(programs) / sizeof(programs[0]); i++)
        if (!send_file(&d, &programs[i], &have, false)) break;
    if (i == sizeof(programs) / sizeof(programs[0])) ESP_LOGI(TAG, "CALCGPT and SOLVER are on the calculator");

    needs_jailbreak = os[0] > 5 || (os[0] == 5 && os[1] >= 5);
    if (!needs_jailbreak || has(&have, JAILBREAK_APP)) {
        ESP_LOGI(TAG, "%s", needs_jailbreak ? "C programs already run (AsmHook2 is installed)" : "this OS runs C programs as they are");
    } else if (os[0] == JAILBREAK_OS_MAJOR && os[1] == JAILBREAK_OS_MINOR && os[2] == JAILBREAK_OS_PATCH) {
        jailbreak(&d, &have);
    } else {
        ESP_LOGW(TAG, "OS %u.%u.%u needs a jailbreak to run C programs, and the one on this board is for 5.8.5 only", os[0], os[1], os[2]);
    }
    dusb_close(&d);
}

/* ---------------------------------------------------------------- USB */

static void client_event(const usb_host_client_event_msg_t *msg, void *arg)
{
    if (msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) xQueueSend(new_devices, &msg->new_dev.address, 0);
    else if (msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) device_gone = true;
}

/* the client's events (new device, device gone, and the end of every transfer) are handled here */
static void client_task(void *arg)
{
    for (;;) usb_host_client_handle_events(client, portMAX_DELAY);
}

static void loader_task(void *arg)
{
    for (;;) {
        uint8_t addr;
        usb_device_handle_t dev;
        const usb_device_desc_t *desc;

        xQueueReceive(new_devices, &addr, portMAX_DELAY);
        device_gone = false;
        if (usb_host_device_open(client, addr, &dev) != ESP_OK) continue;
        if (usb_host_get_device_descriptor(dev, &desc) == ESP_OK && desc->idVendor == TI_VID && desc->idProduct == TI84CE_PID) {
            ESP_LOGI(TAG, "a calculator at its home screen is plugged in");
            load(dev);
        }
        usb_host_device_close(client, dev);
    }
}

void loader_start(void)
{
    const usb_host_client_config_t cfg = {.is_synchronous = false, .max_num_event_msg = 5,
                                          .async = {.client_event_callback = client_event, .callback_arg = NULL}};
    const gpio_config_t button = {.pin_bit_mask = 1ULL << BOOT_BUTTON, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE};

    gpio_config(&button);
    new_devices = xQueueCreate(4, sizeof(uint8_t));
    ESP_ERROR_CHECK(usb_host_client_register(&cfg, &client));
    xTaskCreate(client_task, "usb_loader_events", 3072, NULL, 9, NULL);
    xTaskCreate(loader_task, "usb_loader", 6144, NULL, 5, NULL);
}
