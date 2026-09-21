#include "ceres/terminal.h"
#include "ceres/irq.h"

#define check_status() (read_port(TERM_STATUS, TERM_STATUS_TYPE) & TERM_INPUT_READY)

int term_read_ready(void)
{
    return check_status();
}

int term_eof(void)
{
    return (read_port(TERM_STATUS, TERM_STATUS_TYPE) & TERM_INPUT_EOF) != 0;
}

// Waits until a byte can be read (returns 1) or the input has ended (returns 0). The status word is read once
// per turn: the end-of-input bit only sets while the ring is empty, so "no byte, and the end" is one snapshot.
// Waiting by interrupt masks them while it looks, and sleeps with `sti; halt`: a byte that arrives between the
// look and the sleep wakes the halt, it is not lost.
static int wait_for_input(enum term_read_mode_t mode)
{
    unsigned int was = mode == TERM_READ_UNTIL_ISR ? irq_save() : 0;
    int ready;
    for (;;)
    {
        unsigned int status = read_port(TERM_STATUS, TERM_STATUS_TYPE);
        if (status & TERM_INPUT_READY)
        {
            ready = 1;
            break;
        }
        if (status & TERM_INPUT_EOF)
        {
            ready = 0;
            break;
        }
        if (mode == TERM_READ_UNTIL_ISR)
        {
            irq_wait();
            __builtin_cli();
        }
    }
    if (mode == TERM_READ_UNTIL_ISR)
        irq_restore(was);
    return ready;
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
    if (mode == TERM_READ_UNTIL_STATUS || mode == TERM_READ_UNTIL_ISR)
    {
        if (!wait_for_input(mode))
            return -1;   // the input ended
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

    if (mode == TERM_READ_UNTIL_STATUS || mode == TERM_READ_UNTIL_ISR)
    {
        if (!wait_for_input(mode))
            return 0;    // the input ended
    }

    // Block-transfer up to `max` bytes. The device moves whatever is available
    // and reports the exact count, so a short read is observable (0 when empty).
    write_port(TERM_BLOCK_ADDR, TERM_BLOCK_ADDR_TYPE, (unsigned int)buf);
    write_port(TERM_BLOCK_LEN, TERM_BLOCK_LEN_TYPE, (unsigned int)max);
    write_port(TERM_BLOCK_CMD, TERM_BLOCK_CMD_TYPE, TERM_BLOCK_CMD_READ);
    return (int)read_port(TERM_BLOCK_READ_CNT, TERM_BLOCK_READ_CNT_TYPE);
}
