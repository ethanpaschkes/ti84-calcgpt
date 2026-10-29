/* EXPERIMENT: can a TI-84 Plus CE receive a program from this board as if from another calculator?
 *
 *   Calculator (USB host, 2nd LINK > RECEIVE) --OTG adapter--> this board (USB device that looks like a TI-84 Plus CE)
 *
 * The calculator powers the board through the OTG adapter (wired like the bridge: 5V, GND, GPIO19 = D-, GPIO20 = D+). The board
 * shows itself with the calculator's own USB descriptors (0451:E008, "TI-84 Plus CE", vendor interface 0xFF/0x01 with bulk
 * endpoints 0x81 and 0x02, like a real one), waits to see whether the calculator says anything first, and then sends the 7-byte
 * TI-BASIC program TESTU2U (Disp "HI") with TI's link protocol, as the sending side of a calculator-to-calculator transfer.
 *
 * Everything that happens on USB (connect, set-up, every raw packet both ways in hex) is logged, also as UDP packets to the Mac
 * (port 9999, read by ../boardlog.py into /tmp/board.log), so the Mac shows exactly what the calculator does. Lines logged before
 * the Wi-Fi is up wait in a buffer. Wi-Fi networks and the Mac's addresses come from the bridge's secrets.h. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/message_buffer.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "hal/usb_wrap_ll.h"
#include "soc/usb_wrap_struct.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"
#include "tinyusb.h"
#include "secrets.h"

static const char *TAG = "u2u";

/* ---------------------------------------------------------------- log over Wi-Fi (buffered until it is up) */

#define LOG_PORT 9999
static EventGroupHandle_t wifi_events;
#define WIFI_UP BIT0
static MessageBufferHandle_t log_lines;
static SemaphoreHandle_t log_lock;   /* a message buffer takes one writer at a time */
static int wifi_idx;

static int log_vprintf(const char *fmt, va_list args)
{
    char line[200];
    va_list copy;
    int n;
    va_copy(copy, args);
    n = vsnprintf(line, sizeof(line), fmt, copy);
    va_end(copy);
    if (n > 0 && xSemaphoreTake(log_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
        xMessageBufferSend(log_lines, line, (size_t)n < sizeof(line) ? (size_t)n : sizeof(line) - 1, 0);
        xSemaphoreGive(log_lock);
    }
    return vprintf(fmt, args);
}

/* sends the buffered lines to the Mac's plain address on the current network */
static void log_task(void *arg)
{
    struct sockaddr_in dest = {.sin_family = AF_INET, .sin_port = htons(LOG_PORT)};
    int sock = -1;
    char line[200];
    for (;;) {
        size_t i, n;
        xEventGroupWaitBits(wifi_events, WIFI_UP, pdFALSE, pdTRUE, portMAX_DELAY);
        if (sock < 0) {   /* only once the network is running, and a moment later: packets sent while the Mac's address is
                           * still being looked up are dropped, which lost the first lines of a run */
            vTaskDelay(pdMS_TO_TICKS(1500));
            sock = socket(AF_INET, SOCK_DGRAM, 0);
        }
        for (i = 0; i < ENDPOINT_COUNT; i++)
            if (!ENDPOINTS[i].tls && ENDPOINTS[i].wifi == wifi_idx && inet_aton(ENDPOINTS[i].host, &dest.sin_addr)) break;
        n = xMessageBufferReceive(log_lines, line, sizeof(line), portMAX_DELAY);
        if (i < ENDPOINT_COUNT && sock >= 0) sendto(sock, line, n, 0, (struct sockaddr *)&dest, sizeof(dest));
        vTaskDelay(pdMS_TO_TICKS(2));   /* keep the order of a burst of lines */
    }
}

static void log_hex(const char *what, const uint8_t *p, size_t n)
{
    char hex[3 * 40 + 1];
    size_t i, shown = n < 40 ? n : 40;
    for (i = 0; i < shown; i++) sprintf(hex + 3 * i, "%02X ", p[i]);
    hex[3 * shown] = 0;
    ESP_LOGI(TAG, "%s (%u bytes): %s%s", what, (unsigned)n, hex, n > shown ? "..." : "");
}

/* ---------------------------------------------------------------- Wi-Fi */

static void wifi_try(int idx)
{
    wifi_config_t cfg = {0};
    wifi_idx = idx;
    strlcpy((char *)cfg.sta.ssid, WIFI_NETS[idx].ssid, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, WIFI_NETS[idx].password, sizeof(cfg.sta.password));
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();
}

static void wifi_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        wifi_try(0);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_events, WIFI_UP);
        wifi_try((wifi_idx + 1) % (int)WIFI_NET_COUNT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_events, WIFI_UP);
    }
}

