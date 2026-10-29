/* Calculator wireless bridge for an ESP32-S3.
 *
 *   Calculator (USB host) --USB--> this board (USB serial device) --Wi-Fi--> the CalcGPT server (cloud) or calcnet.py on the Mac
 *
 * The calculator is in charge of the USB link and powers the board through it: its mini-USB port goes through an OTG adapter
 * (USB-A socket, ID pin grounded) to a USB-A plug whose wires go to the board's 5V, GND, GPIO19 and GPIO20. So the board starts
 * when a program (CALCGPT, REELS, ...) turns the calculator's USB on, and stops when it ends.
 *
 * It shows itself to the calculator as a USB serial port, joins the strongest Wi-Fi network it knows, and connects to the first
 * server in main/secrets.h that answers: the Mac on its own network (calcnet.py, login "CALCNET1" with the shared secret, bytes
 * passed through untouched, for every Mac program) or the CalcGPT server from any network (login "CALCNET2" with this board's own
 * id and secret, over TLS). See ../PROTOCOL.md.
 *
 * Wi-Fi networks are added from the calculator: CALCGPT's Wi-Fi screen asks the board to scan ("W") and to join a network ("J"),
 * and the board answers those itself. Networks that worked are remembered in flash (up to 8), after the ones built into secrets.h.
 *
 * Nothing may be plugged into the board's own USB-C port marked USB: it is wired to the same GPIO19/GPIO20.
 * Messages are printed on the board's UART port (the other USB-C port), 115200 baud. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_crt_bundle.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "mbedtls/md.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/usb_serial_jtag.h"
#include "secrets.h"

/* 1: every log line also leaves through the USB-Serial/JTAG port (the calculator's port), to be read on a Mac. Debug builds only. */
#define USB_DEBUG_LOG 0

static const char *TAG = "calcbridge";

#define HELLO_V1 "CALCNET1"
#define HELLO_V2 "CALCNET2"
#define HELLO_LEN 8
#define CHALLENGE_LEN 16
#define DEVICE_ID_LEN 16
#define NET_CHUNK 1024

/* ---------------------------------------------------------------- remembered Wi-Fi networks */

#define SAVED_MAX 8
struct saved_net {
    char ssid[33];
    char password[65];
};
static struct saved_net saved[SAVED_MAX];   /* newest first; an empty ssid ends the list */
static SemaphoreHandle_t saved_lock;

static void saved_load(void)
{
    nvs_handle_t h;
    size_t size = sizeof(saved);
    memset(saved, 0, sizeof(saved));
    if (nvs_open("wifi", NVS_READONLY, &h) != ESP_OK) return;
    if (nvs_get_blob(h, "nets", saved, &size) != ESP_OK || size != sizeof(saved)) memset(saved, 0, sizeof(saved));
    nvs_close(h);
}

/* puts the network first in the list (the oldest one drops off the end when it is full) and stores the list in flash */
static void saved_remember(const char *ssid, const char *password)
{
    struct saved_net entry = {0};
    int i, j;
    nvs_handle_t h;

    strlcpy(entry.ssid, ssid, sizeof(entry.ssid));
    strlcpy(entry.password, password, sizeof(entry.password));
    xSemaphoreTake(saved_lock, portMAX_DELAY);
    for (i = 0; i < SAVED_MAX - 1 && saved[i].ssid[0] && strcmp(saved[i].ssid, ssid) != 0; i++)
        ;
    for (j = i; j > 0; j--) saved[j] = saved[j - 1];
    saved[0] = entry;
    if (nvs_open("wifi", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, "nets", saved, sizeof(saved));
        nvs_commit(h);
        nvs_close(h);
    }
    xSemaphoreGive(saved_lock);
}

/* the password for a network this board knows (remembered ones first, then the ones in secrets.h) */
static bool known_password(const char *ssid, char *password, size_t size)
{
    bool found = false;
    size_t i;
    xSemaphoreTake(saved_lock, portMAX_DELAY);
    for (i = 0; i < SAVED_MAX && saved[i].ssid[0] && !found; i++) {
        if (strcmp(saved[i].ssid, ssid) == 0) {
            strlcpy(password, saved[i].password, size);
            found = true;
        }
    }
    xSemaphoreGive(saved_lock);
    for (i = 0; (int)i < WIFI_NET_COUNT && !found; i++) {   /* (int): the count can be 0 */
        if (strcmp(WIFI_NETS[i].ssid, ssid) == 0) {
            strlcpy(password, WIFI_NETS[i].password, size);
            found = true;
        }
    }
    return found;
}

