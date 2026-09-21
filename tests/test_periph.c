// The peripheral ports: media plugged in by the host (tests/expected/test_periph.ports), read and written a sector
// at a time, and told about through events. The stick's file is new for every run; the cartridge is a fixed file.
#include "ceres/test.h"
#include "ceres/periph.h"
#include "ceres/timer.h"
#include "string.h"

static unsigned char buf[PERIPH_SECTOR_SIZE];
static unsigned char pattern[PERIPH_SECTOR_SIZE];

int main(void)
{
    TEST_SECTION("what the machine has");
    CHECK_EQ(periph_ports(), PERIPH_PORTS);
    CHECK_EQ(periph_present(0), 1);
    CHECK_EQ(periph_present(1), 1);
    CHECK_EQ(periph_present(2), 1);
    CHECK_EQ(periph_present(3), 0);
    CHECK_EQ(periph_type(0), PERIPH_STORAGE);
    CHECK_EQ(periph_type(1), PERIPH_STORAGE);
    CHECK_EQ(periph_type(2), PERIPH_CARTRIDGE);
    CHECK_EQ(periph_type(3), PERIPH_NONE);
    CHECK_EQ(periph_protected(0), 0);
    CHECK_EQ(periph_protected(2), 1);
    CHECK_EQ((int)periph_sectors(0), 64);                   // a stick whose file was not there: 64 sectors
    CHECK_EQ((int)periph_sectors(2), 2);                    // the cartridge file is two sectors long
    CHECK_EQ((int)periph_sectors(3), 0);

    TEST_SECTION("the events of what was plugged in before the program began");
    struct periph_event ev;
    for (int port = 0; port < 3; port++)
    {
        CHECK_EQ(periph_next_event(&ev), 1);
        CHECK_EQ(ev.port, port);
        CHECK_EQ(ev.kind, PERIPH_CONNECTED);
    }
    CHECK_EQ(periph_next_event(&ev), 0);                    // and no more

    TEST_SECTION("identifiers");
    CHECK(periph_id(0) != 0u);
    CHECK(periph_id(1) != 0u);
    CHECK(periph_id(0) != periph_id(1));                    // two different files
    CHECK_EQ((int)periph_id(3), 0);

    TEST_SECTION("a sector written to a stick comes back the same");
    for (int i = 0; i < PERIPH_SECTOR_SIZE; i++)
        pattern[i] = (unsigned char)(i * 7 + 3);
    CHECK_EQ(periph_write(0, 5, pattern), 0);
    memset(buf, 0, sizeof buf);
    CHECK_EQ(periph_read(0, 5, buf), 0);
    CHECK_EQ(memcmp(buf, pattern, PERIPH_SECTOR_SIZE), 0);
    CHECK_EQ(periph_read(0, 4, buf), 0);                    // the neighbour was left alone
    CHECK_EQ(buf[0], 0);
    CHECK_EQ(periph_read(1, 5, buf), 0);                    // and so is the other stick
    CHECK_EQ(buf[0], 0);

    TEST_SECTION("several sectors at once");
    unsigned char two[2 * PERIPH_SECTOR_SIZE];
    memset(two, 0xAB, sizeof two);
    CHECK_EQ(periph_write_n(1, 10, two, 2), 0);
    memset(two, 0, sizeof two);
    CHECK_EQ(periph_read_n(1, 10, two, 2), 0);
    CHECK_EQ(two[0], 0xAB);
    CHECK_EQ(two[2 * PERIPH_SECTOR_SIZE - 1], 0xAB);
    CHECK_EQ(periph_write_n(1, 63, two, 2), -1);            // the second one would be past the end

    TEST_SECTION("the cartridge can be read and never written");
    CHECK_EQ(periph_read(2, 0, buf), 0);
    CHECK_EQ(memcmp(buf, "CERES-CART", 10), 0);
    CHECK_EQ(periph_write(2, 0, pattern), -1);
    CHECK_EQ(periph_read(2, 0, buf), 0);
    CHECK_EQ(memcmp(buf, "CERES-CART", 10), 0);             // unchanged
    CHECK_EQ(periph_read(2, 2, buf), -1);                   // past its two sectors

    TEST_SECTION("what cannot be done says so");
    CHECK_EQ(periph_read(0, 64, buf), -1);                  // past the end of the stick
    CHECK_EQ(periph_read(3, 0, buf), -1);                   // an empty port
    CHECK_EQ(periph_read(4, 0, buf), -1);                   // no such port
    CHECK_EQ(periph_read(-1, 0, buf), -1);
    CHECK_EQ(periph_read(0, 0, 0), -1);                     // no buffer
    CHECK_EQ(periph_present(4), 0);
    CHECK_EQ(periph_type(-1), PERIPH_NONE);
    CHECK_EQ(periph_flush(3), -1);
    CHECK_EQ(periph_eject(3), -1);
    CHECK_EQ(periph_read(0, 5, buf), 0);                    // and a failure does not spoil the next call

    TEST_SECTION("flush keeps the stick plugged in");
    CHECK_EQ(periph_flush(0), 0);
    CHECK_EQ(periph_present(0), 1);

    TEST_SECTION("ejecting");
    CHECK_EQ(periph_eject(1), 0);
    CHECK_EQ(periph_present(1), 0);
    CHECK_EQ(periph_type(1), PERIPH_NONE);
    CHECK_EQ(periph_read(1, 0, buf), -1);
    CHECK_EQ(periph_next_event(&ev), 1);
    CHECK_EQ(ev.port, 1);
    CHECK_EQ(ev.kind, PERIPH_DISCONNECTED);
    CHECK_EQ(periph_eject(1), -1);                          // nothing left to eject

    TEST_SECTION("waiting for an event that does not come");
    unsigned int start = timer_millis();
    CHECK_EQ(periph_wait(&ev, 30), 0);
    CHECK(timer_millis_elapsed(start) >= 30u);
    CHECK(timer_millis_elapsed(start) < 2000u);

    return test_summary();
}
