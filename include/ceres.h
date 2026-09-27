// Ceres stdlib - low-level device access.
//
// Ceres reaches its devices through memory-mapped I/O at the top of the address
// space (see CeresASM docs/07-IO-Devices-and-Ports.md): 256 slots of 64 KiB from
// 0xFF000000, by group - 0x00 system, 0x10 input, 0x20 audio, 0x30 storage,
// 0x40 video, 0xFF control. These are the addresses each device lives at.

#pragma once

#define TERMINAL_BASE       0xFF000000
#define TIMER_BASE          0xFF010000
#define DMA_BASE            0xFF020000
#define DEBUGLOG_BASE       0xFF030000   // the debug log: lines for the host's log, not the terminal (ceres/debug.h)
#define KEYBOARD_BASE       0xFF100000
#define MOUSE_BASE          0xFF110000
#define GAMEPAD_BASE        0xFF120000
#define AUDIO_BASE          0xFF200000
#define DISK_BASE           0xFF300000
#define HOSTFS_BASE         0xFF310000   // the host's files under --host-dir (ceres/hostfs.h)
#define PERIPH_BASE         0xFF320000   // plug-in media: sticks and cartridges
#define GPU_BASE            0xFF400000   // the GPU: the screen, its text and bitmap planes, the copy engine (ceres/video.h)
#define BLITTER_BASE        0xFF460000   // 2D rectangle operations (ceres/blitter.h; until the GPU's 2D engine replaces it)
#define VRAM_BASE           0xA0000000   // the video memory: up to 1 GiB, as much as the machine has (ceres/sys.h: sys_vram_size)
#define SYS_CTRL_BASE       0xFFFF0000

// A device register is 32 bits wide and takes 32-bit accesses only: a byte or halfword access to one
// is a fault. A register that carries a byte, like the terminal's output, uses the word's low byte.
#define mmio_r32(a)     (*((volatile unsigned int*)(a)))
#define mmio_w32(a, v)  (*((volatile unsigned int*)(a)) = (unsigned int)(v))

// The RAM map (CeresASM docs/02-Memory.md).
#define CERES_VECTOR_TABLE  0x00000000   // 64 entries x 4 bytes; entry 0 is the reset vector
#define CERES_BIOS          0x00000100
#define CERES_TEXT_BASE     0x00000400   // where the program image starts
#define CERES_SYSTEM_STACK  4096         // the last 4 KiB of RAM: interrupt handlers run here (CeresASM 331068a)
#define CERES_DEFAULT_RAM   (64 * 1024 * 1024)   // the standard profile's, what `ceres run` gives when asked for nothing

#define irq_enable()        __builtin_sti()
#define irq_disable()       __builtin_cli()
#define wait_irq()          __builtin_halt()

// Kept for existing code (terminal.c and friends); new code should use mmio_*.
#define read_port(port)            mmio_r32(port)
#define write_port(port, value)     mmio_w32(port, value)

void sys_exit(void) __attribute__((__noreturn__));      // halt the VM with status 0 (write 1 to the system-control device)
void sys_exit_status(int status) __attribute__((__noreturn__));   // halt it; the low eight bits of status are the exit status of `ceres run`
void sys_reset(void) __attribute__((__noreturn__));     // start the program again (write 2 to the system-control device)
