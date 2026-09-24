// USE: irq
// The peripheral ports' interrupt (IRQ_PERIPH, 23) through the irq module: a handler attached at run time is
// called when a medium is pulled out. The medium is plugged in by tests/expected/test_periph_irq.ports.
#include "ceres/test.h"
#include "ceres/irq.h"
#include "ceres/periph.h"

static volatile int calls = 0;
static volatile int number = 0;

static void on_periph(int irq)
{
    calls++;
    number = irq;
}

int main(void)
{
    TEST_SECTION("attach");
    CHECK_EQ(irq_attach(IRQ_PERIPH, on_periph), 0);
    CHECK(irq_handler(IRQ_PERIPH) == on_periph);
    CHECK_STR(irq_name(IRQ_PERIPH), "Periph");

    TEST_SECTION("the events of the media plugged in before the start");
    struct periph_event ev;
    int connected = 0;
    while (periph_next_event(&ev))
        if (ev.kind == PERIPH_CONNECTED)
            connected++;
    CHECK_EQ(connected, 1);

    TEST_SECTION("pulling a medium out raises the interrupt");
    irq_enable_all();
    // Connecting the medium before the start raised the interrupt too, and a masked one stays queued: it
    // arrives right after sti. Let it, then count only what the eject raises.
    for (volatile int spin = 0; spin < 10; spin++)
    {
    }
    calls = 0;
    CHECK_EQ(periph_present(0), 1);
    CHECK_EQ(periph_eject(0), 0);
    irq_wait_flag(&calls);                              // the handler sets it; masked while it is read
    CHECK_EQ(calls, 1);
    CHECK_EQ(number, IRQ_PERIPH);
    CHECK_EQ(periph_present(0), 0);
    CHECK_EQ(periph_next_event(&ev), 1);
    CHECK_EQ(ev.kind, PERIPH_DISCONNECTED);
    CHECK_EQ(ev.port, 0);

    irq_detach(IRQ_PERIPH);
    return test_summary();
}