static void wifi_start(void)
{
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* ---------------------------------------------------------------- USB: look like a TI-84 Plus CE */

static const tusb_desc_device_t device_desc = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE, .bcdUSB = 0x0200,
    .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0, .bMaxPacketSize0 = 64,
    .idVendor = 0x0451, .idProduct = 0xE008, .bcdDevice = 0x0220,
    .iManufacturer = 1, .iProduct = 2, .iSerialNumber = 0, .bNumConfigurations = 1,
};

/* configuration 1 | interface 0: vendor 0xFF, subclass 0x01, 2 endpoints | bulk IN 0x81, 64 | bulk OUT 0x02, 64
 * (self-powered, 0 mA, as a real calculator says) */
static const uint8_t config_desc[] = {
    0x09, TUSB_DESC_CONFIGURATION, 32, 0, 1, 1, 0, 0xC0, 0,
    0x09, TUSB_DESC_INTERFACE, 0, 0, 2, 0xFF, 0x01, 0x00, 0,
    0x07, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_BULK, 64, 0, 0,
    0x07, TUSB_DESC_ENDPOINT, 0x02, TUSB_XFER_BULK, 64, 0, 0,
};

static const char *strings[] = {(const char[]){0x09, 0x04}, "Texas Instruments Incorporated", "TI-84 Plus CE"};

void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "USB: the calculator set the board up");
}

void tud_umount_cb(void)
{
    ESP_LOGI(TAG, "USB: the calculator let go of the board");
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    ESP_LOGI(TAG, "USB: the calculator put the link to sleep");
}

void tud_resume_cb(void)
{
    ESP_LOGI(TAG, "USB: the calculator woke the link");
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, const tusb_control_request_t *req)
{
    if (stage == CONTROL_STAGE_SETUP)
        ESP_LOGI(TAG, "USB: vendor request type %02X request %02X value %04X index %04X length %u", req->bmRequestType, req->bRequest,
                 req->wValue, req->wIndex, req->wLength);
    return false;   /* a real calculator has none either */
}

/* show up as the TI-84 Plus CE as early as possible. (A 1.5 s unplug before this, against the chip's built-in USB serial/JTAG
 * port that is on the pins from power-on, made a calculator in RECEIVE give up with "Done".) */
static void usb_start(void)
{
    const tinyusb_config_t usb = {.device_descriptor = &device_desc, .string_descriptor = strings,
                                  .string_descriptor_count = sizeof(strings) / sizeof(strings[0]), .external_phy = false,
                                  .configuration_descriptor = config_desc, .self_powered = false};
    ESP_ERROR_CHECK(tinyusb_driver_install(&usb));
    ESP_LOGI(TAG, "USB: showing up as a TI-84 Plus CE");
}

/* USB On-The-Go session request: a calculator waiting in RECEIVE neither powers the port nor talks first (tested 2026-10-28).
 * A device asks a sleeping host to start by switching its D+ pull-up on for 5-10 ms after a quiet bus ("data-line pulsing").
 * Then show up as the TI-84 Plus CE again a little later, in case the calculator answers by powering up and resetting the bus. */