/* ---------------------------------------------------------------- messages for the calculator from the Wi-Fi task */

/* The session task owns the USB link, so the Wi-Fi task hands it what the calculator should be told */
enum { NOTE_STATUS, NOTE_SCAN_DONE, NOTE_STATE };
struct note {
    int kind;
    char text[64];
};
static QueueHandle_t notes;

static void note(int kind, const char *fmt, ...)
{
    struct note n = {.kind = kind};
    va_list args;
    if (fmt) {
        va_start(args, fmt);
        vsnprintf(n.text, sizeof(n.text), fmt, args);
        va_end(args);
    }
    xQueueSend(notes, &n, 0);
}

/* ---------------------------------------------------------------- Wi-Fi */

static EventGroupHandle_t wifi_events;
#define WIFI_UP BIT0           /* joined, with an address */
#define WIFI_FAILED BIT1       /* the last join attempt ended without an address */
#define WIFI_WANT_SCAN BIT2    /* the calculator asked for the list of networks */
#define WIFI_WANT_JOIN BIT3    /* the calculator asked to join a network */

static char current_ssid[33];  /* the network joined (or being joined); empty when none */
static int last_reason;        /* why the last attempt or connection ended */

#define SCAN_MAX 16
static wifi_ap_record_t scan[SCAN_MAX];
static uint16_t scan_count;
static SemaphoreHandle_t scan_lock;

static char join_ssid[33], join_password[65];   /* the network the calculator asked to join */

/* every log line is also sent as a UDP packet (port 9999, read by ../boardlog.py) to the first plain (not TLS) server address that
 * can be reached from this network, so the board can be watched with no cable */
#define LOG_PORT 9999
static int log_sock = -1;
static struct sockaddr_in log_dest;
static volatile bool log_on;

static int wifi_log(const char *fmt, va_list args)
{
    char line[200];
    va_list copy;
    int n;
    va_copy(copy, args);
    n = vsnprintf(line, sizeof(line), fmt, copy);
    va_end(copy);
    if (log_on && log_sock >= 0 && n > 0)
        sendto(log_sock, line, (size_t)n < sizeof(line) ? (size_t)n : sizeof(line) - 1, 0, (struct sockaddr *)&log_dest, sizeof(log_dest));
    if (USB_DEBUG_LOG && n > 0) usb_serial_jtag_write_bytes(line, (size_t)n < sizeof(line) ? (size_t)n : sizeof(line) - 1, 0);
    return vprintf(fmt, args);
}

static bool on_network(const struct endpoint *ep)
{
    return !ep->ssid[0] || strcmp(ep->ssid, current_ssid) == 0;
}

static void wifi_log_start(void)
{
    size_t i;
    log_on = false;
    for (i = 0; i < ENDPOINT_COUNT; i++) {
        if (ENDPOINTS[i].tls || !on_network(&ENDPOINTS[i])) continue;   /* a plain address (the Mac, or a test server) on this network */
        log_dest.sin_family = AF_INET;
        log_dest.sin_port = htons(LOG_PORT);
        if (inet_aton(ENDPOINTS[i].host, &log_dest.sin_addr) == 0) return;
        if (log_sock < 0) log_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (log_sock >= 0) {
            esp_log_set_vprintf(wifi_log);
            log_on = true;
        }
        return;
    }
}

static void wifi_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        last_reason = ((wifi_event_sta_disconnected_t *)data)->reason;
        log_on = false;
        xEventGroupClearBits(wifi_events, WIFI_UP);
        xEventGroupSetBits(wifi_events, WIFI_FAILED);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        wifi_log_start();
        xEventGroupClearBits(wifi_events, WIFI_FAILED);
        xEventGroupSetBits(wifi_events, WIFI_UP);
    }
}

static void do_scan(void)
{
    uint16_t n = SCAN_MAX;
    wifi_scan_config_t cfg = {.show_hidden = false};
    esp_err_t started, listed = ESP_OK;
    xSemaphoreTake(scan_lock, portMAX_DELAY);
    started = esp_wifi_scan_start(&cfg, true);
    if (started == ESP_OK) listed = esp_wifi_scan_get_ap_records(&n, scan);   /* strongest first */
    if (started != ESP_OK || listed != ESP_OK) n = 0;
    scan_count = n;
    if (USB_DEBUG_LOG) {
        int i;
        for (i = 0; i < n; i++) ESP_LOGI(TAG, "  %s  %d dBm  channel %d", (const char *)scan[i].ssid, scan[i].rssi, scan[i].primary);
    }
    xSemaphoreGive(scan_lock);
    if (started != ESP_OK || listed != ESP_OK)
        ESP_LOGW(TAG, "Wi-Fi scan failed: start %s, list %s", esp_err_to_name(started), esp_err_to_name(listed));
    ESP_LOGI(TAG, "Wi-Fi scan: %u networks", (unsigned)n);
}

