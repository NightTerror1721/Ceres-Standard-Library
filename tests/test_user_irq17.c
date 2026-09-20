// Regression: a program may bind interrupt vector 17 itself while using the standard library.
// terminal.c used to bind it to an empty handler in every link, so this failed with
//   Link error: interrupt 17 is already bound to '__isr_term'
// It only has to link and run; the handler is never entered.
#include "stdio.h"
#include "interrupts.h"

static int terminal_events = 0;

__interrupt void my_terminal_isr(void)
{
    terminal_events++;
}

__interrupt_vector(17, my_terminal_isr);

int main(void)
{
    puts("vector 17 bound by the program");
    return terminal_events;
}
