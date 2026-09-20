// Where everything is: the sections of the program, the heap and the stack, before and after some allocations.
// Shows sys_print_layout, the heap statistics and malloc. The addresses depend on how the library and the program
// were compiled, so this example has no expected output.
#include "stdio.h"
#include "stdlib.h"
#include "ceres/sys.h"
#include "ceres/heap.h"

static void show_heap(const char* title)
{
    struct heap_stats h;
    heap_stats(&h);
    printf("%s\n", title);
    printf("  %d block(s), %u bytes in use, %u free inside the heap, largest free block %u\n",
           h.blocks, h.used, h.free_bytes, h.largest_free);
    printf("  the heap spans %#x - %#x (limit %#x); %u bytes remain before the stack\n",
           h.start, h.brk, h.limit, sys_stack_free());
}

int main(void)
{
    sys_print_layout();
    puts("");
    show_heap("at the start");

    void* a = malloc(1000);
    void* b = malloc(50000);
    void* c = malloc(300);
    show_heap("after malloc(1000), malloc(50000), malloc(300)");
    printf("  a=%p b=%p c=%p\n", a, b, c);

    free(b);
    show_heap("after free(b)");
    free(a);
    free(c);
    show_heap("after freeing everything");
    printf("heap_check: %s\n", heap_check() == 0 ? "sound" : "CORRUPT");
    return 0;
}
