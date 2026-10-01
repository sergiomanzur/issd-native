#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "issd_campaign.h"

static uint8_t ram[0x20000];
static void word(unsigned address, unsigned value) {
    ram[address] = (uint8_t)value;
    ram[address + 1] = (uint8_t)(value >> 8);
}

static void setup(unsigned kind) {
    memset(ram, 0, sizeof ram);
    word(0x32, 6);
    word(0x70, 12);
    word(0x1648, kind == 1 ? 0x205 : 0x21);
    word(0x1446, kind == 1 ? 0xc37c : 0x9431);
    ram[0x1448] = kind == 1 ? 0x85 : 0x8b;
}
static void settled(unsigned kind) {
    word(0x1430, kind == 1 ? 0x1100 : 0x0f00);
    word(0x1446, kind == 1 ? 0xbeb6 : 0xc3eb);
    ram[0x1448] = kind == 1 ? 0xa4 : 0x86;
}

static void load_ram(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file);
    assert(fread(ram, 1, sizeof ram, file) == sizeof ram);
    assert(fgetc(file) == EOF);
    assert(fclose(file) == 0);
}
int main(int argc, char **argv) {
    if (argc == 5) {
        issd_campaign_reset();
        load_ram(argv[2]);
        assert(issd_campaign_tick(ram, true) == NULL);
        load_ram(argv[3]);
        assert(issd_campaign_tick(ram, true) == NULL);
        load_ram(argv[4]);
        const char *label = issd_campaign_tick(ram, true);
        assert(label);
        assert(strcmp(label, argv[1]) == 0);
        assert(issd_campaign_tick(ram, true) == NULL);
        issd_campaign_reset();
        assert(issd_campaign_tick(ram, true) == NULL);
        assert(issd_campaign_tick(ram, true) == NULL);
        puts("cartridge checkpoint fixture passed");
        return 0;
    }
    issd_campaign_reset();
    assert(issd_campaign_tick(NULL, true) == NULL);
    for (unsigned scene = 0; scene < 256; ++scene) {
        ram[0x32] = 6;
        ram[0x70] = (uint8_t)scene;
        assert(issd_campaign_tick(ram, false) == NULL);
    }
    ram[0x32] = 0;
    ram[0x70] = 0;
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true) == NULL);
    /* Generic gameplay/half time/replay transitions cannot establish a
     * committed tournament event, even when a menu choice is stale. */
    ram[0x32] = 6;
    ram[0x19a2] = 1;
    for (unsigned scene = 0; scene < 12; ++scene) {
        ram[0x70] = (uint8_t)scene;
        assert(issd_campaign_tick(ram, true) == NULL);
        assert(issd_campaign_tick(ram, true) == NULL);
    }
    for (unsigned kind = 1; kind <= 2; ++kind) {
        const char *expected = kind == 1 ? "International Cup setup" : "World Series setup";
        issd_campaign_reset();
        setup(kind);
        assert(issd_campaign_tick(ram, true) == NULL);
        settled(kind);
        assert(issd_campaign_tick(ram, true) == NULL);
        const char *result = issd_campaign_tick(ram, true);
        assert(result != NULL);
        assert(strcmp(result, expected) == 0);
        assert(issd_campaign_tick(ram, true) == NULL);
        setup(kind); /* Back into preparation with a newly selected team. */
        word(0x0da0, 0x3e);
        assert(issd_campaign_tick(ram, true) == NULL);
        settled(kind);
        assert(issd_campaign_tick(ram, true) == NULL);
        assert(issd_campaign_tick(ram, true));
        word(0x164a, 1); /* Editing/progression in the same screen is not an event. */
        assert(issd_campaign_tick(ram, true) == NULL);
        issd_campaign_reset(); /* Restored eligible state must stay quiet. */
        assert(issd_campaign_tick(ram, true) == NULL);
        assert(issd_campaign_tick(ram, true) == NULL);
        setup(kind);
        assert(issd_campaign_tick(ram, true) == NULL);
        settled(kind);
        assert(issd_campaign_tick(ram, true) == NULL);
        assert(issd_campaign_tick(ram, false) == NULL);
        assert(issd_campaign_tick(ram, true) == NULL);
        assert(issd_campaign_tick(ram, true) != NULL);
    }
    issd_campaign_reset();
    memset(ram, 0, sizeof ram);
    assert(issd_campaign_tick(ram, true) == NULL);
    setup(1);
    word(0x1648, 0x204); /* Cartridge cleared setup bit after committing 1-0. */
    word(0x1652, 1);
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true) != NULL);
    assert(issd_campaign_tick(ram, true) == NULL);
    word(0xdc26, 2);
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true) == NULL);
    word(0x70, 8); /* Match is never a result checkpoint. */
    assert(issd_campaign_tick(ram, true) == NULL);
    word(0x70, 0x12); /* Original half-time/full-time statistics. */
    assert(issd_campaign_tick(ram, true) == NULL);
    word(0x70, 0x13); /* Original replay. */
    assert(issd_campaign_tick(ram, true) == NULL);
    word(0x70, 12);
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true) == NULL);
    issd_campaign_reset();
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true) == NULL);
    puts("campaign transition exclusions passed");
    issd_campaign_reset();
    memset(ram, 0, sizeof ram);
    assert(issd_campaign_tick(ram, true) == NULL);
    setup(2);
    word(0x1648, 0x20);
    word(0x1652, 1);
    word(0x1446, 0x9496);
    ram[0x1448] = 0x8b;
    assert(issd_campaign_tick(ram, true) == NULL);
    const char *world = issd_campaign_tick(ram, true);
    assert(world && !strcmp(world, "World Series result"));
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_can_export_password(ram));
    word(0x1460, 1);
    assert(!issd_campaign_can_export_password(ram));
    word(0x1460, 0);
    word(0x1652, 35);
    word(0x1446, 0x94ee);
    assert(issd_campaign_tick(ram, true) == NULL);
    const char *completion = issd_campaign_tick(ram, true);
    assert(completion && !strcmp(completion, "World Series complete"));
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_can_export_password(ram));
    issd_campaign_reset();
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true) == NULL);
    /* The original Cup state machine resets round counters between stages. */
    memset(ram, 0, sizeof ram);
    assert(issd_campaign_tick(ram, true) == NULL);
    setup(1); word(0x1648, 0x204); word(0x1652, 1);
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true));
    word(0x1640, 3);
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true));
    word(0x1640, 2); word(0x1648, 0x4205); word(0x1652, 2);
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true));
    /* Starting another campaign after the real no-campaign state rearms. */
    word(0x1648, 0);
    assert(issd_campaign_tick(ram, true) == NULL);
    setup(1);
    assert(issd_campaign_tick(ram, true) == NULL);
    settled(1);
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true));
    issd_campaign_note_password_import();
    memset(ram, 0, sizeof ram); /* Original Password-to-campaign task transition. */
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true) == NULL);
    setup(2); settled(2);
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true));
    assert(issd_campaign_tick(ram, true) == NULL);
    issd_campaign_reset();
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true) == NULL);
    issd_campaign_note_password_import();
    word(0x1648, 0x20); word(0x1652, 5);
    word(0x1446, 0x9496); ram[0x1448] = 0x8b;
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true));
    assert(issd_campaign_tick(ram, true) == NULL);
    issd_campaign_note_password_import();
    setup(1); word(0x1446, 0x953c); ram[0x1448] = 0x8b;
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true));
    setup(1);
    assert(issd_campaign_tick(ram, true) == NULL);
    settled(1);
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true) == NULL);
    issd_campaign_note_password_import();
    setup(2); word(0x1446, 0x9414); ram[0x1448] = 0x8b;
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true));
    issd_campaign_reset();
    memset(ram, 0, sizeof ram);
    assert(issd_campaign_tick(ram, true) == NULL);
    setup(1); word(0x1648, 0x401c); word(0x1640, 8);
    word(0x1446, 0xd32e); ram[0x1448] = 0x85;
    assert(issd_campaign_tick(ram, true) == NULL);
    assert(issd_campaign_tick(ram, true));
    word(0x1640, 9); ram[0xddce] = 0x3c;
    assert(issd_campaign_tick(ram, true) == NULL);
    const char *cup_completion = issd_campaign_tick(ram, true);
    assert(cup_completion && !strcmp(cup_completion, "International Cup complete"));
    assert(issd_campaign_tick(ram, true) == NULL);
    return 0;
}
