# `<ceres/terminal.h>`

Terminal device (0xFF000000). Character stream: a status register, single-byte input/output, and block transfers. The three statistics registers (bytes available, block-read count, dropped input) were added after the original device - see CeresASM docs/07-IO-Devices-and-Ports.md.

```c
#define TERM_STATUS           (TERMINAL_BASE + 0x00)  // read: bit0 input ready, bit1 output ready, bit2 end of input
#define TERM_OUT              (TERMINAL_BASE + 0x04)  // write: one byte -> stdout
#define TERM_IN               (TERMINAL_BASE + 0x08)  // read: next byte, or 0 if empty
#define TERM_BYTES_AVAIL      (TERMINAL_BASE + 0x0C)  // read: bytes buffered and unread
#define TERM_BLOCK_READ_CNT   (TERMINAL_BASE + 0x10)  // read: bytes the last block read moved
#define TERM_DROPPED          (TERMINAL_BASE + 0x14)  // read: bytes dropped by a full ring
#define TERM_MODE             (TERMINAL_BASE + 0x18)  // write: TERM_MODE_RAW to ask for keys as pressed; read: what was granted
#define TERM_ERR              (TERMINAL_BASE + 0x1C)  // write: one byte -> the error stream (term_write_error uses the block form)
#define TERM_BLOCK_ADDR       (TERMINAL_BASE + 0xF0)
#define TERM_BLOCK_LEN        (TERMINAL_BASE + 0xF4)
#define TERM_BLOCK_CMD        (TERMINAL_BASE + 0xF8)  // write: 1 = read, 2 = write, 3 = write to the error stream

#define TERM_INPUT_READY   0x01
#define TERM_OUTPUT_READY  0x02
#define TERM_INPUT_EOF     0x04   // the host closed the input and every byte it sent has been read

#define TERM_MODE_RAW   0x01   // keys arrive as they are pressed: no line buffering, no echo
#define TERM_MODE_KEYS  0x02   // (granted) they arrive on the keyboard device's key register (keyboard.h)

#define TERM_BLOCK_CMD_READ   0x01
#define TERM_BLOCK_CMD_WRITE  0x02
#define TERM_BLOCK_CMD_WRITE_ERR 0x03   // RAM out to the error stream

// How a read behaves when there is no input yet. The two blocking modes stop waiting when the input ends
// (the host closed stdin and everything it sent has been read): the read then reports no input, as if it
// had been non-blocking.
enum term_read_mode_t
{
    TERM_READ_NON_BLOCKING = 0,  // return immediately (0 / -1 if nothing is there)
    TERM_READ_UNTIL_STATUS = 1,  // look at the status register, halting between looks until input arrives:
                                 // the terminal's request ends the halt whether or not it is taken
    TERM_READ_UNTIL_ISR     = 2  // the same, but with interrupts enabled while halted (sti; halt), so a
                                 // handler on vector 17 runs for each byte (link the irq module, or bind your own)
};

int  term_read_ready(void);       // nonzero when input is available
int  term_eof(void);              // nonzero once the input has ended: closed by the host, and nothing left to read
int  term_bytes_available(void);  // bytes currently buffered and unread
int  term_dropped(void);          // bytes discarded because the ring was full

// Asks the host to hand over keys as they are pressed (on != 0) or to go back to lines (0), and returns what it
// granted: TERM_MODE_RAW | TERM_MODE_KEYS, or 0 when it cannot (the input is a pipe or a file). A console gives
// a program whole lines only once Enter is pressed and keeps the arrow keys for its own line editor; this is how
// a menu gets them. Read the keys with key_get() / key_wait() (ceres/key.h), which also cover the 0 case.
// While raw, the console does not echo: the program draws what it wants shown. The host puts the console
// back when the program ends.
int  term_set_raw(int on);

void term_write_char(int ch);
int  term_read_char(enum term_read_mode_t mode);                  // -1 = no input (non-blocking only)
void term_write(const char* restrict buf, int len);
// The same to the error stream: under `ceres run` the host's stderr, apart from what the program prints (CeresASM
// 1c51ade). stderr, perror, assert and abort write here. An older machine drops it.
void term_write_error(const char* restrict buf, int len);
int  term_read(char* buf, int max, enum term_read_mode_t mode);   // bytes actually read
```
