// Ceres stdlib - low-level device access.
//
// Ceres reaches its devices through memory-mapped I/O at the top of the address
// space (see CeresASM docs/07-IO-Devices-and-Ports.md). These are the addresses
// the terminal and the system-control device live at.

#pragma once

#define TERMINAL_BASE       0xFF000000
#define TIMER_BASE          0xFF010000
#define DISK_BASE           0xFF020000
#define FRAMEBUFFER_BASE    0xFF030000   // text grid
#define DMA_BASE            0xFF040000
#define KEYBOARD_BASE       0xFF050000
#define MOUSE_BASE          0xFF060000
#define DISPLAY_BASE        0xFF070000   // RGB32 pixels
#define GAMEPAD_BASE        0xFF080000
#define AUDIO_BASE          0xFF090000
#define PERIPH_BASE         0xFF0A0000   // plug-in media: sticks and cartridges
#define SYS_CTRL_BASE       0xFFFF0000

// Volatile access with an explicit width. The width is part of the register's contract: the
// terminal's output register wants BYTES, and a 32-bit store there emits the byte plus three
// NULs. Prefer these over write_port(), which leaves the type to the caller.
#define mmio_r8(a)      (*((volatile unsigned char*)(a)))
#define mmio_r16(a)     (*((volatile unsigned short*)(a)))
#define mmio_r32(a)     (*((volatile unsigned int*)(a)))
#define mmio_w8(a, v)   (*((volatile unsigned char*)(a)) = (unsigned char)(v))
#define mmio_w16(a, v)  (*((volatile unsigned short*)(a)) = (unsigned short)(v))
#define mmio_w32(a, v)  (*((volatile unsigned int*)(a)) = (unsigned int)(v))

// The RAM map (CeresASM docs/02-Memory.md).
#define CERES_VECTOR_TABLE  0x00000000   // 64 entries x 4 bytes; entry 0 is the reset vector
#define CERES_BIOS          0x00000100
#define CERES_TEXT_BASE     0x00000400   // where the program image starts
#define CERES_SYSTEM_STACK  4096         // the last 4 KiB of RAM: interrupt handlers run here (CeresASM 331068a)
#define CERES_DEFAULT_RAM   (16 * 1024 * 1024)

#define irq_enable()        __builtin_sti()
#define irq_disable()       __builtin_cli()
#define wait_irq()          __builtin_halt()

// Kept for existing code (terminal.c and friends); new code should use mmio_*.
#define read_port(port, type) (*((volatile type*)(port)))
#define write_port(port, type, value) (*((volatile type*)(port)) = (value))

void sys_exit(void) __attribute__((__noreturn__));      // halt the VM with status 0 (write 1 to the system-control device)
void sys_exit_status(int status) __attribute__((__noreturn__));   // halt it; the low eight bits of status are the exit status of `ceres run`
void sys_reset(void) __attribute__((__noreturn__));     // start the program again (write 2 to the system-control device)