/* why the board last restarted: in the status line, and in the first log line after Wi-Fi comes up (earlier lines are lost) */
static const char *reset_name(void)
{
    static const char *reasons[] = {"unknown", "power-on", "external pin", "software", "crash", "interrupt watchdog", "task watchdog",
                                    "other watchdog", "deep sleep", "brownout (power dipped)", "SDIO", "USB", "JTAG", "eFuse",
                                    "power glitch", "CPU lockup"};
    unsigned why = (unsigned)esp_reset_reason();
    return why < sizeof(reasons) / sizeof(reasons[0]) ? reasons[why] : "unknown";
}

/* joins one network and waits until it has an address or gives up (20 s). Returns true when joined. */
static bool join(const char *ssid, const char *password)
{
    wifi_config_t cfg = {0};
    EventBits_t bits;

    if (current_ssid[0]) {
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(300));   /* let the "disconnected" event pass before waiting for the new one */
    }
    xEventGroupClearBits(wifi_events, WIFI_UP | WIFI_FAILED);
    strlcpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, password, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = password[0] ? WIFI_AUTH_WEP : WIFI_AUTH_OPEN;
    strlcpy(current_ssid, ssid, sizeof(current_ssid));
    note(NOTE_STATE, NULL);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    ESP_LOGI(TAG, "joining Wi-Fi %s", ssid);
    esp_wifi_connect();
    bits = xEventGroupWaitBits(wifi_events, WIFI_UP | WIFI_FAILED, pdFALSE, pdFALSE, pdMS_TO_TICKS(20000));
    if (bits & WIFI_UP) {
        ESP_LOGI(TAG, "Wi-Fi connected: %s", ssid);
        ESP_LOGI(TAG, "this start came after: %s; free memory %u bytes", reset_name(), (unsigned)esp_get_free_heap_size());
        note(NOTE_STATE, NULL);
        return true;
    }
    esp_wifi_disconnect();
    if (!(bits & WIFI_FAILED)) last_reason = WIFI_REASON_NO_AP_FOUND;   /* no answer at all */
    ESP_LOGW(TAG, "could not join %s (reason %d)", ssid, last_reason);
    current_ssid[0] = 0;
    note(NOTE_STATE, NULL);
    return false;
}

static bool wrong_password(int reason)
{
    return reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || reason == WIFI_REASON_AUTH_FAIL || reason == WIFI_REASON_HANDSHAKE_TIMEOUT ||
           reason == WIFI_REASON_MIC_FAILURE || reason == WIFI_REASON_CONNECTION_FAIL;
}

/* tries the networks it knows that the last scan saw, strongest first */
static bool join_known(void)
{
    char ssids[SCAN_MAX][33];
    int n = 0, i;
    char password[65];

    xSemaphoreTake(scan_lock, portMAX_DELAY);
    for (i = 0; i < scan_count; i++) strlcpy(ssids[n++], (const char *)scan[i].ssid, sizeof(ssids[0]));
    xSemaphoreGive(scan_lock);
    for (i = 0; i < n; i++) {
        if (!ssids[i][0] || !known_password(ssids[i], password, sizeof(password))) continue;
        if (xEventGroupGetBits(wifi_events) & WIFI_WANT_JOIN) return false;   /* the calculator asked for a network: that comes first */
        if (join(ssids[i], password)) return true;
    }
    return false;
}

