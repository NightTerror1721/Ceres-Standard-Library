// Running another program in place of this one: the system-control device's command 3 (CeresASM plan/v2 SPEC 5.7).
// Kept apart from sys.c because it flushes stdio first, and a program that only stops the machine should not carry
// stdio for it.
#include "ceres/sys.h"
#include "ceres.h"
#include "stdio.h"

int sys_run(const char* path, int argc, char** argv, char** envp)
{
    // What is still in a stream's buffer would be lost with the program: the machine restarts with the new image.
    fflush(0);

    // The block command 3 reads: argc, argv and envp (0 keeps this program's environment). Static, so it is in RAM
    // and not only in registers when the device reads it.
    static unsigned int block[3];
    block[0] = (unsigned int)argc;
    block[1] = (unsigned int)argv;
    block[2] = (unsigned int)envp;
    mmio_w32(SYS_CTRL_LOAD_PATH, (unsigned int)path);
    mmio_w32(SYS_CTRL_LOAD_ARGS, argv != 0 ? (unsigned int)block : 0u);
    mmio_w32(SYS_CTRL_CMD, 3u);
    return -1;                                   // still here: the load failed
}
