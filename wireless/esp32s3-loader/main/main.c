/* The box: an ESP32-S3 with a small battery, plugged into any TI-84 Plus CE (../../BOX_DESIGN.md).
 *
 *   as the USB host (on battery)   loader.c puts CALC, CALCGPT, SOLVER, their libraries and (OS 5.5+) the jailbreak on the
 *                                  calculator and starts CALC; then role.c hands the host role to the calculator.
 *   as the USB device (calculator powers the box)   the bridge (../../esp32s3-bridge/main/bridge.c, the same code as the
 *                                  device-mode board) carries CALC's and CALCGPT's messages over Wi-Fi to the CalcGPT server.
 *
 * On the bare DevKitC-1 (BOX_HW 0) the box is always the host: the bridge then talks to CALC through link_cdc.c, and the board
 * must be powered from its UART USB-C port or 5V pin, never from the USB-C port marked USB (that port shares GPIO19/20, the
 * calculator link). Settings are the bridge's main/secrets.h. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"
#include "bridge.h"
#include "calclink.h"
#include "loader.h"
#include "role.h"

static const char *TAG = "calcbox";

static void usb_library_task(void *arg)
{
    for (;;) {
        uint32_t flags;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) usb_host_device_free_all();
    }
}

static void host_mode(void)
{
    const usb_host_config_t host = {.skip_phy_setup = false, .intr_flags = ESP_INTR_FLAG_LEVEL1};
    ESP_ERROR_CHECK(usb_host_install(&host));
    xTaskCreate(usb_library_task, "usb_lib", 4096, NULL, 10, NULL);
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));
    loader_start();                 /* calculators at their home screen */
    calclink_use(&link_cdc);        /* calculators running a program with a serial link (DevKitC-1 only: the box hands over) */
    ESP_LOGI(TAG, "host: waiting for a calculator: at its home screen it gets CALC and friends, then CALC is started");
}

static void device_mode(void)
{
    calclink_use(&link_jtag);
    role_watch_button();
    ESP_LOGI(TAG, "device: waiting for CALC or CALCGPT on the calculator to set up the USB link");
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    if (role_decide() == ROLE_HOST) host_mode();
    else device_mode();
    calclink_start();
    bridge_start();
}
