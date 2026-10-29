/* The device-mode bridge board: see bridge.c for what it does and how it is wired. */
#include "esp_err.h"
#include "nvs_flash.h"
#include "bridge.h"
#include "calclink.h"

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    calclink_use(&link_jtag);
    calclink_start();
    bridge_start();
}
