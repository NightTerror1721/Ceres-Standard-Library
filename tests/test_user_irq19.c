// Regression: a program may bind the terminal's interrupt vector (19) itself while using the standard library.
// terminal.c used to bind it to an empty handler in every link, so this failed with
//   Link error: interrupt 17 is already bound to '__isr_term'
// (17 was the terminal's until the device map of plan/v2 F4.3).
// It only has to link and run; the handler is never entered.
#include "stdio.h"
#include "interrupts.h"

static int terminal_events = 0;

__interrupt void my_terminal_isr(void)
{
    terminal_events++;
}

__interrupt_vector(19, my_terminal_isr);

int main(void)
{
    puts("vector 19 bound by the program");
    return terminal_events;
}
