/* Calculator wireless bridge for an ESP32-S3.
 *
 *   Calculator --USB--> this board (USB host) --Wi-Fi--> calcnet.py on the Mac
 *
 * Plugged into a calculator at its home screen, it first puts CALCGPT, SOLVER and their libraries on it (loader.c).
 * It waits for the calculator (running CALCGPT, REELS, ...) to appear as a USB serial device, joins the Wi-Fi, connects to
 * calcnet.py on the Mac, proves it knows the shared secret (challenge and response, see wireless/calcnet.py) and then copies bytes
 * both ways. When either side goes away it starts again. Settings (Wi-Fi networks, the Mac's addresses, the secret) are in main/secrets.h.
 * It knows several Wi-Fi networks (home, phone hotspot, ...) and tries them in turn. It reaches the Mac either directly on the home
 * network or, from anywhere, through the Mac's Tailscale Funnel address over TLS (see ../AWAY.md).
 *
 * The board's native USB pins (GPIO19 = D-, GPIO20 = D+) must go to a USB-A socket that also gets 5 V and ground: see ../ESP_SETUP.md.
 * The calculator must be the USB device, so its cable is a normal mini-USB (B) one, not an OTG adapter; the board powers it, so
 * the board needs its own 5 V (a power bank on the 5V and GND pins, not on its USB-C port marked USB: that port shares GPIO19/20).
 * Messages are printed on the board's UART port (the other USB-C port), 115200 baud. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_crt_bundle.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "mbedtls/md.h"
#include "nvs_flash.h"
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"
#include "loader.h"
#include "secrets.h"

static const char *TAG = "calcbridge";

/* a calculator program using the srldrvce library (CALCGPT, REELS, ...) shows up as this USB serial device */
#define SRL_VID 0x16C0
#define SRL_PID 0x05E1

#define HELLO "CALCNET1"
#define HELLO_LEN 8
#define CHALLENGE_LEN 16
#define NET_CHUNK 1024

static EventGroupHandle_t wifi_events;
#define WIFI_UP BIT0

static StreamBufferHandle_t usb_to_net;          /* bytes from the calculator, waiting to go to the Mac */
static volatile bool usb_lost;                   /* the calculator was unplugged or its program quit (its USB handle is already closed) */
static int wifi_idx;                             /* which network in WIFI_NETS is being tried */
static int wifi_fails;                           /* attempts in a row that did not get an address */
static bool time_ok;                             /* the clock has been set (needed to check TLS certificates) */

/* ---------------------------------------------------------------- log over Wi-Fi */

/* every log line is also sent as a UDP packet to the Mac (port 9999, read by ../boardlog.py), so the board can be watched with no cable.
 * Only used once the Wi-Fi is up; the first lines after a restart are only on the UART port. */
#define LOG_PORT 9999
static int log_sock = -1;
static struct sockaddr_in log_dest;

static int wifi_log(const char *fmt, va_list args)
{
    char line[200];
    va_list copy;
    int n;
    va_copy(copy, args);
    n = vsnprintf(line, sizeof(line), fmt, copy);
    va_end(copy);
    if (log_sock >= 0 && n > 0) sendto(log_sock, line, (size_t)n < sizeof(line) ? (size_t)n : sizeof(line) - 1, 0, (struct sockaddr *)&log_dest, sizeof(log_dest));
    return vprintf(fmt, args);
}

static void wifi_log_start(void)
{
    size_t i;
    for (i = 0; i < ENDPOINT_COUNT; i++) {
        if (ENDPOINTS[i].tls || ENDPOINTS[i].wifi != wifi_idx) continue;   /* the Mac's plain address on this network */
        log_dest.sin_family = AF_INET;
        log_dest.sin_port = htons(LOG_PORT);
        if (inet_aton(ENDPOINTS[i].host, &log_dest.sin_addr) == 0) return;
        if (log_sock < 0) log_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (log_sock >= 0) esp_log_set_vprintf(wifi_log);
        return;
    }
}

/* ---------------------------------------------------------------- Wi-Fi */

static void wifi_try(int idx)
{
    wifi_config_t cfg = {0};
    wifi_idx = idx;
    strlcpy((char *)cfg.sta.ssid, WIFI_NETS[idx].ssid, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, WIFI_NETS[idx].password, sizeof(cfg.sta.password));
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    ESP_LOGI(TAG, "joining Wi-Fi %d of %d", idx + 1, (int)WIFI_NET_COUNT);
    esp_wifi_connect();
}