/* keeps the board on a network: scans, joins the strongest known one, and handles what the calculator asks for */
static void wifi_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(500));   /* the radio is still starting */
    for (;;) {
        EventBits_t bits = xEventGroupGetBits(wifi_events);
        if (bits & WIFI_WANT_JOIN) {
            char ssid[33], password[65];
            xEventGroupClearBits(wifi_events, WIFI_WANT_JOIN);
            strlcpy(ssid, join_ssid, sizeof(ssid));
            strlcpy(password, join_password, sizeof(password));
            note(NOTE_STATUS, "Joining %s...", ssid);
            if (join(ssid, password)) {
                saved_remember(ssid, password);
                note(NOTE_STATUS, "Connected to %s", ssid);
            } else {
                note(NOTE_STATUS, wrong_password(last_reason) ? "%s: wrong password?" : "Could not join %s", ssid);
            }
            continue;
        }
        if (bits & WIFI_WANT_SCAN) {
            xEventGroupClearBits(wifi_events, WIFI_WANT_SCAN);
            do_scan();
            note(NOTE_SCAN_DONE, NULL);
            continue;
        }
        if (!(bits & WIFI_UP)) {
            current_ssid[0] = 0;
            do_scan();
            if (!join_known()) {
                xEventGroupWaitBits(wifi_events, WIFI_WANT_SCAN | WIFI_WANT_JOIN, pdFALSE, pdFALSE, pdMS_TO_TICKS(8000));
            }
            continue;
        }
        xEventGroupWaitBits(wifi_events, WIFI_FAILED | WIFI_WANT_SCAN | WIFI_WANT_JOIN, pdFALSE, pdFALSE, portMAX_DELAY);
        if (xEventGroupGetBits(wifi_events) & WIFI_FAILED) {
            ESP_LOGW(TAG, "Wi-Fi lost (reason %d)", last_reason);
            note(NOTE_STATE, NULL);
        }
    }
}

static void wifi_start(void)
{
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);  /* no power saving: it adds delay to every message */
    xTaskCreate(wifi_task, "wifi", 4096, NULL, 5, NULL);
}

/* ---------------------------------------------------------------- the server */

/* one connection to a server: a plain socket (the Mac on its own network) or TLS (the CalcGPT server, from anywhere) */
typedef struct {
    int fd;
    esp_tls_t *tls;
    int version;   /* 1: calcnet on the Mac, bytes passed through; 2: the CalcGPT server, CalcGPT messages */
} conn_t;

static int conn_read(conn_t *c, uint8_t *buf, size_t n)
{
    return c->tls ? esp_tls_conn_read(c->tls, buf, n) : recv(c->fd, buf, n, 0);
}

static int conn_write(conn_t *c, const uint8_t *buf, size_t n)
{
    return c->tls ? esp_tls_conn_write(c->tls, buf, n) : send(c->fd, buf, n, 0);
}

static bool conn_pending(conn_t *c)
{
    return c->tls && esp_tls_get_bytes_avail(c->tls) > 0;
}

static void conn_close(conn_t *c)
{
    if (c->tls) esp_tls_conn_destroy(c->tls);
    else if (c->fd >= 0) close(c->fd);
    c->tls = NULL;
    c->fd = -1;
    c->version = 0;
}

static bool recv_exact(conn_t *c, uint8_t *buf, size_t n)
{
    size_t got = 0;
    while (got < n) {
        int r = conn_read(c, buf + got, n - got);
        if (r <= 0) return false;
        got += (size_t)r;
    }
    return true;
}

static bool send_all(conn_t *c, const uint8_t *buf, size_t n)
{
    size_t sent = 0;
    while (sent < n) {
        int r = conn_write(c, buf + sent, n - sent);
        if (r <= 0) return false;
        sent += (size_t)r;
    }
    return true;
}

static void set_timeouts(int fd, int seconds)
{
    struct timeval timeout = {.tv_sec = seconds, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

static bool open_plain(const char *host, int port, conn_t *c)
{
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM}, *res = NULL;
    char port_text[8];
    int sock;

    snprintf(port_text, sizeof(port_text), "%d", port);
    if (getaddrinfo(host, port_text, &hints, &res) != 0 || !res) return false;
    sock = socket(res->ai_family, res->ai_socktype, 0);
    if (sock < 0) {
        freeaddrinfo(res);
        return false;
    }
    set_timeouts(sock, 4);
    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        freeaddrinfo(res);
        close(sock);
        return false;
    }
    freeaddrinfo(res);
    c->fd = sock;
    c->tls = NULL;
    return true;
}

/* No clock is needed: this build checks certificates without their dates (CONFIG_MBEDTLS_HAVE_TIME_DATE is off), so TLS also
 * works on networks that block internet time servers, as school guest Wi-Fi often does. */
