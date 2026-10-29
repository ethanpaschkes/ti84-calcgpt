/* The loader: when a TI-84 Plus CE is plugged in at its home screen (its own USB device, 0451:E008), put on it whatever it is
 * missing of CALC, CALCGPT, SOLVER and the C libraries they need, using TI's link protocol (dusb.c), like TI Connect CE does.
 * Then make sure C programs can run on it (payload.h, run_plan):
 *   OS 5.4 and older   nothing to do.
 *   OS 5.5 to 5.8.4    arTIfiCE v2.1 is sent: a TI-BASIC program "A" the user runs from the prgm menu to start CALC.
 *   OS 5.8.5           arTIfiCE v3: the Inequalz app is sent if missing, then arTIfiCE's program to RAM; the user opens
 *                      Inequalz (Y= screen) and presses the board's BOOT button; the trigger variable is sent; the calculator
 *                      installs the AsmHook2 app and restarts. Launching AsmHook2 once (and after every RAM reset) makes C
 *                      programs run from the prgm menu. Nothing is repeated when AsmHook2 is already there.
 *
 * Then, where C programs can run, it starts CALC on the calculator itself (the DUSB execute command TILP uses).
 *
 * The files are built into the firmware (main/payload, filled by fill_payload.sh, see CMakeLists.txt). A variable or app the
 * calculator already has is left alone. */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "dusb.h"
#include "loader.h"
#include "payload.h"
#include "role.h"

static const char *TAG = "loader";

#define BOOT_BUTTON GPIO_NUM_0
#define BUTTON_WAIT_MS (5 * 60 * 1000)
#define MAX_VARS 128

/* the files, built into the firmware */
#define PAYLOAD(sym) extern const uint8_t _binary_##sym##_start[], _binary_##sym##_end[];
PAYLOAD(clibs_8xg)
PAYLOAD(CALC_8xp)
PAYLOAD(CALCGPT_8xp)
PAYLOAD(SOLVER_8xp)
PAYLOAD(arTIfiCE2_8xp)
PAYLOAD(arTIfiCE_8xp)
PAYLOAD(INEQUVAR_8xv)
PAYLOAD(Inequalz_8ek)
#define FILE(sym) {_binary_##sym##_start, _binary_##sym##_end}

static const file_t programs[] = {
    FILE(clibs_8xg),   /* LibLoad, GRAPHX, USBDRVCE, SRLDRVCE, ... first: the programs need them */
    FILE(CALC_8xp),
    FILE(CALCGPT_8xp),
    FILE(SOLVER_8xp),
};
static const file_t artifice2 = FILE(arTIfiCE2_8xp);           /* v2.1: TI-BASIC program "A" */
static const file_t artifice3_program = FILE(arTIfiCE_8xp);    /* v3: its program, to RAM */
static const file_t artifice3_trigger = FILE(INEQUVAR_8xv);    /* v3: the variable that fires the exploit inside Inequalz */
static const file_t inequalz_app = FILE(Inequalz_8ek);         /* TI's Inequalz app, which v3 needs */
#define HOOK_APP "AsmHook2"   /* the app arTIfiCE v3 installs */

static usb_host_client_handle_t client;
static QueueHandle_t new_devices;
static volatile bool device_gone;

/* ---------------------------------------------------------------- what the calculator has */

typedef struct {
    char names[MAX_VARS][VAR_NAME_MAX];
    int count;
} have_t;

static void note_var(const char *name, uint8_t type, bool archived, void *arg)
{
    have_t *have = arg;
    if (have->count < MAX_VARS) strlcpy(have->names[have->count++], name, VAR_NAME_MAX);
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

/* sends a flash app the calculator does not have */
static bool send_app(dusb_t *d, const file_t *f, const have_t *have)
{
    var_t v;
    if (!file_flash_app(f, &v)) {
        ESP_LOGW(TAG, "the app file built into this firmware is not readable");
        return false;
    }
    if (has(have, v.name)) return true;
    ESP_LOGI(TAG, "sending the %s app (%lu bytes, this takes a while)", v.name, (unsigned long)v.size);
    if (!dusb_send_app(d, v.name, v.data, v.size)) {
        ESP_LOGW(TAG, "could not send the %s app", v.name);
        return false;
    }
    return true;
}

static bool button_pressed(void)
{
    if (gpio_get_level(BOOT_BUTTON) != 0) return false;
    vTaskDelay(pdMS_TO_TICKS(30));
    return gpio_get_level(BOOT_BUTTON) == 0;
}

/* arTIfiCE v3 (OS 5.8.5): Inequalz if missing, its program to RAM, then the trigger once Inequalz is open on the Y= screen
 * (the user presses BOOT) */
static void artifice3(dusb_t *d, const have_t *have)
{
    int waited = 0;
    if (!send_app(d, &inequalz_app, have)) return;
    if (!send_file(d, &artifice3_program, have, false)) return;
    ESP_LOGI(TAG, "jailbreak: on the calculator open APPS > Inequalz, press a key to pass its first screen, stay on the Y= screen, then press BOOT on the board");
    while (!button_pressed()) {
        if (device_gone || waited >= BUTTON_WAIT_MS) {
            ESP_LOGI(TAG, "jailbreak: no BOOT press, skipped (plug in again to retry)");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        waited += 100;
    }
    if (dusb_hello(d) && send_file(d, &artifice3_trigger, have, true))
        ESP_LOGI(TAG, "jailbreak: sent; the calculator installs AsmHook2 and restarts. Open APPS > AsmHook2 once, then prgm > CALC");
}

static void load(usb_device_handle_t dev)
{
    static have_t have;
    dusb_t d;
    uint8_t os[3] = {0};
    size_t i;
    int plan;

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
    if (i == sizeof(programs) / sizeof(programs[0])) ESP_LOGI(TAG, "CALC, CALCGPT, SOLVER and their libraries are on the calculator");

    plan = run_plan(os);
    if (plan == RUN_ARTIFICE2) {
        if (send_file(&d, &artifice2, &have, false)) ESP_LOGI(TAG, "%s", run_plan_text(plan));
    } else if (plan == RUN_ARTIFICE3 && !has(&have, HOOK_APP)) {
        artifice3(&d, &have);
    } else {
        ESP_LOGI(TAG, "%s", run_plan_text(plan));
    }
    /* where C programs run as they are (or the hook is in place), start CALC for the user: the calculator leaves its home
     * screen, CALC turns USB on and this board becomes its serial link (link_cdc.c) */
    if (plan == RUN_NATIVE || (plan == RUN_ARTIFICE3 && has(&have, HOOK_APP))) {
        if (dusb_execute(&d, "CALC", true)) ESP_LOGI(TAG, "CALC started on the calculator");
        else ESP_LOGW(TAG, "could not start CALC from here: press prgm, run CALC");
    }
    dusb_close(&d);
    role_hand_over();   /* the LiPo box: the calculator becomes the host now; the DevKitC-1 stays the host */
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