static void wifi_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        wifi_try(0);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        int reason = ((wifi_event_sta_disconnected_t *)data)->reason;
        xEventGroupClearBits(wifi_events, WIFI_UP);
        ESP_LOGW(TAG, "Wi-Fi lost (reason %d), reconnecting", reason);
        /* the network is not here (or keeps failing): try the next one, and the first again after the last */
        if (WIFI_NET_COUNT > 1 && (reason == WIFI_REASON_NO_AP_FOUND || ++wifi_fails >= 3)) {
            wifi_fails = 0;
            wifi_try((wifi_idx + 1) % (int)WIFI_NET_COUNT);
        } else {
            esp_wifi_connect();
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Wi-Fi connected (network %d)", wifi_idx + 1);
        wifi_fails = 0;
        wifi_log_start();
        xEventGroupSetBits(wifi_events, WIFI_UP);
    }
}

static void wifi_start(void)
{
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();

    wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);  /* no power saving: it adds delay to every message */
}

/* ---------------------------------------------------------------- the Mac */

/* one connection to the Mac: a plain socket (home network) or TLS (through the Tailscale Funnel address, from anywhere) */
typedef struct {
    int fd;
    esp_tls_t *tls;
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
    else close(c->fd);
    c->tls = NULL;
    c->fd = -1;
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

/* TLS needs the right time to check the certificate: get it from the internet once */
static bool clock_set(void)
{
    static bool started;
    if (time_ok) return true;
    if (!started) {
        esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        esp_netif_sntp_init(&sntp);
        started = true;
    }
    time_ok = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(8000)) == ESP_OK;
    if (!time_ok) ESP_LOGW(TAG, "could not get the time, so no TLS yet");
    return time_ok;
}

static bool open_tls(const char *host, int port, conn_t *c)
{
    esp_tls_cfg_t cfg = {.crt_bundle_attach = esp_crt_bundle_attach, .timeout_ms = 8000};
    esp_tls_t *tls;
    int one = 1;

    if (!clock_set()) return false;
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

/* try the Mac's addresses in order (the home one only on the home Wi-Fi) and log in: it sends "CALCNET1" + 16 random bytes, we answer
 * HMAC-SHA256(secret, challenge + "calc"), it says "OK". Returns true with c open. */
static bool connect_to_mac(conn_t *c)
{
    size_t i;
    for (i = 0; i < ENDPOINT_COUNT; i++) {
        uint8_t hello[HELLO_LEN + CHALLENGE_LEN], msg[CHALLENGE_LEN + 4], answer[32], ok[2];
        const struct endpoint *ep = &ENDPOINTS[i];
        int one = 1;

        if (ep->wifi >= 0 && ep->wifi != wifi_idx) continue;
        if (!(ep->tls ? open_tls(ep->host, ep->port, c) : open_plain(ep->host, ep->port, c))) continue;
        setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        set_timeouts(c->fd, 5);
        if (!recv_exact(c, hello, sizeof(hello)) || memcmp(hello, HELLO, HELLO_LEN) != 0) {
            ESP_LOGW(TAG, "%s is not calcnet", ep->host);
            conn_close(c);
            continue;
        }
        memcpy(msg, hello + HELLO_LEN, CHALLENGE_LEN);
        memcpy(msg + CHALLENGE_LEN, "calc", 4);
        if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const unsigned char *)CALC_SECRET, strlen(CALC_SECRET), msg,
                            sizeof(msg), answer) != 0 ||
            !send_all(c, answer, sizeof(answer)) || !recv_exact(c, ok, sizeof(ok)) || memcmp(ok, "OK", 2) != 0) {
            ESP_LOGW(TAG, "%s did not accept the secret (check that secrets.h matches ~/.calcnet-token)", ep->host);
            conn_close(c);
            continue;
        }
        set_timeouts(c->fd, 0);  /* from here on, wait as long as it takes */
        ESP_LOGI(TAG, "logged in to the Mac at %s%s", ep->host, ep->tls ? " (encrypted)" : "");
        return true;
    }
    return false;
}

/* ---------------------------------------------------------------- the calculator (USB host) */

static bool usb_data_cb(const uint8_t *data, size_t len, void *arg)
{
    if (xStreamBufferSend(usb_to_net, data, len, pdMS_TO_TICKS(20)) != len) ESP_LOGW(TAG, "dropped %u bytes from the calculator", (unsigned)len);
    return true;
}

static void usb_event_cb(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    switch (event->type) {
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGI(TAG, "the calculator was unplugged or its program ended");
        cdc_acm_host_close(event->data.cdc_hdl);
        usb_lost = true;
        break;
    case CDC_ACM_HOST_ERROR:
        ESP_LOGW(TAG, "USB error %d", event->data.error);
        break;
    default:
        break;
    }
}

static void usb_library_task(void *arg)
{
    for (;;) {
        uint32_t flags;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) usb_host_device_free_all();
    }
}

static void usb_start(void)
{
    const usb_host_config_t host = {.skip_phy_setup = false, .intr_flags = ESP_INTR_FLAG_LEVEL1};
    usb_to_net = xStreamBufferCreate(8192, 1);
    ESP_ERROR_CHECK(usb_host_install(&host));
    xTaskCreate(usb_library_task, "usb_lib", 4096, NULL, 10, NULL);
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));
    loader_start();
}

