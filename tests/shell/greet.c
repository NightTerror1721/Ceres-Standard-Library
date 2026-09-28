// A program for the shell's sessions (tools/runtests: built into the host directory as games/greet.cres): it prints
// the arguments it was started with and the PWD the shell gave it, and ends with the status its first argument
// names - so a session sees what `run` passed on, and what the shell says when the program comes back.
#include "stdio.h"
#include "stdlib.h"

int main(int argc, char** argv)
{
    printf("greet:");
    for (int i = 0; i < argc; i++)
        printf(" [%s]", argv[i]);
    const char* pwd = getenv("PWD");
    printf(" PWD=%s\n", pwd ? pwd : "(none)");
    return argc > 1 ? atoi(argv[1]) : 0;
}
