# `<ceres/periph.h>`

Peripheral ports (0xFF0A0000): media that a person plugs in while the program runs - a memory stick, a game cartridge. See CeresASM docs/07-IO-Devices-and-Ports.md. The disk (ceres/disk.h) is the machine's own internal drive; this is what gets connected to it.

There are PERIPH_PORTS ports. Each is empty, holds a STORAGE medium (512-byte sectors, read and write) or a CARTRIDGE (the same, read only). The host plugs files in before the program starts (`ceres run --port 0=stick.img --cart 1=game.cart`) or while it runs (a file dropped on the window; the debugger's `attach`). Every connection or disconnection is an EVENT: read them with periph_next_event, or wait for one with periph_wait. Media plugged in before the program started have their events waiting for it.

```c
struct periph_event ev;
while (periph_next_event(&ev))
    if (ev.kind == PERIPH_CONNECTED && periph_type(ev.port) == PERIPH_STORAGE)
        periph_read(ev.port, 0, sector);
```

The functions return -1 for a port that does not exist, an empty one, a sector past the end, and a write to a cartridge or to a stick the host has write protected. Nothing here needs an interrupt handler; an event also raises IRQ_PERIPH (ceres/irq.h) for a program that wants to be woken.

```c
#define PERIPH_STATUS_REG   (PERIPH_BASE + 0x00)   // R: bit0 EVENT pending, bit1 READY, bit2 ERROR
#define PERIPH_COUNT_REG    (PERIPH_BASE + 0x04)   // R: how many ports
#define PERIPH_SELECT_REG   (PERIPH_BASE + 0x08)   // RW: the port the registers below are about
#define PERIPH_EVENT_REG    (PERIPH_BASE + 0x0C)   // R: takes the next event; bit31 valid, bits15:8 kind, bits7:0 port
#define PERIPH_CMD_REG      (PERIPH_BASE + 0x10)   // W: 1 eject the selected port's medium, 2 flush it to its file
#define PERIPH_PSTATUS_REG  (PERIPH_BASE + 0x20)   // R: bit0 PRESENT, bit1 WRITE-PROTECTED, bit3 ERROR
#define PERIPH_TYPE_REG     (PERIPH_BASE + 0x24)   // R: 0 nothing, 1 storage, 2 cartridge
#define PERIPH_ID_REG       (PERIPH_BASE + 0x28)   // R: an identifier of the medium; 0 when empty
#define PERIPH_SECTORS_REG  (PERIPH_BASE + 0x2C)   // R: how many sectors it has
#define PERIPH_SECTOR_REG   (PERIPH_BASE + 0x30)   // RW: the sector a transfer is about
#define PERIPH_BLOCK_ADDR   (PERIPH_BASE + 0xF0)
#define PERIPH_BLOCK_LEN    (PERIPH_BASE + 0xF4)
#define PERIPH_BLOCK_CMD    (PERIPH_BASE + 0xF8)   // 1 reads the selected sector into RAM, 2 writes it

#define PERIPH_PORTS        4
#define PERIPH_SECTOR_SIZE  512

// what a port holds
#define PERIPH_NONE         0
#define PERIPH_STORAGE      1
#define PERIPH_CARTRIDGE    2

// what an event says
#define PERIPH_CONNECTED    1
#define PERIPH_DISCONNECTED 2

struct periph_event
{
    int port;     // which port
    int kind;     // PERIPH_CONNECTED or PERIPH_DISCONNECTED
};

int          periph_ports(void);                    // how many ports the machine has
int          periph_present(int port);              // 1 when something is plugged in, else 0 (also 0 for a port that does not exist)
int          periph_type(int port);                 // PERIPH_NONE, PERIPH_STORAGE or PERIPH_CARTRIDGE
int          periph_protected(int port);            // 1 when it cannot be written: a cartridge, or a stick the host has protected
unsigned int periph_id(int port);                   // the same medium gives the same identifier every time; 0 when the port is empty
unsigned int periph_sectors(int port);              // how many sectors it holds; 0 when empty

int periph_read(int port, unsigned int sector, void* buf);          // 512 bytes; 0 on success, -1 on error
int periph_write(int port, unsigned int sector, const void* buf);   // 512 bytes; 0 on success, -1 on error
int periph_read_n(int port, unsigned int sector, void* buf, unsigned int count);          // `count` sectors, one after another
int periph_write_n(int port, unsigned int sector, const void* buf, unsigned int count);   // stops at the first error
int periph_eject(int port);                         // pull the medium out, as a person would: its file is saved and an event queued
int periph_flush(int port);                         // save the medium's changes to its host file now; 0 ok, -1 error

int periph_next_event(struct periph_event* ev);     // 1 and fills `ev` when there was an event to take, 0 when none
int periph_wait(struct periph_event* ev, unsigned int timeout_ms);   // waits for one; 1 when it came, 0 on timeout (0 ms waits for ever)
```
