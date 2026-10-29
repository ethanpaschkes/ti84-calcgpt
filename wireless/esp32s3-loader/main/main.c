/* The box: an ESP32-S3 as USB HOST with its own power, plugged into any TI-84 Plus CE.
 *
 *   calculator at its home screen  -> loader.c puts CALC, CALCGPT, SOLVER, their libraries and (OS 5.5+) the jailbreak on it
 *   calculator running CALC/CALCGPT -> link_cdc.c + the bridge (../../esp32s3-bridge/main/bridge.c, the same code as the
 *                                      device-mode board) carry its messages over Wi-Fi to the CalcGPT server or calcnet
 *
 * Wiring: the board's USB pins (GPIO19 = D-, GPIO20 = D+) go to a USB-A socket that also gets 5 V and ground (../ESP_SETUP.md);
 * the calculator is on a normal mini-USB (B) cable, and the board's 5 V comes from a battery or power bank on the 5V/GND pins,
 * never from its USB-C port marked USB (that port shares GPIO19/20). Settings are the bridge's main/secrets.h. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"
#include "bridge.h"
#include "calclink.h"
#include "loader.h"

static const char *TAG = "calcbox";

static void usb_library_task(void *arg)
{
    for (;;) {
        uint32_t flags;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) usb_host_device_free_all();
    }
}

void app_main(void)
{
    const usb_host_config_t host = {.skip_phy_setup = false, .intr_flags = ESP_INTR_FLAG_LEVEL1};
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(usb_host_install(&host));
    xTaskCreate(usb_library_task, "usb_lib", 4096, NULL, 10, NULL);
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));
    loader_start();      /* calculators at their home screen */
    calclink_start();    /* calculators running a program with a serial link */
    bridge_start();
    ESP_LOGI(TAG, "waiting for a calculator: at its home screen it gets CALC and friends; running CALC or CALCGPT it is bridged");
}
