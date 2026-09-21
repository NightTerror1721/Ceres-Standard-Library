// USE: irq
// The software vector table: handlers attached at run time, driven by the timer.
#include "ceres/test.h"
#include "ceres/irq.h"
#include "ceres.h"

static volatile int ticks = 0;
static volatile int flag = 0;

static void on_tick(int n)
{
    ticks++;
}

static void on_flag(int n)
{
    flag = 1;
}

int main(void)
{
    TEST_SECTION("attach");
    CHECK_EQ(irq_attach(16, on_tick), 0);
    CHECK(irq_handler(16) == on_tick);
    CHECK_EQ(irq_attach(3, on_tick), -1);         // no stub is bound to a fault number
    CHECK_EQ(irq_attach(23, on_tick), -1);         // the audio device's number, 22, is the last a device raises
    CHECK_EQ(irq_attach(22, on_tick), 0);
    CHECK_STR(irq_name(22), "Audio");
    irq_detach(22);
    CHECK_EQ(irq_attach(-1, on_tick), -1);
    CHECK(irq_handler(3) == 0);
    CHECK_STR(irq_name(16), "Timer");
    CHECK_STR(irq_name(17), "Terminal");
    CHECK_STR(irq_name(6), "AlignmentFault");

    TEST_SECTION("timer interrupts");
    mmio_w32(TIMER_BASE + 0x08, 0x80000000u | 500u);   // periodic, every 500 instructions
    irq_enable_all();
    while (ticks < 5)
        __builtin_halt();
    mmio_w32(TIMER_BASE + 0x08, 0);
    irq_disable();
    CHECK(ticks >= 5);
    int seen = ticks;

    TEST_SECTION("detach");
    irq_detach(16);
    CHECK(irq_handler(16) == 0);
    mmio_w32(TIMER_BASE + 0x08, 300);                 // one shot; nobody is listening
    irq_enable_all();
    __builtin_halt();                                 // the stub is entered, finds no handler, returns
    irq_disable();
    CHECK_EQ(ticks, seen);

    TEST_SECTION("critical sections");
    irq_enable_all();
    unsigned int outer = irq_save();
    CHECK(outer != 0);                                // they were enabled
    unsigned int inner = irq_save();
    CHECK_EQ((int)inner, 0);                          // already masked
    irq_restore(inner);                               // restoring "was masked" leaves them masked
    unsigned int still = irq_save();
    CHECK_EQ((int)still, 0);
    irq_restore(outer);                               // the outer restore turns them back on
    unsigned int after = irq_save();
    CHECK(after != 0);
    irq_restore(after);
    irq_disable();

    TEST_SECTION("wait for a flag");
    irq_attach(16, on_flag);
    mmio_w32(TIMER_BASE + 0x08, 1000);
    irq_wait_flag(&flag);
    CHECK_EQ(flag, 1);
    irq_detach(16);
    return test_summary();
}
