#include "ceres/terminal.h"

#define check_status() (read_port(TERM_STATUS, TERM_STATUS_TYPE) & TERM_INPUT_READY)

int term_read_ready(void)
{
    return check_status();
}

int term_bytes_available(void)
{
    return (int)read_port(TERM_BYTES_AVAIL, TERM_BYTES_AVAIL_TYPE);
}

int term_dropped(void)
{
    return (int)read_port(TERM_DROPPED, TERM_DROPPED_TYPE);
}

void term_write_char(int ch)
{
    write_port(TERM_OUT, TERM_OUT_TYPE, (unsigned int)ch);
}

int term_read_char(enum term_read_mode_t mode)
{
    if (mode == TERM_READ_UNTIL_STATUS)
    {
        while (!check_status()) {}
    }
    else if (mode == TERM_READ_UNTIL_ISR)
    {
        while (!check_status())
            wait_irq();
    }
    else if (!check_status())
    {
        return -1;   // nothing available, and we must not block
    }
    return (int)read_port(TERM_IN, TERM_IN_TYPE);
}

void term_write(const char* restrict buf, int len)
{
    if (!buf || len <= 0)
        return;
    write_port(TERM_BLOCK_ADDR, TERM_BLOCK_ADDR_TYPE, (unsigned int)buf);
    write_port(TERM_BLOCK_LEN, TERM_BLOCK_LEN_TYPE, (unsigned int)len);
    write_port(TERM_BLOCK_CMD, TERM_BLOCK_CMD_TYPE, TERM_BLOCK_CMD_WRITE);
}

int term_read(char* buf, int max, enum term_read_mode_t mode)
{
    if (!buf || max <= 0)
        return 0;

    if (mode == TERM_READ_UNTIL_STATUS)
    {
        while (!check_status()) {}
    }
    else if (mode == TERM_READ_UNTIL_ISR)
    {
        while (!check_status())
            wait_irq();
    }

    // Block-transfer up to `max` bytes. The device moves whatever is available
    // and reports the exact count, so a short read is observable (0 when empty).
    write_port(TERM_BLOCK_ADDR, TERM_BLOCK_ADDR_TYPE, (unsigned int)buf);
    write_port(TERM_BLOCK_LEN, TERM_BLOCK_LEN_TYPE, (unsigned int)max);
    write_port(TERM_BLOCK_CMD, TERM_BLOCK_CMD_TYPE, TERM_BLOCK_CMD_READ);
    return (int)read_port(TERM_BLOCK_READ_CNT, TERM_BLOCK_READ_CNT_TYPE);
}
