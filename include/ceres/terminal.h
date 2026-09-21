#pragma once

#include "../ceres.h"

// Terminal device (0xFF000000). Character stream: a status register, single-byte
// input/output, and block transfers. The three statistics registers
// (bytes available, block-read count, dropped input) were added after the
// original device - see CeresASM docs/07-IO-Devices-and-Ports.md.

#define TERM_STATUS           (TERMINAL_BASE + 0x00)  // read: bit0 input ready, bit1 output ready, bit2 end of input
#define TERM_OUT              (TERMINAL_BASE + 0x04)  // write: one byte -> stdout
#define TERM_IN               (TERMINAL_BASE + 0x08)  // read: next byte, or 0 if empty
#define TERM_BYTES_AVAIL      (TERMINAL_BASE + 0x0C)  // read: bytes buffered and unread
#define TERM_BLOCK_READ_CNT   (TERMINAL_BASE + 0x10)  // read: bytes the last block read moved
#define TERM_DROPPED          (TERMINAL_BASE + 0x14)  // read: bytes dropped by a full ring
#define TERM_BLOCK_ADDR       (TERMINAL_BASE + 0xF0)
#define TERM_BLOCK_LEN        (TERMINAL_BASE + 0xF4)
#define TERM_BLOCK_CMD        (TERMINAL_BASE + 0xF8)  // write: 1 = read, 2 = write

#define TERM_STATUS_TYPE          unsigned int
// The output register takes BYTES. Storing an unsigned int here writes the character plus three
// NULs, so every putchar() would put four bytes on stdout.
#define TERM_OUT_TYPE             unsigned char
#define TERM_IN_TYPE              unsigned int
#define TERM_BYTES_AVAIL_TYPE     unsigned int
#define TERM_BLOCK_READ_CNT_TYPE  unsigned int
#define TERM_DROPPED_TYPE         unsigned int
#define TERM_BLOCK_ADDR_TYPE      unsigned int
#define TERM_BLOCK_LEN_TYPE       unsigned int
#define TERM_BLOCK_CMD_TYPE       unsigned int

#define TERM_INPUT_READY   0x01
#define TERM_OUTPUT_READY  0x02
#define TERM_INPUT_EOF     0x04   // the host closed the input and every byte it sent has been read

#define TERM_BLOCK_CMD_READ   0x01
#define TERM_BLOCK_CMD_WRITE  0x02

// How a read behaves when there is no input yet. The two blocking modes stop waiting when the input ends
// (the host closed stdin and everything it sent has been read): the read then reports no input, as if it
// had been non-blocking.
enum term_read_mode_t
{
    TERM_READ_NON_BLOCKING = 0,  // return immediately (0 / -1 if nothing is there)
    TERM_READ_UNTIL_STATUS = 1,  // spin on the status register until input arrives
    TERM_READ_UNTIL_ISR     = 2  // halt until the terminal interrupt wakes us (needs irq_enable AND a
                                 // handler on vector 17: link the irq module, or bind your own - a halt
                                 // is only woken by an interrupt that is actually dispatched)
};

int  term_read_ready(void);       // nonzero when input is available
int  term_eof(void);              // nonzero once the input has ended: closed by the host, and nothing left to read
int  term_bytes_available(void);  // bytes currently buffered and unread
int  term_dropped(void);          // bytes discarded because the ring was full

void term_write_char(int ch);
int  term_read_char(enum term_read_mode_t mode);                  // -1 = no input (non-blocking only)
void term_write(const char* restrict buf, int len);
int  term_read(char* buf, int max, enum term_read_mode_t mode);   // bytes actually read
