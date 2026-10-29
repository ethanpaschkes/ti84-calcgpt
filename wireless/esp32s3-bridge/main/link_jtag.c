/* calclink.h on the chip's built-in USB-Serial/JTAG port: the board is a USB serial device, the calculator the host.
 * That port is on the USB pins from power-on, so the calculator sets it up while the board is still starting and nothing changes
 * after that (a software USB device would have to take the pins over later, and the calculator does not notice that).
 * The console stays on the UART port only, so nothing else is written to this one. */
#include "calclink.h"
#include "driver/usb_serial_jtag.h"
#include "esp_err.h"

void calclink_start(void)
{
    usb_serial_jtag_driver_config_t cfg = {.tx_buffer_size = 4096, .rx_buffer_size = 4096};
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));
}

int calclink_read(uint8_t *buf, size_t n, TickType_t wait)
{
    return usb_serial_jtag_read_bytes(buf, n, wait);
}

int calclink_write(const uint8_t *buf, size_t n, TickType_t wait)
{
    return usb_serial_jtag_write_bytes(buf, n, wait);
}

bool calclink_connected(void)
{
    return usb_serial_jtag_is_connected();
}