static bool open_tls(const char *host, int port, conn_t *c)
{
    esp_tls_cfg_t cfg = {.crt_bundle_attach = esp_crt_bundle_attach, .timeout_ms = 8000};
    esp_tls_t *tls;
    int one = 1;

    tls = esp_tls_init();
    if (!tls) return false;
    if (esp_tls_conn_new_sync(host, (int)strlen(host), port, &cfg, tls) != 1) {
        esp_tls_conn_destroy(tls);
        return false;
    }
    if (esp_tls_get_conn_sockfd(tls, &c->fd) != ESP_OK) {
        esp_tls_conn_destroy(tls);
        return false;
    }
    c->tls = tls;
    setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    return true;
}

static int hex_value(char ch)
{
    return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
}

/* the answer to a server's challenge: HMAC-SHA256(secret, challenge + "calc"). Version 1 uses the shared secret as text, version 2
 * this board's own secret (hex in secrets.h). */
static bool login_answer(int version, const uint8_t *challenge, uint8_t answer[32])
{
    uint8_t msg[CHALLENGE_LEN + 4], key[64];
    size_t key_len = 0, i;
    const char *hex = DEVICE_SECRET;

    memcpy(msg, challenge, CHALLENGE_LEN);
    memcpy(msg + CHALLENGE_LEN, "calc", 4);
    if (version == 1) {
        if (!CALC_SECRET[0]) return false;
        return mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const unsigned char *)CALC_SECRET, strlen(CALC_SECRET), msg,
                               sizeof(msg), answer) == 0;
    }
    for (i = 0; hex[i] && hex[i + 1] && key_len < sizeof(key); i += 2) {
        int hi = hex_value(hex[i]), lo = hex_value(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        key[key_len++] = (uint8_t)(hi << 4 | lo);
    }
    return key_len > 0 && mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, key_len, msg, sizeof(msg), answer) == 0;
}

/* tries the servers in secrets.h that can be reached from this network, in order, and logs in to the first that answers.
 * Returns true with c open (c->version says which kind of server it is). */
static bool connect_to_server(conn_t *c)
{
    size_t i;
    for (i = 0; i < ENDPOINT_COUNT; i++) {
        uint8_t hello[HELLO_LEN + CHALLENGE_LEN], answer[32], ok[2];
        const struct endpoint *ep = &ENDPOINTS[i];
        int one = 1;

        if (!on_network(ep)) continue;
        if (!(ep->tls ? open_tls(ep->host, ep->port, c) : open_plain(ep->host, ep->port, c))) continue;
        setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        set_timeouts(c->fd, 5);
        if (!recv_exact(c, hello, sizeof(hello))) {
            ESP_LOGW(TAG, "%s did not say hello", ep->host);
            conn_close(c);
            continue;
        }
        c->version = memcmp(hello, HELLO_V2, HELLO_LEN) == 0 ? 2 : memcmp(hello, HELLO_V1, HELLO_LEN) == 0 ? 1 : 0;
        if (!c->version || !login_answer(c->version, hello + HELLO_LEN, answer) ||
            (c->version == 2 && !send_all(c, (const uint8_t *)DEVICE_ID_PADDED, DEVICE_ID_LEN)) || !send_all(c, answer, sizeof(answer)) ||
            !recv_exact(c, ok, sizeof(ok)) || memcmp(ok, "OK", 2) != 0) {
            ESP_LOGW(TAG, "%s did not accept the login", ep->host);
            conn_close(c);
            continue;
        }
        set_timeouts(c->fd, 0);  /* from here on, wait as long as it takes */
        ESP_LOGI(TAG, "logged in to %s%s (%s)", ep->host, ep->tls ? " (encrypted)" : "", c->version == 2 ? "CalcGPT server" : "calcnet");
        return true;
    }
    return false;
}

/* ---------------------------------------------------------------- the calculator (USB serial device) */

/* The chip's built-in USB serial/JTAG port is the serial port: it is on the pins from power-on, so the calculator sets it up while the
 * board is still starting and nothing changes after that. (A software USB device would have to take the pins over later, and the
 * calculator does not notice that.) The console stays on the UART port only, so nothing else is written to this one. */

static StreamBufferHandle_t usb_to_net;          /* bytes from the calculator, waiting for the session task */

static void usb_rx_task(void *arg)
{
    uint8_t buf[512];
    for (;;) {
        int n = usb_serial_jtag_read_bytes(buf, sizeof(buf), portMAX_DELAY);
        if (n > 0 && xStreamBufferSend(usb_to_net, buf, (size_t)n, pdMS_TO_TICKS(20)) != (size_t)n)
            ESP_LOGW(TAG, "dropped %d bytes from the calculator", n);
    }
}

