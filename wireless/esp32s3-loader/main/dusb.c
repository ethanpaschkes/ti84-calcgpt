/* TI's DUSB link protocol, host side (see dusb.h).
 *
 *   raw packet:      size (4 bytes, big-endian, not counting these 5) | type (1) | data
 *                    1 buffer size request, 2 buffer size allocation, 3 data (more follows), 4 data (last), 5 acknowledgement (E0 00)
 *   virtual packet:  size (4) | type (2) | data, carried in raw packets 3/4 (only the first raw packet has this header);
 *                    every raw packet 3/4 is acknowledged with a raw packet 5, both ways.
 *
 * Virtual packet types used here are the DUSB_VPKT_* values of libticalcs2. Everything is big-endian. */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "usb/usb_helpers.h"
#include "dusb.h"

static const char *TAG = "dusb";

#define RAW_BUF_SIZE_REQ 1
#define RAW_BUF_SIZE_ALLOC 2
#define RAW_DATA 3
#define RAW_DATA_LAST 4
#define RAW_ACK 5

#define VPKT_PING 0x0001          /* also "set mode" */
#define VPKT_PARM_REQ 0x0007
#define VPKT_PARM_DATA 0x0008
#define VPKT_DIR_REQ 0x0009
#define VPKT_VAR_HDR 0x000A
#define VPKT_RTS 0x000B
#define VPKT_VAR_CNTS 0x000D
#define VPKT_MODE_SET 0x0012      /* the answer to a ping */
#define VPKT_DATA_ACK 0xAA00
#define VPKT_DELAY_ACK 0xBB00
#define VPKT_EOT 0xDD00
#define VPKT_ERROR 0xEE00

#define PID_OS_VERSION 0x000B
#define AID_VAR_SIZE 0x0001
#define AID_VAR_TYPE 0x0002
#define AID_ARCHIVED 0x0003
#define AID_VAR_VERSION 0x0008

#define BUF_SIZE_WANTED 1024
#define RAW_MAX_CE 1018           /* the CE offers more than it handles (libticalcs clamps to this) */
#define VPKT_HDR 6
#define REPLY_MAX 1024            /* the replies read here (directory entries, parameters, acks) are small */
#define XFER_TIMEOUT_MS 4000

static SemaphoreHandle_t xfer_done;
static uint8_t raw_buf[5 + RAW_MAX_CE];
static uint8_t reply[REPLY_MAX];

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}

static uint32_t get32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

/* ---------------------------------------------------------------- USB transfers */

static void xfer_cb(usb_transfer_t *xfer)
{
    xSemaphoreGive(xfer_done);
}

/* runs one transfer to the end; on a timeout the endpoint is flushed so the transfer comes back cancelled */
static bool xfer_run(dusb_t *d, usb_transfer_t *x)
{
    x->device_handle = d->dev;
    x->callback = xfer_cb;
    x->timeout_ms = 0;
    if (usb_host_transfer_submit(x) != ESP_OK) return false;
    if (xSemaphoreTake(xfer_done, pdMS_TO_TICKS(XFER_TIMEOUT_MS)) != pdTRUE) {
        usb_host_endpoint_halt(d->dev, x->bEndpointAddress);
        usb_host_endpoint_flush(d->dev, x->bEndpointAddress);
        xSemaphoreTake(xfer_done, portMAX_DELAY);
        usb_host_endpoint_clear(d->dev, x->bEndpointAddress);
        ESP_LOGW(TAG, "no answer from the calculator");
        return false;
    }
    return x->status == USB_TRANSFER_STATUS_COMPLETED;
}

static bool usb_write(dusb_t *d, const uint8_t *buf, size_t n)
{
    usb_transfer_t *x = d->xfer_out;
    memcpy(x->data_buffer, buf, n);
    x->num_bytes = (int)n;
    x->bEndpointAddress = d->ep_out;
    return xfer_run(d, x) && x->actual_num_bytes == (int)n;
}

/* the calculator's bytes come one USB packet at a time and raw packets are read to the byte, so keep what is left over */
static bool usb_read(dusb_t *d, uint8_t *buf, size_t n)
{
    while (n) {
        size_t take;
        if (d->in_pos == d->in_len) {
            usb_transfer_t *x = d->xfer_in;
            x->num_bytes = d->in_mps;
            x->bEndpointAddress = d->ep_in;
            if (!xfer_run(d, x)) return false;
            memcpy(d->in_buf, x->data_buffer, x->actual_num_bytes);
            d->in_len = (uint16_t)x->actual_num_bytes;
            d->in_pos = 0;
            continue;
        }
        take = d->in_len - d->in_pos;
        if (take > n) take = n;
        memcpy(buf, d->in_buf + d->in_pos, take);
        d->in_pos += take;
        buf += take;
        n -= take;
    }
    return true;
}

