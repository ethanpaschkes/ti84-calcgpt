#include "calclink.h"

static const struct calclink_ops *ops = &link_jtag;

void calclink_use(const struct calclink_ops *o)
{
    ops = o;
}

void calclink_start(void)
{
    ops->start();
}

int calclink_read(uint8_t *buf, size_t n, TickType_t wait)
{
    return ops->read(buf, n, wait);
}

int calclink_write(const uint8_t *buf, size_t n, TickType_t wait)
{
    return ops->write(buf, n, wait);
}

bool calclink_connected(void)
{
    return ops->connected();
}
