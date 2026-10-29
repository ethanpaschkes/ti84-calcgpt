/* calclink.h with the board as the USB HOST: a calculator program using the srldrvce library (CALC, CALCGPT, REELS, ...) shows
 * up as a USB serial device (CDC-ACM, 16C0:05E1); this opens it, feeds its bytes to the bridge and writes the bridge's bytes back.
 * The USB host library and the CDC-ACM host driver are installed by main.c (the loader shares them). */
#include "calclink.h"
#include <stdbool.h>
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "usb/cdc_acm_host.h"

static const char *TAG = "link_cdc";

#define SRL_VID 0x16C0
#define SRL_PID 0x05E1

static StreamBufferHandle_t from_calc;
static cdc_acm_dev_hdl_t dev;
static volatile bool connected;

static bool data_cb(const uint8_t *data, size_t len, void *arg)
{
    if (xStreamBufferSend(from_calc, data, len, pdMS_TO_TICKS(20)) != len) ESP_LOGW(TAG, "dropped %u bytes from the calculator", (unsigned)len);
    return true;
}

static void event_cb(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    if (event->type == CDC_ACM_HOST_DEVICE_DISCONNECTED) {
        connected = false;
        cdc_acm_host_close(event->data.cdc_hdl);
        dev = NULL;
        ESP_LOGI(TAG, "the calculator program ended or the calculator was unplugged");
    } else if (event->type == CDC_ACM_HOST_ERROR) {
        ESP_LOGW(TAG, "USB error %d", event->data.error);
    }
}

/* waits for a calculator program to appear as a serial device, opens it, and waits again when it goes */
static void open_task(void *arg)
{
    const cdc_acm_host_device_config_t cfg = {.connection_timeout_ms = 1000, .out_buffer_size = 512, .in_buffer_size = 2048,
                                              .event_cb = event_cb, .data_cb = data_cb, .user_arg = NULL};
    for (;;) {
        cdc_acm_dev_hdl_t opened = NULL;
        if (connected) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        if (cdc_acm_host_open(SRL_VID, SRL_PID, 0, &cfg, &opened) != ESP_OK) continue;   /* the 1 s timeout passed: try again */
        {
            cdc_acm_line_coding_t coding = {.dwDTERate = 115200, .bCharFormat = 0, .bParityType = 0, .bDataBits = 8};
            cdc_acm_host_line_coding_set(opened, &coding);              /* what a computer's serial port would set */
            cdc_acm_host_set_control_line_state(opened, true, true);
        }
        xStreamBufferReset(from_calc);
        dev = opened;
        connected = true;
        ESP_LOGI(TAG, "a calculator program is connected over USB");
    }
}

static void cdc_start(void)
{
    from_calc = xStreamBufferCreate(8192, 1);
    xTaskCreate(open_task, "link_cdc", 4096, NULL, 7, NULL);
}

static int cdc_read(uint8_t *buf, size_t n, TickType_t wait)
{
    return (int)xStreamBufferReceive(from_calc, buf, n, wait);
}

static int cdc_write(const uint8_t *buf, size_t n, TickType_t wait)
{
    cdc_acm_dev_hdl_t d = dev;
    if (!connected || !d) return -1;
    return cdc_acm_host_data_tx_blocking(d, buf, n, wait * portTICK_PERIOD_MS) == ESP_OK ? (int)n : -1;
}

static bool cdc_connected(void)
{
    return connected;
}

const struct calclink_ops link_cdc = {cdc_start, cdc_read, cdc_write, cdc_connected};