/* ---------------------------------------------------------------- raw packets */

static bool raw_send(dusb_t *d, uint8_t type, const uint8_t *data, uint32_t size)
{
    put32(raw_buf, size);
    raw_buf[4] = type;
    if (size) memcpy(raw_buf + 5, data, size);
    if (!usb_write(d, raw_buf, 5 + size)) return false;
    /* the CE needs a zero-length packet after a last data packet that fills whole USB packets */
    if (type == RAW_DATA_LAST && (size + 5) % 64 == 0) return usb_write(d, raw_buf, 0);
    return true;
}

static bool raw_recv(dusb_t *d, uint8_t *type, uint8_t *data, uint32_t max, uint32_t *size)
{
    uint8_t head[5];
    if (!usb_read(d, head, sizeof(head))) return false;
    *size = get32(head);
    *type = head[4];
    if (*size > max) {
        ESP_LOGW(TAG, "raw packet of %lu bytes is too big", (unsigned long)*size);
        return false;
    }
    return usb_read(d, data, *size);
}

static bool raw_send_ack(dusb_t *d)
{
    static const uint8_t ack[2] = {0xE0, 0x00};
    return raw_send(d, RAW_ACK, ack, sizeof(ack));
}

/* the calculator acknowledges each raw data packet; it may ask for a buffer size first */
static bool raw_recv_ack(dusb_t *d)
{
    uint8_t type, data[4];
    uint32_t size;
    if (!raw_recv(d, &type, data, sizeof(data), &size)) return false;
    if (type == RAW_BUF_SIZE_REQ && size == 4) {
        uint8_t alloc[4];
        put32(alloc, get32(data));
        if (!raw_send(d, RAW_BUF_SIZE_ALLOC, alloc, sizeof(alloc))) return false;
        d->raw_max = get32(data);
        if (!raw_recv(d, &type, data, sizeof(data), &size)) return false;
    }
    return type == RAW_ACK && size == 2;
}

/* ---------------------------------------------------------------- virtual packets */

static bool vpkt_send(dusb_t *d, uint16_t type, const uint8_t *data, uint32_t size)
{
    static uint8_t chunk[RAW_MAX_CE];
    uint32_t room = d->raw_max - VPKT_HDR, first = size < room ? size : room, sent;

    put32(chunk, size);
    chunk[4] = type >> 8;
    chunk[5] = type;
    if (first) memcpy(chunk + VPKT_HDR, data, first);
    if (!raw_send(d, size <= room ? RAW_DATA_LAST : RAW_DATA, chunk, VPKT_HDR + first) || !raw_recv_ack(d)) return false;
    if (size <= room) return true;
    /* full packets, then a last one with the rest, empty if nothing is left (as libticalcs does) */
    for (sent = first; size - sent >= d->raw_max; sent += d->raw_max)
        if (!raw_send(d, RAW_DATA, data + sent, d->raw_max) || !raw_recv_ack(d)) return false;
    return raw_send(d, RAW_DATA_LAST, data + sent, size - sent) && raw_recv_ack(d);
}

/* one virtual packet from the calculator into reply[] */
static bool vpkt_recv_once(dusb_t *d, uint16_t *type, uint32_t *size)
{
    static uint8_t raw[RAW_MAX_CE];
    uint32_t declared = 0, got = 0, n;
    uint8_t rtype;
    bool first = true;
    do {
        if (!raw_recv(d, &rtype, raw, sizeof(raw), &n)) return false;
        if (rtype != RAW_DATA && rtype != RAW_DATA_LAST) {
            ESP_LOGW(TAG, "unexpected raw packet type %u", rtype);
            return false;
        }
        if (first) {
            if (n < VPKT_HDR) return false;
            declared = get32(raw);
            *type = get16(raw + 4);
            memmove(raw, raw + VPKT_HDR, n - VPKT_HDR);
            n -= VPKT_HDR;
            first = false;
        }
        if (got + n > sizeof(reply)) {
            ESP_LOGW(TAG, "reply of %lu bytes is too big", (unsigned long)declared);
            return false;
        }
        memcpy(reply + got, raw, n);
        got += n;
        if (!raw_send_ack(d)) return false;
    } while (rtype != RAW_DATA_LAST);
    *size = got;
    return got == declared;
}

