#include "issd_gameplay.h"
#include <assert.h>
#include <string.h>

static unsigned char ram[0x20000], original[0x20000];
static void put(unsigned a, int v) { ram[a] = (unsigned char)v; ram[a+1] = (unsigned char)(v >> 8); }
static int get(unsigned a) { return ram[a] | ram[a+1] << 8; }
static void fixture(void) {
    memset(ram, 0, sizeof ram);
    put(0x32, 6); put(0x70, 8); put(0x12a2, 2048); put(0x12a4, 640);
    put(0x530, 1); put(0x560, 0x2000); put(0x550, 16); put(0x552, 320);
    put(0x42a, 96); put(0x42c, 288); put(0x424, -8); put(0x428, 2);
    put(0x630, 1); put(0x69a, 0xd00); put(0x668, 1);
    put(0x660, 0x2381); /* The status gate used by the keeper is shared by players. */
    put(0x650, 600); put(0x652, 320);
    ram[0x3f91] = 1; ram[0x3e0a] = 0x55; ram[0x3e0c] = 0x60; ram[0x3e12] = 2;
    ram[0xd000] = 12; ram[0xd001] = 20; ram[0xd280] = 1;
}
int main(void) {
    fixture(); memcpy(original, ram, sizeof ram);
    assert(!issd_gameplay_goalkeeper(ram, 0x500, false));
    assert(!issd_gameplay_player(ram, 0x600, false));
    assert(!memcmp(original, ram, sizeof ram));
    assert(issd_gameplay_goalkeeper(ram, 0x500, true));
    assert(get(0x552) < 320 && get(0x552) >= 308);
    /* Only the target changes: no teleport, random state or animation writes. */
    original[0x552] = ram[0x552]; original[0x553] = ram[0x553];
    assert(!memcmp(original, ram, sizeof ram));
    assert(issd_gameplay_player(ram, 0x600, true));
    int first = get(0x652); assert(first > 320 && first <= 350);
    assert(ram[0x662] == 5 && ram[0x666] == 4 && ram[0x667] == 6);
    /* Changing formation changes shape at the next decision. */
    put(0x650, 600); put(0x652, 320); ram[0xda6] = 1;
    ram[0xd014] = (unsigned char)-12; ram[0xd015] = (unsigned char)-20; ram[0xd28a] = 3;
    assert(issd_gameplay_player(ram, 0x600, true)); assert(get(0x652) < 320);
    /* A bench player's native stats and condition determine response speed. */
    put(0x650, 600); put(0x652, 320); ram[0x3f91] = 12;
    ram[0x3e78] = 0x82; ram[0x3e7a] = 0x30; ram[0x3e80] = 0;
    assert(issd_gameplay_player(ram, 0x600, true));
    assert(ram[0x662] == 0 && ram[0x666] == 5 && ram[0x667] == 3);
    assert(320-get(0x652) <= 8);
    /* Stack order does not affect either independent policy. */
    fixture(); issd_gameplay_goalkeeper(ram, 0x500, true); issd_gameplay_player(ram, 0x600, true);
    memcpy(original, ram, sizeof ram); fixture();
    issd_gameplay_player(ram, 0x600, true); issd_gameplay_goalkeeper(ram, 0x500, true);
    assert(!memcmp(original, ram, sizeof ram));
    /* Mirrored keeper tracks the opposite incoming direction. */
    fixture(); memcpy(ram+0x1000, ram+0x500, 0x100); put(0x106e, 1);
    put(0x1050, 2032); put(0x42a, 1952); put(0x424, 12);
    assert(issd_gameplay_goalkeeper(ram, 0x1000, true)); assert(get(0x1052) < 320);
    /* Mirrored outfield formation uses the second team's runtime tables. */
    fixture(); memcpy(ram+0x1100, ram+0x600, 0x100); put(0x119a, 0xe00); put(0x116e, 1);
    put(0x1150, 1448); put(0x1152, 320); ram[0x3fa5] = 1;
    memcpy(ram+0x3ed2, ram+0x3e0a, 10); ram[0xd140] = 12; ram[0xd141] = 20; ram[0xd320] = 1;
    assert(issd_gameplay_player(ram, 0x1100, true)); assert(get(0x1152) < 320);
    /* Selected humans, inactive slots, paused play and outgoing balls are untouched. */
    fixture(); put(0x66e, 0x80); put(0x1acc, 0x600); put(0x1ace, 0); put(0x59e, 0x8000); memcpy(original, ram, sizeof ram);
    assert(!issd_gameplay_player(ram, 0x600, true)); assert(!issd_gameplay_goalkeeper(ram, 0x500, true));
    assert(!memcmp(original, ram, sizeof ram));
    /* Selected CPU actors still receive formation support. */
    fixture(); put(0x66e, 0x80); put(0x1acc, 0x600); put(0x1ace, 0x8000);
    assert(issd_gameplay_player(ram, 0x600, true));
    fixture(); ram[0x3f91] |= 0x20; memcpy(original, ram, sizeof ram);
    assert(!issd_gameplay_player(ram, 0x600, true)); assert(!memcmp(original, ram, sizeof ram));
    fixture(); put(0x70, 12); memcpy(original, ram, sizeof ram);
    assert(!issd_gameplay_player(ram, 0x600, true)); assert(!issd_gameplay_goalkeeper(ram, 0x500, true));
    assert(!memcmp(original, ram, sizeof ram));
    fixture(); put(0x424, 12); memcpy(original, ram, sizeof ram);
    assert(!issd_gameplay_goalkeeper(ram, 0x500, true)); assert(!memcmp(original, ram, sizeof ram));
    fixture(); ram[0xda6] = 16; memcpy(original, ram, sizeof ram);
    assert(!issd_gameplay_player(ram, 0x600, true)); assert(!memcmp(original, ram, sizeof ram));
    for (unsigned controller = 0x1aa0; controller <= 0x1b60; controller += 0x30) {
        fixture(); put(controller+0x2c, 0x600); put(controller+0x2e, 0);
        memcpy(original, ram, sizeof ram);
        assert(!issd_gameplay_player(ram, 0x600, true)); assert(!memcmp(original, ram, sizeof ram));
        fixture(); put(controller+0x2c, 0x500); put(controller+0x2e, 0);
        memcpy(original, ram, sizeof ram);
        assert(!issd_gameplay_goalkeeper(ram, 0x500, true)); assert(!memcmp(original, ram, sizeof ram));
    }
    /* Width/depth extremes remain on-pitch and never exceed the speed cap. */
    fixture(); put(0x650, 0); put(0x652, 0); ram[0xd000] = 127; ram[0xd001] = 127;
    assert(issd_gameplay_player(ram, 0x600, true));
    assert(get(0x650) <= 18 && get(0x652) <= 18);
    fixture(); put(0x550, 16); put(0x552, 641); memcpy(original, ram, sizeof ram);
    assert(!issd_gameplay_goalkeeper(ram, 0x500, true)); assert(!memcmp(original, ram, sizeof ram));
    return 0;
}