static void session_request(void)
{
    const usb_wrap_pull_override_vals_t quiet = {.dp_pu = false, .dm_pu = false, .dp_pd = true, .dm_pd = true};
    const usb_wrap_pull_override_vals_t pulse = {.dp_pu = true, .dm_pu = false, .dp_pd = false, .dm_pd = true};
    tud_disconnect();
    usb_wrap_ll_phy_enable_pull_override(&USB_WRAP, &quiet);
    vTaskDelay(pdMS_TO_TICKS(50));
    usb_wrap_ll_phy_enable_pull_override(&USB_WRAP, &pulse);
    vTaskDelay(pdMS_TO_TICKS(8));
    usb_wrap_ll_phy_enable_pull_override(&USB_WRAP, &quiet);
    vTaskDelay(pdMS_TO_TICKS(300));
    usb_wrap_ll_phy_disable_pull_override(&USB_WRAP);
    tud_connect();
}

/* ---------------------------------------------------------------- TI's link protocol, device side (see ../esp32s3-loader/main/dusb.c) */

#define RAW_BUF_SIZE_REQ 1
#define RAW_BUF_SIZE_ALLOC 2
#define RAW_DATA 3
#define RAW_DATA_LAST 4
#define RAW_ACK 5
#define VPKT_PING 0x0001
#define VPKT_RTS 0x000B
#define VPKT_VAR_CNTS 0x000D
#define VPKT_DATA_ACK 0xAA00
#define VPKT_DELAY_ACK 0xBB00
#define VPKT_EOT 0xDD00
#define RAW_MAX 1018
#define VPKT_HDR 6

static uint32_t raw_max = RAW_MAX;
static uint8_t in_raw[5 + RAW_MAX];

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = v >> 24, p[1] = v >> 16, p[2] = v >> 8, p[3] = v;
}

static uint32_t get32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/* set up by a host that is still driving the bus. Without a wire to sense the USB power the board cannot see an unplug; a host
 * that is gone leaves the bus idle, which reads as "suspended". */
static bool calc_here(void)
{
    return tud_mounted() && !tud_suspended();
}

/* the calculator reads when it wants to: queue the bytes on the bulk IN endpoint and wait until they have all gone in */
static bool usb_write(const uint8_t *buf, size_t n, int timeout_ms)
{
    size_t done = 0;
    while (done < n) {
        uint32_t w = tud_vendor_write(buf + done, n - done);
        tud_vendor_write_flush();
        done += w;
        if (done < n) {
            if (timeout_ms <= 0 || !calc_here()) return false;
            vTaskDelay(1);
            timeout_ms--;
        }
    }
    return true;
}

static bool usb_read(uint8_t *buf, size_t n, int timeout_ms)
{
    size_t done = 0;
    while (done < n) {
        uint32_t r = tud_vendor_available() ? tud_vendor_read(buf + done, n - done) : 0;
        done += r;
        if (!r) {
            if (timeout_ms <= 0 || !calc_here()) return false;
            vTaskDelay(1);
            timeout_ms--;
        }
    }
    return true;
}

static bool raw_send(uint8_t type, const uint8_t *data, uint32_t size)
{
    static uint8_t buf[5 + RAW_MAX];
    put32(buf, size);
    buf[4] = type;
    if (size) memcpy(buf + 5, data, size);
    log_hex("board -> calc", buf, 5 + size);
    return usb_write(buf, 5 + size, 3000);
}

/* one raw packet from the calculator into in_raw (header at 0, data at 5) */
static bool raw_recv(uint8_t *type, uint32_t *size, int timeout_ms)
{
    if (!usb_read(in_raw, 5, timeout_ms)) return false;
    *size = get32(in_raw);
    *type = in_raw[4];
    if (*size > RAW_MAX || !usb_read(in_raw + 5, *size, 3000)) {
        ESP_LOGW(TAG, "bad raw packet from the calculator (size %lu)", (unsigned long)*size);
        return false;
    }
    log_hex("calc -> board", in_raw, 5 + *size);
    return true;
}

static bool vpkt_send(uint16_t type, const uint8_t *data, uint32_t size);

/* a whole virtual packet from the calculator (one raw packet) that a calculator would answer: answer it as a real one does
 * (ping -> mode set echoing the last 4 bytes, parameter request -> each "not available", directory request -> end) */