/* like vpkt_recv_once, but waits out "busy, ask again later" answers and reports errors */
static bool vpkt_recv(dusb_t *d, uint16_t *type, uint32_t *size)
{
    for (;;) {
        if (!vpkt_recv_once(d, type, size)) return false;
        if (*type == VPKT_DELAY_ACK && *size >= 4) {
            uint32_t us = get32(reply);
            vTaskDelay(pdMS_TO_TICKS((us > 400000 ? 400000 : us) / 1000 + 1));
            continue;
        }
        if (*type == VPKT_ERROR) ESP_LOGW(TAG, "the calculator answered with error %04x", *size >= 2 ? get16(reply) : 0);
        return true;
    }
}

static bool vpkt_expect(dusb_t *d, uint16_t want)
{
    uint16_t type;
    uint32_t size;
    if (!vpkt_recv(d, &type, &size)) return false;
    if (type != want) ESP_LOGW(TAG, "expected packet %04x, got %04x", want, type);
    return type == want;
}

/* ---------------------------------------------------------------- commands */

bool dusb_open(dusb_t *d, usb_host_client_handle_t client, usb_device_handle_t dev)
{
    const usb_config_desc_t *cfg;
    const usb_intf_desc_t *intf;
    int intf_offset = 0, i;

    memset(d, 0, sizeof(*d));
    d->client = client;
    d->dev = dev;
    if (!xfer_done) xfer_done = xSemaphoreCreateBinary();
    if (usb_host_get_active_config_descriptor(dev, &cfg) != ESP_OK) return false;
    intf = usb_parse_interface_descriptor(cfg, 0, 0, &intf_offset);
    if (!intf) return false;
    for (i = 0; i < intf->bNumEndpoints; i++) {
        int offset = intf_offset;
        const usb_ep_desc_t *ep = usb_parse_endpoint_descriptor_by_index(intf, i, cfg->wTotalLength, &offset);
        if (!ep || (ep->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) != USB_BM_ATTRIBUTES_XFER_BULK) continue;
        if (ep->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK) {
            d->ep_in = ep->bEndpointAddress;
            d->in_mps = USB_EP_DESC_GET_MPS(ep);
        } else {
            d->ep_out = ep->bEndpointAddress;
        }
    }
    if (!d->ep_in || !d->ep_out || d->in_mps > sizeof(d->in_buf)) return false;
    if (usb_host_interface_claim(client, dev, intf->bInterfaceNumber, intf->bAlternateSetting) != ESP_OK) return false;
    if (usb_host_transfer_alloc(sizeof(raw_buf), 0, &d->xfer_out) != ESP_OK ||
        usb_host_transfer_alloc(d->in_mps, 0, &d->xfer_in) != ESP_OK) {
        dusb_close(d);
        return false;
    }
    d->raw_max = RAW_MAX_CE;
    return true;
}

void dusb_close(dusb_t *d)
{
    if (d->xfer_in) usb_host_transfer_free(d->xfer_in);
    if (d->xfer_out) usb_host_transfer_free(d->xfer_out);
    usb_host_interface_release(d->client, d->dev, 0);
    d->xfer_in = d->xfer_out = NULL;
}

bool dusb_hello(dusb_t *d)
{
    static const uint8_t normal_mode[10] = {0, 3, 0, 1, 0, 0, 0, 0, 0x07, 0xD0};
    uint8_t want[4], type, got[4];
    uint32_t size;

    put32(want, BUF_SIZE_WANTED);
    if (!raw_send(d, RAW_BUF_SIZE_REQ, want, sizeof(want))) return false;
    if (!raw_recv(d, &type, got, sizeof(got), &size) || type != RAW_BUF_SIZE_ALLOC || size != 4) return false;
    d->raw_max = get32(got) > RAW_MAX_CE ? RAW_MAX_CE : get32(got);
    return vpkt_send(d, VPKT_PING, normal_mode, sizeof(normal_mode)) && vpkt_expect(d, VPKT_MODE_SET);
}

bool dusb_os_version(dusb_t *d, uint8_t version[3])
{
    static const uint8_t request[4] = {0, 1, PID_OS_VERSION >> 8, PID_OS_VERSION & 0xFF};
    uint16_t type;
    uint32_t size;

    if (!vpkt_send(d, VPKT_PARM_REQ, request, sizeof(request)) || !vpkt_recv(d, &type, &size)) return false;
    /* count (2) | id (2) | 0 = ok (1) | length (2) | value: the CE's is 4 bytes, the version in the last 3 */
    if (type != VPKT_PARM_DATA || size < 11 || get16(reply) != 1 || reply[4] != 0 || get16(reply + 5) != 4) return false;
    memcpy(version, reply + 8, 3);
    return true;
}