static void usb_start(void)
{
    usb_serial_jtag_driver_config_t cfg = {.tx_buffer_size = 4096, .rx_buffer_size = 4096};
    usb_to_net = xStreamBufferCreate(8192, 1);
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));
    xTaskCreate(usb_rx_task, "usb_rx", 3072, NULL, 9, NULL);
}

/* the calculator is driving the USB link (it sends a frame start every millisecond while it does). The driver's check reacts to a gap
 * of a few milliseconds, which the calculator leaves now and then, so it only counts as gone after 3 s without frames. */
static bool calc_here(void)
{
    static TickType_t last_seen;
    if (usb_serial_jtag_is_connected()) last_seen = xTaskGetTickCount();
    return last_seen != 0 && xTaskGetTickCount() - last_seen < pdMS_TO_TICKS(3000);
}

static bool usb_send(const uint8_t *buf, size_t n)
{
    size_t sent = 0;
    while (sent < n) {
        int r;
        if (!calc_here()) return false;
        r = usb_serial_jtag_write_bytes(buf + sent, n - sent, pdMS_TO_TICKS(1000));
        if (r <= 0) return false;
        sent += (size_t)r;
    }
    return true;
}

/* ---------------------------------------------------------------- CalcGPT messages (../PROTOCOL.md) */

/* Cuts the calculator's bytes into whole messages: Q len text, M index, J len ssid len password, and single letters */
struct calc_reader {
    uint8_t msg[2 + 255 + 1 + 255];
    size_t have;
};

/* returns the length of the complete message at the start of r->msg, or 0 if more bytes are needed */
static size_t calc_message_length(const struct calc_reader *r)
{
    if (r->have == 0) return 0;
    switch (r->msg[0]) {
    case 'Q':
        return r->have >= 2 && r->have >= 2u + r->msg[1] ? 2u + r->msg[1] : 0;
    case 'M':
        return r->have >= 2 ? 2 : 0;
    case 'J':
        if (r->have < 2 || r->have < 3u + r->msg[1]) return 0;
        return r->have >= 3u + r->msg[1] + r->msg[2u + r->msg[1]] ? 3u + r->msg[1] + r->msg[2u + r->msg[1]] : 0;
    default:
        return 1;
    }
}

/* Follows the server's bytes so the board only adds its own messages between whole server messages */
struct server_reader {
    int left;   /* bytes still to come of the current message; -1 = its length byte comes next */
};

static void server_follow(struct server_reader *s, const uint8_t *data, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        if (s->left > 0) s->left--;
        else if (s->left < 0) s->left = data[i];
        else if (strchr("ATLVY", data[i])) s->left = -1;   /* length byte, then text */
        else if (strchr("UX", data[i])) s->left = 1;        /* one byte follows */
    }
}

/* the board's own messages for the calculator, kept until they can be sent */
static uint8_t local_out[1024];
static size_t local_len;

static void local_add(char tag, const char *text)
{
    size_t n = text ? strlen(text) : 0;
    if (n > 255) n = 255;
    if (local_len + 2 + n > sizeof(local_out)) return;
    local_out[local_len++] = (uint8_t)tag;
    if (text) {
        local_out[local_len++] = (uint8_t)n;
        memcpy(local_out + local_len, text, n);
        local_len += n;
    }
}

/* a message whose text may contain any byte (a Wi-Fi list entry starts with its signal strength, 0 to 4) */
static void local_bytes(char tag, const char *data, size_t n)
{
    if (n > 255) n = 255;
    if (local_len + 2 + n > sizeof(local_out)) return;
    local_out[local_len++] = (uint8_t)tag;
    local_out[local_len++] = (uint8_t)n;
    memcpy(local_out + local_len, data, n);
    local_len += n;
}

static void local_byte(char tag, uint8_t value)
{
    if (local_len + 2 > sizeof(local_out)) return;
    local_out[local_len++] = (uint8_t)tag;
    local_out[local_len++] = value;
}

/* the connection line CALCGPT shows ("V") */
static void add_state(const conn_t *net)
{
    char line[64];
    if (xEventGroupGetBits(wifi_events) & WIFI_UP)
        snprintf(line, sizeof(line), "Wi-Fi: %s | %s", current_ssid, net->fd >= 0 ? "online" : "no server yet");
    else if (current_ssid[0])
        snprintf(line, sizeof(line), "Wi-Fi: joining %s", current_ssid);
    else
        snprintf(line, sizeof(line), "Wi-Fi: none (Mode = set up)");
    local_add('V', line);
}

