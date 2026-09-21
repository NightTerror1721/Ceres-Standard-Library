#include "ceres/keyboard.h"

int kbd_event_ready(void)
{
    return (mmio_r32(KBD_STATUS) & KBD_EVENT_READY) != 0;
}

int kbd_text_ready(void)
{
    return (mmio_r32(KBD_STATUS) & KBD_TEXT_READY) != 0;
}

unsigned int kbd_read_text(void)
{
    return mmio_r32(KBD_TEXT);          // pops one; 0 when the queue is empty
}

unsigned int kbd_read_event(void)
{
    return mmio_r32(KBD_EVENT);         // pops one; 0 when the queue is empty
}

int kbd_is_pressed(unsigned int event)
{
    return (event & KBD_EVENT_PRESSED) != 0;
}

int kbd_keycode(unsigned int event)
{
    return (int)(event & KBD_EVENT_CODE_MASK);
}

int kbd_read_block(unsigned int* events, int max)
{
    if (events == 0 || max <= 0)
        return 0;
    if (max > KBD_QUEUE_SIZE)
        max = KBD_QUEUE_SIZE;           // the device never holds more, and the caller's buffer is sized for `max`
    mmio_w32(KBD_BLOCK_ADDR, (unsigned int)events);
    mmio_w32(KBD_BLOCK_LEN, (unsigned int)max * 4u);
    mmio_w32(KBD_BLOCK_CMD, KBD_BLOCK_CMD_READ);
    return (int)mmio_r32(KBD_BLOCK_READ_CNT);
}

int kbd_flush(void)
{
    unsigned int scratch[KBD_QUEUE_SIZE];
    return kbd_read_block(scratch, KBD_QUEUE_SIZE);
}