bool dusb_list(dusb_t *d, dusb_var_cb cb, void *arg)
{
    static const uint8_t request[17] = {0, 0, 0, 3,  AID_VAR_SIZE >> 8, AID_VAR_SIZE & 0xFF,  AID_VAR_TYPE >> 8, AID_VAR_TYPE & 0xFF,
                                        AID_ARCHIVED >> 8, AID_ARCHIVED & 0xFF,  0, 1, 0, 1, 0, 1, 1};
    if (!vpkt_send(d, VPKT_DIR_REQ, request, sizeof(request))) return false;
    for (;;) {
        uint16_t type, nattr, a;
        uint32_t size, j = 0;
        char name[DUSB_NAME_MAX] = "";
        uint8_t var_type = 0;
        bool archived = false;

        if (!vpkt_recv(d, &type, &size)) return false;
        if (type == VPKT_EOT) return true;
        if (type != VPKT_VAR_HDR) return false;
        /* folder length | folder + 0 | name length | name + 0 | attribute count | attributes */
        if (reply[j]) j += reply[j] + 1;
        j++;
        if (j >= size) return false;
        if (reply[j]) {
            uint8_t len = reply[j] < DUSB_NAME_MAX - 1 ? reply[j] : DUSB_NAME_MAX - 1;
            memcpy(name, reply + j + 1, len);
            name[len] = 0;
            j += reply[j] + 1;
        }
        j++;
        nattr = get16(reply + j);
        j += 2;
        for (a = 0; a < nattr && j + 3 <= size; a++) {
            /* id (2) | 0 = ok (1) | length (2) | value */
            uint16_t id = get16(reply + j), len;
            bool ok = reply[j + 2] == 0;
            j += 3;
            if (!ok) continue;
            len = get16(reply + j);
            j += 2;
            if (id == AID_VAR_TYPE && len == 4) var_type = reply[j + 3];
            if (id == AID_ARCHIVED && len == 1) archived = reply[j];
            j += len;
        }
        cb(name, var_type, archived, arg);
    }
}

/* RTS: no folder | name length | name + 0 | size | 1 | attributes: type (F0 0F 00 type), archived, and for a variable its version
 * (an app has none: that is how TI Connect CE sends .8ek files) */
static bool send_rts(dusb_t *d, const char *name, uint8_t type, bool with_version, uint8_t version, bool archived, uint32_t size)
{
    uint8_t rts[64];
    size_t n = strlen(name), j = 0;

    if (n > 8) return false;
    rts[j++] = 0;
    rts[j++] = (uint8_t)n;
    memcpy(rts + j, name, n + 1);
    j += n + 1;
    put32(rts + j, size);
    j += 4;
    rts[j++] = 0x01;
    rts[j++] = 0;
    rts[j++] = with_version ? 3 : 2;
    rts[j++] = 0, rts[j++] = AID_VAR_TYPE, rts[j++] = 0, rts[j++] = 4;
    rts[j++] = 0xF0, rts[j++] = 0x0F, rts[j++] = 0x00, rts[j++] = type;
    rts[j++] = 0, rts[j++] = AID_ARCHIVED, rts[j++] = 0, rts[j++] = 1;
    rts[j++] = archived;
    if (with_version) {
        rts[j++] = 0, rts[j++] = AID_VAR_VERSION, rts[j++] = 0, rts[j++] = 4;
        rts[j++] = 0, rts[j++] = 0, rts[j++] = 0, rts[j++] = version;
    }
    return vpkt_send(d, VPKT_RTS, rts, j) && vpkt_expect(d, VPKT_DATA_ACK);
}

static bool send_contents(dusb_t *d, const uint8_t *data, uint32_t size)
{
    if (!vpkt_send(d, VPKT_VAR_CNTS, data, size) || !vpkt_expect(d, VPKT_DATA_ACK)) return false;
    if (!vpkt_send(d, VPKT_EOT, NULL, 0)) return false;
    vTaskDelay(pdMS_TO_TICKS(50));   /* libticalcs waits too ("needed") */
    return true;
}

bool dusb_send_var(dusb_t *d, const char *name, uint8_t type, uint8_t version, bool archived, const uint8_t *data, uint32_t size)
{
    return send_rts(d, name, type, true, version, archived, size) && send_contents(d, data, size);
}

bool dusb_send_app(dusb_t *d, const char *name, const uint8_t *data, uint32_t size)
{
    return send_rts(d, name, 0x24, false, 0, true, size) && send_contents(d, data, size);
}
