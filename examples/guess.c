// Guess the number: the program picks one from 1 to 100 and says "too low" or "too high" until you get it.
// Shows line input (read_int) and rand. The generator is seeded with a constant so a game can be replayed,
// and so the example has an expected output; a real game would seed from the clock: srand(time(0)).
#include "stdio.h"
#include "stdlib.h"
#include "ceres/line.h"

int main(void)
{
    srand(2026);
    int secret = rand() % 100 + 1;
    int tries = 0;

    puts("I am thinking of a number from 1 to 100.");
    for (;;)
    {
        int guess;
        if (read_int("guess> ", &guess) != 0)
        {
            puts("that is not a number");
            continue;
        }
        tries++;
        if (guess < secret)
            puts("too low");
        else if (guess > secret)
            puts("too high");
        else
            break;
    }
    printf("yes, it was %d! you needed %d %s.\n", secret, tries, tries == 1 ? "guess" : "guesses");
    return 0;
}
