#include "ceres/terminal.h"
#include "ceres/irq.h"
#include "signal.h"

#define check_status() (mmio_r32(TERM_STATUS) & TERM_INPUT_READY)

int term_read_ready(void)
{
    return check_status();
}

int term_eof(void)
{
    return (mmio_r32(TERM_STATUS) & TERM_INPUT_EOF) != 0;
}

int term_interrupted(void)
{
    return (mmio_r32(TERM_STATUS) & TERM_INTERRUPT) != 0;
}

void term_check_interrupt(void)
{
    if (term_interrupted())
    {
        mmio_w32(TERM_INT_ACK, 1u);
        raise(SIGINT);
    }
}

// Waits until a byte can be read (returns 1) or the input has ended (returns 0). The status word is read once
// per turn: the end-of-input bit only sets while nothing is waiting, so "no byte, and the end" is one snapshot.
// Between looks the machine halts: the terminal raises its request for input, for its end and for Ctrl+C, and a
// halt ends on any request, taken or not; one that comes between the look and the halt keeps the halt from
// sleeping (CeresASM 551cdbd). Waiting by interrupt masks them while it looks and sleeps with `sti; halt`, so
// the handler on vector 19 runs for each byte. A Ctrl+C while waiting raises SIGINT.
static int wait_for_input(enum term_read_mode_t mode)
{
    unsigned int was = mode == TERM_READ_UNTIL_ISR ? irq_save() : 0;
    int ready;
    for (;;)
    {
        unsigned int status = mmio_r32(TERM_STATUS);
        if (status & TERM_INTERRUPT)
        {
            if (mode == TERM_READ_UNTIL_ISR)
                irq_restore(was);
            term_check_interrupt();
            if (mode == TERM_READ_UNTIL_ISR)
                was = irq_save();
            continue;
        }
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
        else
            __builtin_halt();
    }
    if (mode == TERM_READ_UNTIL_ISR)
        irq_restore(was);
    return ready;
}

int term_mode(void)
{
    return (int)mmio_r32(TERM_MODE);
}

void term_set_mode(int mode)
{
    mmio_w32(TERM_MODE, (unsigned int)mode);
}

int term_set_raw(int on)
{
    term_set_mode(on ? (TERM_MODE_RAW | TERM_MODE_IRQ) : TERM_MODE_DEFAULT);
    return term_mode();
}

int term_bytes_available(void)
{
    return (int)mmio_r32(TERM_AVAILABLE);
}

int term_cols(void) { return (int)mmio_r32(TERM_COLS); }
int term_rows(void) { return (int)mmio_r32(TERM_ROWS); }

void term_cursor(int* x, int* y)
{
    if (x)
        *x = (int)mmio_r32(TERM_CURSOR_X);
    if (y)
        *y = (int)mmio_r32(TERM_CURSOR_Y);
}

void term_set_cursor(int x, int y)
{
    mmio_w32(TERM_CURSOR_X, (unsigned int)(x < 0 ? 0 : x));
    mmio_w32(TERM_CURSOR_Y, (unsigned int)(y < 0 ? 0 : y));
}

void term_show_cursor(int on)
{
    unsigned int control = mmio_r32(TERM_CONTROL);
    mmio_w32(TERM_CONTROL, on ? (control | TERM_CONTROL_CURSOR) : (control & ~(unsigned int)TERM_CONTROL_CURSOR));
}

void term_write_char(int ch)
{
    mmio_w32(TERM_OUT, (unsigned int)ch);
}

int term_read_char(enum term_read_mode_t mode)
{
    if (mode == TERM_READ_UNTIL_STATUS || mode == TERM_READ_UNTIL_ISR)
    {
        if (!wait_for_input(mode))
            return -1;   // the input ended
    }
    else
    {
        term_check_interrupt();
        if (!check_status())
            return -1;   // nothing available, and we must not block
    }
    return (int)mmio_r32(TERM_IN);
}

void term_write(const char* restrict buf, int len)
{
    if (!buf || len <= 0)
        return;
    mmio_w32(TERM_BLOCK_ADDR, (unsigned int)buf);
    mmio_w32(TERM_BLOCK_LEN, (unsigned int)len);
    mmio_w32(TERM_BLOCK_CMD, TERM_BLOCK_CMD_WRITE);
}

void term_write_error(const char* restrict buf, int len)
{
    if (!buf || len <= 0)
        return;
    mmio_w32(TERM_BLOCK_ADDR, (unsigned int)buf);
    mmio_w32(TERM_BLOCK_LEN, (unsigned int)len);
    mmio_w32(TERM_BLOCK_CMD, TERM_BLOCK_CMD_WRITE_ERR);
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
    else
        term_check_interrupt();

    // Block-transfer up to `max` bytes. The device moves whatever is available
    // and reports the exact count, so a short read is observable (0 when empty).
    mmio_w32(TERM_BLOCK_ADDR, (unsigned int)buf);
    mmio_w32(TERM_BLOCK_LEN, (unsigned int)max);
    mmio_w32(TERM_BLOCK_CMD, TERM_BLOCK_CMD_READ);
    return (int)mmio_r32(TERM_BLOCK_COUNT);
}