static void answer_vpkt(uint16_t vtype, const uint8_t *data, uint32_t n)
{
    uint8_t reply[2 + 3 * 16];
    uint32_t i, j = 0;
    if (vtype == VPKT_PING && n >= 4) {
        ESP_LOGI(TAG, "answering the calculator's ping with mode set");
        memcpy(reply, data + n - 4, 4);
        vpkt_send(0x0012, reply, 4);
    } else if (vtype == 0x0007 && n >= 2) {
        uint16_t count = (uint16_t)((data[0] << 8) | data[1]);
        ESP_LOGI(TAG, "answering the calculator's parameter request (%u ids) with not available", count);
        reply[j++] = data[0], reply[j++] = data[1];
        for (i = 0; i < count && i < 16 && 2 + 2 * i + 1 < n; i++) reply[j++] = data[2 + 2 * i], reply[j++] = data[3 + 2 * i], reply[j++] = 1;
        vpkt_send(0x0008, reply, j);
    } else if (vtype == 0x0009) {
        ESP_LOGI(TAG, "answering the calculator's directory request with an empty list");
        vpkt_send(VPKT_EOT, NULL, 0);
    } else if (vtype == VPKT_RTS || vtype == VPKT_VAR_CNTS) {
        /* the calculator sends us a variable: accept it as a receiving calculator does (data acknowledgement, one byte 01) */
        static const uint8_t ok[1] = {0x01};
        if (vtype == VPKT_RTS && n >= 2 && data[0] == 0 && 2 + (uint32_t)data[1] <= n)
            ESP_LOGI(TAG, "the calculator wants to send %.*s", data[1], (const char *)data + 2);
        else
            ESP_LOGI(TAG, "the calculator sent the contents");
        vpkt_send(VPKT_DATA_ACK, ok, sizeof(ok));
    } else if (vtype == VPKT_EOT) {
        ESP_LOGI(TAG, "the calculator ended the transfer");
    }
}

/* answers what the calculator says on its own: buffer size requests, and data (acknowledged; a whole virtual packet is answered) */
static void answer(uint8_t type, uint32_t size)
{
    static const uint8_t ack[2] = {0xE0, 0x00};
    if (type == RAW_BUF_SIZE_REQ && size == 4) {
        uint8_t alloc[4];
        uint32_t want = get32(in_raw + 5);
        raw_max = want < RAW_MAX ? want : RAW_MAX;
        put32(alloc, raw_max);
        raw_send(RAW_BUF_SIZE_ALLOC, alloc, sizeof(alloc));
    } else if (type == RAW_DATA || type == RAW_DATA_LAST) {
        /* a virtual packet may span several raw packets: its header is only in the first; it is answered after the last */
        static uint8_t data[64];
        static uint16_t vtype;
        static uint32_t n;
        static bool inside;
        if (!inside) {
            if (size < VPKT_HDR) {
                raw_send(RAW_ACK, ack, sizeof(ack));
                return;
            }
            vtype = (uint16_t)((in_raw[9] << 8) | in_raw[10]);
            n = size - VPKT_HDR < sizeof(data) ? size - VPKT_HDR : sizeof(data);   /* keep the start: answering reuses in_raw */
            memcpy(data, in_raw + 5 + VPKT_HDR, n);
            ESP_LOGI(TAG, "the calculator sent virtual packet type %04X (%lu bytes)", vtype, (unsigned long)get32(in_raw + 5));
        }
        inside = type == RAW_DATA;
        raw_send(RAW_ACK, ack, sizeof(ack));
        if (!inside) answer_vpkt(vtype, data, n);
    }
}

static bool raw_recv_ack(void)
{
    uint8_t type;
    uint32_t size;
    for (;;) {
        if (!raw_recv(&type, &size, 8000)) {
            ESP_LOGW(TAG, "no acknowledgement from the calculator");
            return false;
        }
        if (type == RAW_ACK) return true;
        answer(type, size);   /* a buffer size request first is allowed */
        if (type != RAW_BUF_SIZE_REQ) return false;
    }
}

