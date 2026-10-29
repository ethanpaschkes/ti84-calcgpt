#include "role.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "box_pins.h"

static const char *TAG = "role";

#define WANT_DEVICE 0xCA1CDE51u   /* "after this restart, be the device" */
#define WANT_HOST 0xCA1C4057u     /* "after this restart, be the host" */
RTC_NOINIT_ATTR static uint32_t next_role;   /* survives a software restart, not a power loss */

static void pins_setup(void)
{
    gpio_config_t out = {.pin_bit_mask = (1ULL << PIN_VBUS_OUT_EN) | (1ULL << PIN_ID_LOW), .mode = GPIO_MODE_OUTPUT};
    gpio_config_t in = {.pin_bit_mask = 1ULL << PIN_VBUS_SENSE, .mode = GPIO_MODE_INPUT, .pull_down_en = GPIO_PULLDOWN_ENABLE};
    gpio_config(&out);
    gpio_config(&in);
}

/* the box is the host: the calculator is the device (ID floating) and gets 5 V from the box */
static void be_host(void)
{
    gpio_set_level(PIN_ID_LOW, 0);
    gpio_set_level(PIN_VBUS_OUT_EN, 1);
}

/* the calculator is the host: ID grounded, the box's 5 V off (the calculator supplies the cable) */
static void be_device(void)
{
    gpio_set_level(PIN_VBUS_OUT_EN, 0);
    gpio_set_level(PIN_ID_LOW, 1);
}

int role_decide(void)
{
    uint32_t asked;
    if (!BOX_HW) return ROLE_HOST;
    pins_setup();
    asked = next_role;
    next_role = 0;
    if (asked == WANT_DEVICE) {
        be_device();
        ESP_LOGI(TAG, "device: handed over to the calculator");
        return ROLE_DEVICE;
    }
    if (asked != WANT_HOST && gpio_get_level(PIN_VBUS_SENSE)) {
        be_device();
        ESP_LOGI(TAG, "device: the calculator is powering the cable");
        return ROLE_DEVICE;
    }
    be_host();
    ESP_LOGI(TAG, "host: powering the cable (battery)");
    return ROLE_HOST;
}

void role_hand_over(void)
{
    if (!BOX_HW) return;
    ESP_LOGI(TAG, "handing the host role to the calculator (restart in 1 s)");
    be_device();
    vTaskDelay(pdMS_TO_TICKS(1000));   /* let the log line leave and the cable settle */
    next_role = WANT_DEVICE;
    esp_restart();
}

static void button_task(void *arg)
{
    gpio_config_t btn = {.pin_bit_mask = 1ULL << PIN_BOOT_BUTTON, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE};
    gpio_config(&btn);
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (gpio_get_level(PIN_BOOT_BUTTON) != 0) continue;
        vTaskDelay(pdMS_TO_TICKS(50));
        if (gpio_get_level(PIN_BOOT_BUTTON) != 0) continue;
        ESP_LOGI(TAG, "BOOT pressed: becoming the host again (install / start CALC)");
        next_role = WANT_HOST;
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_restart();
    }
}

void role_watch_button(void)
{
    if (!BOX_HW) return;
    xTaskCreate(button_task, "role_button", 2048, NULL, 2, NULL);
}
