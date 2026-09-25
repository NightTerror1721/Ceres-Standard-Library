// Tasks and channels (ceres/task.h): cooperative turns, sleeping, joining, messages, and a wait that could never
// end reported instead of hanging.
#include "ceres/test.h"
#include "ceres/task.h"
#include "ceres/heap.h"
#include "stdio.h"
#include "stdlib.h"
#include "errno.h"

static char trace[64];
static int trace_len = 0;

static void note(char c)
{
    if (trace_len < 63)
    {
        trace[trace_len++] = c;
        trace[trace_len] = 0;
    }
}

static void worker(void* arg)
{
    char c = (char)(unsigned int)arg;
    for (int i = 0; i < 3; i++)
    {
        note(c);
        task_yield();
    }
}

static void sleeper(void* arg)
{
    task_sleep_ms((unsigned int)arg);
    note((unsigned int)arg < 20 ? 's' : 'L');
}

static struct chan numbers;
static int received_sum = 0;

static void producer(void* arg)
{
    for (int i = 1; i <= 10; i++)
        chan_send(&numbers, &i);                       // waits whenever the two slots are full
    chan_close(&numbers);
}

static void consumer(void* arg)
{
    int v;
    while (chan_recv(&numbers, &v) == 0)
        received_sum += v;
}

static int deep(int n)
{
    volatile char pad[64];
    pad[0] = (char)n;
    return n == 0 ? pad[0] : deep(n - 1) + pad[0];
}

static int deep_result = 0;
static void uses_its_stack(void* arg)
{
    deep_result = deep(20);                           // 20 frames of 64+ bytes: well within 8 KiB
    void* p = malloc(1000);                           // and malloc works from a task's stack
    deep_result += p != 0;
    free(p);
}

int main(void)
{
    TEST_SECTION("turns");
    CHECK_EQ(task_self(), 0);
    CHECK_EQ(task_count(), 1);
    int a = task_spawn(worker, (void*)'a', 0);
    int b = task_spawn(worker, (void*)'b', 0);
    CHECK(a > 0 && b > 0 && a != b);
    CHECK_EQ(task_count(), 3);
    CHECK_EQ(task_join(a), 0);
    CHECK_EQ(task_join(b), 0);
    printf("%s\n", trace);                            // they took turns: abababb... in order
    CHECK_EQ(task_count(), 1);
    CHECK_EQ(task_alive(a), 0);
    CHECK_EQ(task_join(a), 0);                        // joining a finished task returns at once
    CHECK_EQ(task_join(12345), -1);
    CHECK_EQ(errno, ESRCH);

    TEST_SECTION("sleeping");
    trace_len = 0;
    trace[0] = 0;
    int longer = task_spawn(sleeper, (void*)40u, 0);
    int shorter = task_spawn(sleeper, (void*)10u, 0);
    CHECK_EQ(task_join(longer), 0);
    CHECK_EQ(task_join(shorter), 0);
    printf("%s\n", trace);                            // the shorter sleep ends first

    TEST_SECTION("channels");
    CHECK_EQ(chan_init(&numbers, sizeof(int), 2), 0);
    int p = task_spawn(producer, 0, 0);
    int c = task_spawn(consumer, 0, 0);
    task_join(p);
    task_join(c);
    CHECK_EQ(received_sum, 55);
    int v = 7;
    CHECK_EQ(chan_send(&numbers, &v), -1);            // closed
    CHECK_EQ(errno, EPIPE);
    chan_free(&numbers);
    struct chan lonely;
    CHECK_EQ(chan_init(&lonely, sizeof(int), 1), 0);
    CHECK_EQ(chan_recv(&lonely, &v), -1);             // nothing else exists to ever send
    CHECK_EQ(errno, EDEADLK);
    CHECK_EQ(chan_try_recv(&lonely, &v), -1);
    CHECK_EQ(errno, EAGAIN);
    CHECK_EQ(chan_try_send(&lonely, &v), 0);
    CHECK_EQ(chan_try_send(&lonely, &v), -1);
    CHECK_EQ(errno, EAGAIN);
    CHECK_EQ((int)chan_len(&lonely), 1);
    chan_free(&lonely);
    CHECK_EQ(chan_try_recv(&lonely, &v), -1);         // freed with a message queued: gone, not read from NULL
    CHECK_EQ(errno, EPIPE);

    TEST_SECTION("stacks of their own");
    unsigned int before = heap_used();
    int u = task_spawn(uses_its_stack, 0, 0);
    task_join(u);
    CHECK_EQ(deep_result, 211);                       // 20 + 19 + ... + 0, and 1 for the malloc
    int ids[TASK_MAX];
    int made = 0;
    for (int i = 0; i < TASK_MAX + 2; i++)
    {
        int id = task_spawn(worker, (void*)'x', 4096);   // at -O0 the scheduler's own frames need a few KiB
        if (id < 0) break;
        ids[made++] = id;
    }
    CHECK_EQ(made, TASK_MAX - 1);                      // main holds the first slot
    CHECK_EQ(task_spawn(worker, (void*)'x', 0), -1);
    CHECK_EQ(errno, EAGAIN);
    for (int i = 0; i < made; i++)
        task_join(ids[i]);
    task_yield();                                     // the last finished stack goes once someone else ran
    CHECK_EQ((int)heap_used(), (int)before);          // every stack was given back
    return test_summary();
}
