// USE: irq
// Waiting for an interrupt cannot lose it. The timer is armed for every small count, so it expires at every
// point around the sti and the halt - before the flag is read, between the read and the sleep, right on the
// sti, after the halt. The machine takes no interrupt between an sti and the instruction after it, so each
// wait ends. Before that, a wake-up that fell in the gap left the halt asleep with nothing to wake it, and
// the library kept a guard timer around every wait to bound the damage.
#include "ceres/test.h"
#include "ceres/irq.h"
#include "ceres/timer.h"

static volatile int fired;

static void on_timer(int irq)
{
    fired = fired + 1;
}

int main(void)
{
    TEST_SECTION("irq_wait_flag with a timer that expires at every moment");
    irq_attach(IRQ_TIMER, on_timer);
    int all_woke = 1;
    for (unsigned int ticks = 1; ticks <= 40; ticks++)
    {
        fired = 0;
        timer_arm(ticks, 0);
        irq_wait_flag(&fired);                          // would sleep for ever if the wake-up were lost
        if (fired != 1)
            all_woke = 0;
    }
    CHECK(all_woke);
    timer_disarm();

    TEST_SECTION("irq_wait");
    fired = 0;
    timer_arm(30, 0);
    irq_wait();                                         // sti; halt
    CHECK_EQ((int)fired, 1);

    TEST_SECTION("interrupts are put back as they were");
    unsigned int was = irq_save();                      // masked from here
    fired = 0;
    timer_arm(5, 0);
    irq_wait_flag(&fired);
    CHECK_EQ((int)fired, 1);
    CHECK(irq_save() == 0u);                            // irq_wait_flag restored the masked state it found
    irq_restore(was);

    return test_summary();
}
