// The one pointer ceres/task.h and ceres/mmu.h share: set by mmu_guard_task_stacks, read by task_spawn. On its own,
// so that neither module links the other.
#include "stddef.h"

void (*__task_stack_guard)(unsigned char* stack, int guard) = NULL;
