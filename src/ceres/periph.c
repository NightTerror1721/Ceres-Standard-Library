// Peripheral ports. See ceres/periph.h.
#include "ceres/periph.h"
#include "ceres/timer.h"

// Points the port registers at `port`; false when the device says there is no such port.
static int select_port(int port)
{
    if (port < 0 || port >= PERIPH_PORTS)
        return 0;
    mmio_w32(PERIPH_SELECT_REG, (unsigned int)port);
    return (mmio_r32(PERIPH_STATUS_REG) & 4u) == 0;         // bit 2: the error bit
}

int periph_ports(void)
{
    return (int)mmio_r32(PERIPH_COUNT_REG);
}

int periph_present(int port)
{
    if (!select_port(port))
        return 0;
    return (int)(mmio_r32(PERIPH_PSTATUS_REG) & 1u);
}

int periph_type(int port)
{
    if (!select_port(port))
        return PERIPH_NONE;
    return (int)mmio_r32(PERIPH_TYPE_REG);
}

int periph_protected(int port)
{
    if (!select_port(port))
        return 0;
    return (int)((mmio_r32(PERIPH_PSTATUS_REG) >> 1) & 1u);
}

unsigned int periph_id(int port)
{
    if (!select_port(port))
        return 0;
    return mmio_r32(PERIPH_ID_REG);
}

unsigned int periph_sectors(int port)
{
    if (!select_port(port))
        return 0;
    return mmio_r32(PERIPH_SECTORS_REG);
}

static int transfer(int port, unsigned int sector, void* buf, unsigned int command)
{
    if (buf == 0 || !select_port(port))
        return -1;
    mmio_w32(PERIPH_SECTOR_REG, sector);
    mmio_w32(PERIPH_BLOCK_ADDR, (unsigned int)buf);
    mmio_w32(PERIPH_BLOCK_LEN, BLOCKDEV_SECTOR);
    mmio_w32(PERIPH_BLOCK_CMD, command);
    return (mmio_r32(PERIPH_STATUS_REG) & 4u) ? -1 : 0;
}

int periph_read(int port, unsigned int sector, void* buf)
{
    return transfer(port, sector, buf, 1);
}

int periph_write(int port, unsigned int sector, const void* buf)
{
    return transfer(port, sector, (void*)buf, 2);
}

int periph_read_n(int port, unsigned int sector, void* buf, unsigned int count)
{
    unsigned char* p = (unsigned char*)buf;
    for (unsigned int i = 0; i < count; i++)
    {
        if (periph_read(port, sector + i, p + i * BLOCKDEV_SECTOR) != 0)
            return -1;
    }
    return 0;
}

int periph_write_n(int port, unsigned int sector, const void* buf, unsigned int count)
{
    const unsigned char* p = (const unsigned char*)buf;
    for (unsigned int i = 0; i < count; i++)
    {
        if (periph_write(port, sector + i, p + i * BLOCKDEV_SECTOR) != 0)
            return -1;
    }
    return 0;
}

static int command(int port, unsigned int cmd)
{
    if (!select_port(port))
        return -1;
    mmio_w32(PERIPH_CMD_REG, cmd);
    return (mmio_r32(PERIPH_STATUS_REG) & 4u) ? -1 : 0;
}

int periph_eject(int port)
{
    return command(port, 1u);
}

int periph_flush(int port)
{
    return command(port, 2u);
}

int periph_next_event(struct periph_event* ev)
{
    unsigned int raw = mmio_r32(PERIPH_EVENT_REG);
    if ((raw & 0x80000000u) == 0)
        return 0;
    if (ev != 0)
    {
        ev->port = (int)(raw & 0xFFu);
        ev->kind = (int)((raw >> 8) & 0xFFu);
    }
    return 1;
}

int periph_wait(struct periph_event* ev, unsigned int timeout_ms)
{
    if (periph_next_event(ev))
        return 1;
    // A connection raises a request (IRQ_PERIPH), which ends a halt whether it is taken or not, so this sleeps
    // instead of spinning; the timeout is the timer's alarm. 0 ms waits for ever (a halt with nothing scheduled
    // is woken by the host, and by the next request).
    uint64_t deadline = timeout_ms != 0 ? timer_nanos64() + (uint64_t)timeout_ms * 1000000u : 0;
    for (;;)
    {
        if (deadline != 0 && timer_halt_until_ns(deadline))
            return periph_next_event(ev) ? 1 : 0;    // the timeout came
        if (periph_next_event(ev))
            return 1;
        if (deadline == 0)
            __builtin_halt();
    }
}