/* ---------------------------------------------------------------- status every 10 s */

/* a status line every 10 s, so a silent board can be told apart from a board that is up but sees no calculator */
static void status_task(void *arg)
{
    static const char *reasons[] = {"unknown", "power-on", "external pin", "software", "crash", "interrupt watchdog", "task watchdog",
                                    "other watchdog", "deep sleep", "brownout (power dipped)", "SDIO", "USB", "JTAG", "eFuse",
                                    "power glitch", "CPU lockup"};
    esp_reset_reason_t why = esp_reset_reason();
    for (;;) {
        esp_netif_ip_info_t ip = {0};
        uint8_t addrs[4];
        int devices = 0;
        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip);
        usb_host_device_addr_list_fill(sizeof(addrs), addrs, &devices);
        ESP_LOGI(TAG, "status: up %lu s (last restart: %s), Wi-Fi network %d of %d at " IPSTR ", USB devices on the socket: %d",
                 (unsigned long)(xTaskGetTickCount() / configTICK_RATE_HZ), (unsigned)why < sizeof(reasons) / sizeof(reasons[0]) ? reasons[why] : "unknown",
                 wifi_idx + 1, (int)WIFI_NET_COUNT, IP2STR(&ip.ip), devices);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/* ---------------------------------------------------------------- one session */

static bool readable(int fd, int milliseconds)
{
    fd_set set;
    struct timeval tv = {.tv_sec = 0, .tv_usec = milliseconds * 1000};
    FD_ZERO(&set);
    FD_SET(fd, &set);
    return select(fd + 1, &set, NULL, NULL, &tv) > 0;
}

static void session(void)
{
    cdc_acm_dev_hdl_t dev = NULL;
    const cdc_acm_host_device_config_t cfg = {.connection_timeout_ms = 1000, .out_buffer_size = 512, .in_buffer_size = 2048,
                                              .event_cb = usb_event_cb, .data_cb = usb_data_cb, .user_arg = NULL};
    uint8_t buf[NET_CHUNK];
    conn_t net = {.fd = -1, .tls = NULL};

    usb_lost = false;
    if (cdc_acm_host_open(SRL_VID, SRL_PID, 0, &cfg, &dev) != ESP_OK) return;  /* no calculator program running yet */
    ESP_LOGI(TAG, "the calculator is connected over USB");
    {
        cdc_acm_line_coding_t coding = {.dwDTERate = 115200, .bCharFormat = 0, .bParityType = 0, .bDataBits = 8};
        esp_err_t e1 = cdc_acm_host_line_coding_set(dev, &coding);   /* what a computer's serial port would set */
        esp_err_t e2 = cdc_acm_host_set_control_line_state(dev, true, true);
        ESP_LOGI(TAG, "serial setup: line coding %s, control lines %s", esp_err_to_name(e1), esp_err_to_name(e2));
    }
    xStreamBufferReset(usb_to_net);

    xEventGroupWaitBits(wifi_events, WIFI_UP, pdFALSE, pdTRUE, portMAX_DELAY);
    while (!connect_to_mac(&net)) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        if (usb_lost) return;  /* the calculator left while we were waiting for the Mac */
    }
    if (usb_lost) {  /* it left just now */
        conn_close(&net);
        return;
    }

    /* one task copies both ways (a TLS connection must not be read and written from two tasks at once) */
    const char *why = "the calculator was unplugged or its program ended";
    while (!usb_lost) {
        size_t n = xStreamBufferReceive(usb_to_net, buf, sizeof(buf), 0);   /* the calculator's bytes go to the Mac */
        if (n && !send_all(&net, buf, n)) {
            why = "could not send to the Mac";
            break;
        }
        if (n) continue;
        if (conn_pending(&net) || readable(net.fd, 5)) {                      /* the Mac's bytes (pictures, text) go to the calculator */
            int r = conn_read(&net, buf, sizeof(buf));
            if (r <= 0) {
                why = "the Mac closed the connection";
                break;
            }
            if (cdc_acm_host_data_tx_blocking(dev, buf, (size_t)r, 1000) != ESP_OK) {
                why = "could not send to the calculator";
                break;
            }
        }
    }
    conn_close(&net);
    if (!usb_lost) cdc_acm_host_close(dev);                   /* otherwise the unplug event already closed it */
    ESP_LOGI(TAG, "link ended (%s); starting again", why);
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    wifi_start();
    usb_start();
    xTaskCreate(status_task, "status", 3072, NULL, 2, NULL);
    ESP_LOGI(TAG, "waiting for a calculator: at its home screen it gets CALCGPT and SOLVER; running CALCGPT, it is bridged to the Mac");
    for (;;) {
        session();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