/* the last scan as the calculator's Wi-Fi list ("X" count, then "Y" entries) */
static void add_scan(void)
{
    int i, n = 0;
    char entry[3 + 33], password[65];
    xSemaphoreTake(scan_lock, portMAX_DELAY);
    for (i = 0; i < scan_count; i++) n += scan[i].ssid[0] != 0;
    local_byte('X', (uint8_t)n);
    for (i = 0; i < scan_count; i++) {
        int rssi = scan[i].rssi;
        if (!scan[i].ssid[0]) continue;
        entry[0] = (char)(rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : rssi >= -85 ? 1 : 0);
        entry[1] = scan[i].authmode == WIFI_AUTH_OPEN ? ' ' : '*';
        entry[2] = known_password((const char *)scan[i].ssid, password, sizeof(password)) ? '+' : ' ';
        strlcpy(entry + 3, (const char *)scan[i].ssid, sizeof(entry) - 3);
        local_bytes('Y', entry, 3 + strlen(entry + 3));
    }
    xSemaphoreGive(scan_lock);
}

/* what the board does with one whole message from the calculator. Returns true if it is for the server. */
static bool calc_message(const uint8_t *m, size_t n, const conn_t *net)
{
    ESP_LOGI(TAG, "from the calculator: '%c' (%u bytes)", m[0] >= 32 && m[0] < 127 ? m[0] : '?', (unsigned)n);
    switch (m[0]) {
    case 'W':
        local_add('T', "Scanning...");
        xEventGroupSetBits(wifi_events, WIFI_WANT_SCAN);
        return false;
    case 'I':
        add_state(net);
        return false;
    case 'J': {
        size_t ssid_len = m[1], pass_len = m[2 + ssid_len];
        if (ssid_len == 0 || ssid_len >= sizeof(join_ssid) || pass_len >= sizeof(join_password)) {
            local_add('T', "That name or password is too long");
            return false;
        }
        memcpy(join_ssid, m + 2, ssid_len);
        join_ssid[ssid_len] = 0;
        memcpy(join_password, m + 3 + ssid_len, pass_len);
        join_password[pass_len] = 0;
        xEventGroupSetBits(wifi_events, WIFI_WANT_JOIN);
        return false;
    }
    default:
        break;
    }
    if (net->fd >= 0) return true;
    if (m[0] == 'R') {                 /* offline: an empty model list, and how to get online */
        local_add('K', NULL);
        add_state(net);
        local_add('D', NULL);          /* "done" clears the footer, so the hint goes after it */
        local_add('T', xEventGroupGetBits(wifi_events) & WIFI_UP ? "Connecting to the server..." : "No internet: Mode = Wi-Fi setup");
    } else if (m[0] == 'Q') {
        local_add('D', NULL);
        local_add('T', "Not connected yet");
    }
    return false;
}

/* ---------------------------------------------------------------- status every 10 s */

