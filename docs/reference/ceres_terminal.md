# `<ceres/terminal.h>`

The terminal (0xFF000000): the program's standard input, output and error. It is the machine's own, drawn in the window's text plane - never the host's terminal (CeresASM plan/v2 SPEC 8). What the program writes is UTF-8 with the usual controls and ANSI sequences (ceres/ansi.h); what the error stream gets is drawn in its own colour. Input comes from the window's keyboard through a line discipline: by default a line is edited on the screen (echoed, with Backspace, the arrows, a history) and handed over whole with Enter; in RAW mode every key comes at once, as the bytes a terminal sends (ceres/key.h decodes them). Ctrl+D on an empty line is the end of the input; Ctrl+C interrupts: the reads here turn it into raise(SIGINT) (signal.h). Without a window the input is what `ceres run --type` or `--keys` typed, and then it ends.

```c
#define TERM_STATUS        (TERMINAL_BASE + 0x00)  // R: TERM_INPUT_READY, TERM_OUTPUT_READY, TERM_INPUT_EOF, TERM_INTERRUPT
#define TERM_OUT           (TERMINAL_BASE + 0x04)  // W: one byte to the output
#define TERM_IN            (TERMINAL_BASE + 0x08)  // R: the next input byte, or 0 if there is none
#define TERM_AVAILABLE     (TERMINAL_BASE + 0x0C)  // R: input bytes waiting
#define TERM_MODE          (TERMINAL_BASE + 0x10)  // RW: TERM_MODE_RAW, TERM_MODE_ECHO, TERM_MODE_HISTORY, TERM_MODE_IRQ
#define TERM_ERR           (TERMINAL_BASE + 0x14)  // W: one byte to the output, in the error colour
#define TERM_CONTROL       (TERMINAL_BASE + 0x18)  // RW: TERM_CONTROL_ON, _CURSOR, _SCROLLBACK, _AUTOSCROLL
#define TERM_COLS          (TERMINAL_BASE + 0x1C)  // R: columns of the screen
#define TERM_ROWS          (TERMINAL_BASE + 0x20)  // R: rows
#define TERM_CURSOR_X      (TERMINAL_BASE + 0x24)  // RW: the cursor's column
#define TERM_CURSOR_Y      (TERMINAL_BASE + 0x28)  // RW: its row
#define TERM_INT_ACK       (TERMINAL_BASE + 0x2C)  // W: 1 clears a pending Ctrl+C
#define TERM_BLOCK_ADDR    (TERMINAL_BASE + 0xF0)
#define TERM_BLOCK_LEN     (TERMINAL_BASE + 0xF4)
#define TERM_BLOCK_CMD     (TERMINAL_BASE + 0xF8)  // W: TERM_BLOCK_CMD_WRITE, _READ or _WRITE_ERR
#define TERM_BLOCK_COUNT   (TERMINAL_BASE + 0xFC)  // R: bytes the last block moved

#define TERM_INPUT_READY   0x01
#define TERM_OUTPUT_READY  0x02
#define TERM_INPUT_EOF     0x04   // the input has ended (Ctrl+D, or nothing more will come) and everything was read
#define TERM_INTERRUPT     0x08   // Ctrl+C was pressed and not yet acknowledged

#define TERM_MODE_RAW      0x01   // every key at once, no editing, no echo
#define TERM_MODE_ECHO     0x02   // what is typed is shown (cooked mode)
#define TERM_MODE_HISTORY  0x04   // Up and Down recall earlier lines (cooked mode)
#define TERM_MODE_IRQ      0x08   // interrupt 19 when input comes
#define TERM_MODE_DEFAULT  (TERM_MODE_ECHO | TERM_MODE_HISTORY | TERM_MODE_IRQ)

#define TERM_CONTROL_ON          0x01   // the terminal draws on the screen
#define TERM_CONTROL_CURSOR      0x02   // its cursor shows
#define TERM_CONTROL_SCROLLBACK  0x04   // rows that scroll off are kept (Shift+PageUp shows them)
#define TERM_CONTROL_AUTOSCROLL  0x08   // a line past the bottom scrolls the screen

#define TERM_BLOCK_CMD_WRITE      0x01
#define TERM_BLOCK_CMD_READ       0x02
#define TERM_BLOCK_CMD_WRITE_ERR  0x03   // RAM out to the error stream

// How a read behaves when there is no input yet. The two blocking modes stop waiting when the input ends: the
// read then reports no input, as if it had been non-blocking.
enum term_read_mode_t
{
    TERM_READ_NON_BLOCKING = 0,  // return immediately (0 / -1 if nothing is there)
    TERM_READ_UNTIL_STATUS = 1,  // look at the status register, halting between looks until input arrives:
                                 // the terminal's request ends the halt whether or not it is taken
    TERM_READ_UNTIL_ISR     = 2  // the same, but with interrupts enabled while halted (sti; halt), so a
                                 // handler on vector 19 runs for each byte (link the irq module, or bind your own)
};

int  term_read_ready(void);       // nonzero when input is available
int  term_eof(void);              // nonzero once the input has ended and nothing is left to read
int  term_bytes_available(void);  // bytes waiting to be read

// Keys as they are pressed (on != 0: raw mode) or lines again (0), and returns the mode now. In raw mode nothing
// is echoed: the program draws what it wants shown. Read the keys with key_get() / key_wait() (ceres/key.h).
int  term_set_raw(int on);
int  term_mode(void);             // TERM_MODE_* as they stand
void term_set_mode(int mode);     // all of them at once

// Ctrl+C: nonzero when one is pending. term_check_interrupt() acknowledges a pending one and raises SIGINT - the
// reads below call it, so a program waiting for input is interrupted; one that never reads can call it itself.
int  term_interrupted(void);
void term_check_interrupt(void);

// The screen the terminal draws on, and its cursor (0-based). The cursor can be moved with ANSI sequences too.
int  term_cols(void);
int  term_rows(void);
void term_cursor(int* x, int* y);
void term_set_cursor(int x, int y);
void term_show_cursor(int on);

void term_write_char(int ch);
int  term_read_char(enum term_read_mode_t mode);                  // -1 = no input (non-blocking only)
void term_write(const char* restrict buf, int len);
// The same to the error stream: stderr, perror, assert and abort write here. It is drawn in the error colour and
// kept apart in `ceres run --transcript`.
void term_write_error(const char* restrict buf, int len);
int  term_read(char* buf, int max, enum term_read_mode_t mode);   // bytes actually read
```
