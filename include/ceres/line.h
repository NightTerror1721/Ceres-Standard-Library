#pragma once

#include "stddef.h"

// Text input for terminal programs: a whole line, a number, a yes/no, a numbered choice.
//
// The host terminal is LINE-ORIENTED: pressing Enter hands the program the whole line at once, several
// bytes in one go, and the terminal's status only says "there is data", not "there is a character".
// So these routines take one byte at a time from the terminal and stop at the end of the line, never
// reading past it: whatever follows the newline stays queued for the next call (or for getchar).
//
// Each call WAITS for a line, or for the end of the input (the host closed stdin: a pipe that ran dry,
// Ctrl-Z / Ctrl-D at a console); term_eof() says which.

// Reads one line WITHOUT its "\n" (a "\r\n" pair counts as one line ending). At most max-1 characters
// are stored and the buffer is always NUL-terminated; a longer line is read to its end and the rest
// discarded. Returns the length stored, or -1 when max < 1 or the input ended before a single character
// (the buffer then holds an empty string). A last line with no newline is still a line.
int  line_read(char* buf, int max);

int  line_prompt(const char* prompt, char* buf, int max);   // prints the prompt, then line_read

// Each prints its prompt and reads a line. read_int/read_float return 0 and set *out when the line is
// a complete number (surrounding blanks allowed), else -1 and leave *out alone - also when the input ended,
// which term_eof() tells apart from a bad line.
int  read_int(const char* prompt, int* out);
int  read_float(const char* prompt, float* out);

// Asks until it gets "y", "yes", "n" or "no" (any case); an empty line means the default.
// Returns 1 for yes, 0 for no (the default, once the input has ended). The prompt is followed by " [Y/n] " or
// " [y/N] ".
int  read_yes_no(const char* prompt, int default_yes);

// Prints the prompt and the options numbered 1..n, and asks until the answer is a number in that
// range. Returns the chosen number (1..n), or -1 when the input ended with no answer.
int  read_choice(const char* prompt, const char* const* options, int n);

int  line_available(void);     // bytes waiting in the terminal (0 means a read would wait)
void line_discard(void);       // throws away everything waiting in the terminal