static bool vpkt_send(uint16_t type, const uint8_t *data, uint32_t size)
{
    static uint8_t chunk[RAW_MAX];
    uint32_t room = raw_max - VPKT_HDR, first = size < room ? size : room, sent;
    put32(chunk, size);
    chunk[4] = type >> 8;
    chunk[5] = type;
    if (first) memcpy(chunk + VPKT_HDR, data, first);
    if (!raw_send(size <= room ? RAW_DATA_LAST : RAW_DATA, chunk, VPKT_HDR + first) || !raw_recv_ack()) return false;
    if (size <= room) return true;
    for (sent = first; size - sent >= raw_max; sent += raw_max)
        if (!raw_send(RAW_DATA, data + sent, raw_max) || !raw_recv_ack()) return false;
    return raw_send(RAW_DATA_LAST, data + sent, size - sent) && raw_recv_ack();
}

/* waits for the calculator's next virtual packet (acknowledging its raw packets) and returns its type, 0 if none came */
static uint16_t vpkt_recv(int timeout_ms)
{
    uint8_t type;
    uint32_t size;
    uint16_t vtype = 0;
    bool first = true;
    do {
        if (!raw_recv(&type, &size, first ? timeout_ms : 3000)) return 0;
        if (type != RAW_DATA && type != RAW_DATA_LAST) {
            answer(type, size);
            continue;
        }
        if (first && size >= VPKT_HDR) vtype = (uint16_t)((in_raw[9] << 8) | in_raw[10]);
        if (vtype == VPKT_DELAY_ACK && size >= VPKT_HDR + 4) {
            ESP_LOGI(TAG, "the calculator asks to wait %lu us", (unsigned long)get32(in_raw + 11));
            answer(type, size);
            first = true;
            continue;
        }
        first = false;
        answer(type, size);
    } while (type != RAW_DATA_LAST);
    return vtype;
}

/* ---------------------------------------------------------------- the experiment */

/* TESTU2U: a TI-BASIC program, length (2, little-endian) then tokens: Disp "HI" */
static const uint8_t test_program[] = {0x05, 0x00, 0xDE, 0x2A, 0x48, 0x49, 0x2A};

/* with_ping: start like TI Connect (a mode-setting ping before the request); a calculator in RECEIVE (OS 5.x, 2026-10-28)
 * acknowledged the ping's raw packet but never answered it and then ignored the request, so the tries alternate */
static bool send_test_program(bool with_ping)
{
    static const uint8_t normal_mode[10] = {0, 3, 0, 1, 0, 0, 0, 0, 0x07, 0xD0};
    static const char name[] = "TESTU2U";
    uint8_t want[4], rts[64], type;
    uint32_t size;
    size_t j = 0, n = strlen(name);
    uint16_t reply;

    ESP_LOGI(TAG, "step 1: buffer size request");
    put32(want, 1024);
    if (!raw_send(RAW_BUF_SIZE_REQ, want, sizeof(want))) return false;
    if (raw_recv(&type, &size, 3000) && type == RAW_BUF_SIZE_ALLOC && size == 4) {
        raw_max = get32(in_raw + 5) < RAW_MAX ? get32(in_raw + 5) : RAW_MAX;
        ESP_LOGI(TAG, "the calculator allows raw packets of %lu bytes", (unsigned long)raw_max);
    } else {
        ESP_LOGW(TAG, "no buffer size answer; going on with %lu", (unsigned long)raw_max);
    }

    if (with_ping) {
        ESP_LOGI(TAG, "step 2: ping (normal mode)");
        if (vpkt_send(VPKT_PING, normal_mode, sizeof(normal_mode))) {
            reply = vpkt_recv(3000);
            ESP_LOGI(TAG, "ping answer: %04X", reply);
        }
    } else {
        ESP_LOGI(TAG, "step 2: no ping; listening 1 s");
        while (raw_recv(&type, &size, 1000)) answer(type, size);
    }

    ESP_LOGI(TAG, "step 3: request to send TESTU2U");
    rts[j++] = 0;
    rts[j++] = (uint8_t)n;
    memcpy(rts + j, name, n + 1);
    j += n + 1;
    put32(rts + j, sizeof(test_program));
    j += 4;
    rts[j++] = 0x01;
    rts[j++] = 0, rts[j++] = 3;
    rts[j++] = 0, rts[j++] = 0x02, rts[j++] = 0, rts[j++] = 4, rts[j++] = 0xF0, rts[j++] = 0x0F, rts[j++] = 0x00, rts[j++] = 0x05;
    rts[j++] = 0, rts[j++] = 0x03, rts[j++] = 0, rts[j++] = 1, rts[j++] = 0;
    rts[j++] = 0, rts[j++] = 0x08, rts[j++] = 0, rts[j++] = 4, rts[j++] = 0, rts[j++] = 0, rts[j++] = 0, rts[j++] = 0;
    if (!vpkt_send(VPKT_RTS, rts, j)) return false;
    reply = vpkt_recv(10000);
    ESP_LOGI(TAG, "answer to the request: %04X (AA00 = go ahead)", reply);
    if (reply != VPKT_DATA_ACK) return false;

    ESP_LOGI(TAG, "step 4: contents");
    if (!vpkt_send(VPKT_VAR_CNTS, test_program, sizeof(test_program))) return false;
    reply = vpkt_recv(5000);
    ESP_LOGI(TAG, "answer to the contents: %04X (AA00 = received)", reply);
    if (reply != VPKT_DATA_ACK) return false;

    ESP_LOGI(TAG, "step 5: end of transmission");
    return vpkt_send(VPKT_EOT, NULL, 0);
}