/* a status line every 10 s, so a silent board can be told apart from a board that is up but sees no calculator */
static void status_task(void *arg)
{
    for (;;) {
        esp_netif_ip_info_t ip = {0};
        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip);
        ESP_LOGI(TAG, "status: up %lu s (last restart: %s), Wi-Fi %s at " IPSTR ", calculator: %s",
                 (unsigned long)(xTaskGetTickCount() / configTICK_RATE_HZ), reset_name(),
                 current_ssid[0] ? current_ssid : "none", IP2STR(&ip.ip), calc_here() ? "connected" : "no USB signal from it");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/* ---------------------------------------------------------------- the session: calculator <-> board <-> server */

static bool readable(int fd, int milliseconds)
{
    fd_set set;
    struct timeval tv = {.tv_sec = 0, .tv_usec = milliseconds * 1000};
    FD_ZERO(&set);
    FD_SET(fd, &set);
    return select(fd + 1, &set, NULL, NULL, &tv) > 0;
}

/* One task does all the copying (a TLS connection must not be read and written from two tasks at once). With calcnet (version 1)
 * the bytes go through untouched; otherwise the board reads the calculator's messages, answers the Wi-Fi ones itself, and passes the
 * rest to the CalcGPT server. */
static void session_task(void *arg)
{
    static uint8_t buf[NET_CHUNK];
    static struct calc_reader reader;
    struct server_reader follow = {0};
    conn_t net = {.fd = -1, .tls = NULL, .version = 0};
    TickType_t next_try = 0;
    int wait_s = 2;
    bool was_here = false;

    for (;;) {
        struct note n;
        bool here = calc_here();
        size_t got;

        if (!here) {
            if (was_here) ESP_LOGI(TAG, "the calculator left");
            if (net.fd >= 0) conn_close(&net);
            was_here = false;
            reader.have = 0;
            local_len = 0;
            follow.left = 0;
            xStreamBufferReset(usb_to_net);
            xQueueReset(notes);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (!was_here) {
            ESP_LOGI(TAG, "the calculator is connected over USB");
            was_here = true;
            next_try = 0;
        }

        /* what the Wi-Fi task has to say */
        while (xQueueReceive(notes, &n, 0) == pdTRUE) {
            if (net.version == 1) continue;   /* another Mac program owns the screen */
            if (n.kind == NOTE_STATUS) local_add('T', n.text);
            else if (n.kind == NOTE_SCAN_DONE) {
                add_scan();
                local_add('T', "");   /* clears "Scanning..." */
            } else add_state(&net);
        }

        /* log in when there is a network and no server yet (this can take a few seconds) */
        if (net.fd < 0 && (xEventGroupGetBits(wifi_events) & WIFI_UP) && xTaskGetTickCount() >= next_try) {
            if (connect_to_server(&net)) {
                wait_s = 2;
                follow.left = 0;
                if (net.version == 2) add_state(&net);
            } else {
                next_try = xTaskGetTickCount() + pdMS_TO_TICKS(wait_s * 1000);
                wait_s = wait_s < 30 ? wait_s * 2 : 30;
            }
        }

        /* the calculator's bytes */
        got = xStreamBufferReceive(usb_to_net, buf, sizeof(buf), 0);
        if (got && net.version == 1) {
            if (!send_all(&net, buf, got)) {
                ESP_LOGI(TAG, "link ended (could not send to the Mac)");
                conn_close(&net);
            }
        } else {
            size_t i;
            for (i = 0; i < got; i++) {
                size_t len;
                if (reader.have < sizeof(reader.msg)) reader.msg[reader.have++] = buf[i];
                len = calc_message_length(&reader);
                if (!len) {
                    if (reader.have == sizeof(reader.msg)) reader.have = 0;   /* garbled: start again */
                    continue;
                }
                if (calc_message(reader.msg, len, &net) && !send_all(&net, reader.msg, len)) {
                    ESP_LOGI(TAG, "link ended (could not send to the server)");
                    conn_close(&net);
                    add_state(&net);
                }
                reader.have = 0;
            }
        }

        /* the server's bytes */
        if (net.fd >= 0 && (conn_pending(&net) || readable(net.fd, got ? 0 : 5))) {
            int r = conn_read(&net, buf, sizeof(buf));
            if (r <= 0) {
                ESP_LOGI(TAG, "link ended (the server closed the connection)");
                conn_close(&net);
                follow.left = 0;
                add_state(&net);
                next_try = xTaskGetTickCount() + pdMS_TO_TICKS(2000);
            } else {
                if (net.version == 2) server_follow(&follow, buf, (size_t)r);
                if (!usb_send(buf, (size_t)r)) ESP_LOGW(TAG, "could not send to the calculator");
            }
        } else if (net.fd < 0) {
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        /* the board's own messages, only between whole server messages */
        if (local_len && (net.fd < 0 || (net.version == 2 && follow.left == 0))) {
            usb_send(local_out, local_len);
            local_len = 0;
        } else if (net.version == 1) {
            local_len = 0;
        }
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    saved_lock = xSemaphoreCreateMutex();
    scan_lock = xSemaphoreCreateMutex();
    wifi_events = xEventGroupCreate();
    notes = xQueueCreate(8, sizeof(struct note));
    saved_load();
    usb_start();
    if (USB_DEBUG_LOG) {
        esp_log_set_vprintf(wifi_log);
        ESP_LOGI(TAG, "start: after %s", reset_name());
    }
    wifi_start();
    if (USB_DEBUG_LOG) {
        wifi_country_t country = {0};
        int8_t power = 0;
        esp_wifi_get_country(&country);
        esp_wifi_get_max_tx_power(&power);
        ESP_LOGI(TAG, "radio: country %.2s, channels %d-%d, max power %d (quarter dBm)", country.cc, country.schan,
                 country.schan + country.nchan - 1, power);
    }
    xTaskCreate(status_task, "status", 3072, NULL, 2, NULL);
    xTaskCreate(session_task, "session", 8192, NULL, 6, NULL);
    ESP_LOGI(TAG, "waiting for the calculator to set up the USB link");
}
