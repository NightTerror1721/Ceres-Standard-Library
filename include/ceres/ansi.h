#pragma once

// ANSI escape sequences: cursor movement, colours, clearing. The terminal device forwards bytes as they
// are to the host's standard output, so these work wherever that terminal understands VT sequences
// (Windows Terminal, VS Code's terminal and most Unix terminals do; the old Windows console needs VT
// enabled). Colour for a text game before the text grid has per-cell attributes.

#define ANSI_RESET     "\x1b[0m"
#define ANSI_BOLD      "\x1b[1m"
#define ANSI_CLEAR     "\x1b[2J\x1b[H"     // clear the screen and go home
#define ANSI_HOME      "\x1b[H"
#define ANSI_HIDE_CUR  "\x1b[?25l"
#define ANSI_SHOW_CUR  "\x1b[?25h"

enum ansi_color { ANSI_BLACK, ANSI_RED, ANSI_GREEN, ANSI_YELLOW, ANSI_BLUE, ANSI_MAGENTA, ANSI_CYAN, ANSI_WHITE };

void ansi_goto(int col, int row);           // 1-based, like the sequence itself
void ansi_fg(int color);                    // an ansi_color
void ansi_bg(int color);
void ansi_fg256(int n);                     // the 256-colour palette
void ansi_bg256(int n);
void ansi_up(int n);
void ansi_down(int n);
void ansi_left(int n);
void ansi_right(int n);
void ansi_clear_line(void);
void ansi_save_cursor(void);
void ansi_restore_cursor(void);