static void experiment_task(void *arg)
{
    for (;;) {
        uint8_t type;
        uint32_t size;
        int tries = 0;

        while (!calc_here()) {
            int waited;
            if (tud_suspended()) {   /* moved to another host (Air -> calculator) without a power cut: plug in again for it */
                ESP_LOGI(TAG, "the host went quiet: showing up again as a new device");
                tud_disconnect();
                vTaskDelay(pdMS_TO_TICKS(500));
                tud_connect();
                for (waited = 0; waited < 5000 && !calc_here(); waited += 50) vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }
            if (tud_connected()) {
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }
            ESP_LOGI(TAG, "no host on the bus: session request pulse");
            session_request();
            for (waited = 0; waited < 5000 && !tud_connected(); waited += 50) vTaskDelay(pdMS_TO_TICKS(50));
            if (tud_connected()) ESP_LOGI(TAG, "the calculator started the bus %d ms after the pulse", waited + 300);
        }
        ESP_LOGI(TAG, "listening 4 s for the calculator to speak first");
        while (raw_recv(&type, &size, 4000)) answer(type, size);
        while (calc_here()) {
            ++tries;
            ESP_LOGI(TAG, "sending TESTU2U, try %d (%s)", tries, tries % 2 ? "no ping" : "with ping");
            if (send_test_program(tries % 2 == 0)) {
                ESP_LOGI(TAG, "DONE: the calculator accepted TESTU2U. Check PRGM.");
                while (calc_here()) {
                    if (raw_recv(&type, &size, 1000)) answer(type, size);
                }
                break;
            }
            ESP_LOGW(TAG, "try %d failed; listening, then trying again in 5 s", tries);
            while (raw_recv(&type, &size, 5000)) answer(type, size);
        }
        ESP_LOGI(TAG, "the calculator is gone");
    }
}

/* a status line every 5 s */
static void status_task(void *arg)
{
    for (;;) {
        ESP_LOGI(TAG, "status: up %lu s, USB %s%s, Wi-Fi %s", (unsigned long)(xTaskGetTickCount() / configTICK_RATE_HZ),
                 tud_mounted() ? "set up by the calculator" : tud_connected() ? "talking, not set up" : "no signal",
                 tud_suspended() ? " (asleep)" : "", (xEventGroupGetBits(wifi_events) & WIFI_UP) ? "up" : "not yet");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    esp_err_t err;
    wifi_events = xEventGroupCreate();
    log_lines = xMessageBufferCreate(16384);
    log_lock = xSemaphoreCreateMutex();
    esp_log_set_vprintf(log_vprintf);
    xTaskCreate(log_task, "log", 3072, NULL, 3, NULL);
    ESP_LOGI(TAG, "u2u experiment starting");

    usb_start();   /* first, so the calculator sees the right device as soon as possible */
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    wifi_start();
    xTaskCreate(status_task, "status", 3072, NULL, 2, NULL);
    xTaskCreate(experiment_task, "u2u", 6144, NULL, 5, NULL);
}
