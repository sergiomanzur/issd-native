#include "issd_match.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint8_t ram[0x20000];
static bool fail_capture, fail_restore;
static unsigned restores, fallback_calls;
static bool fallback(void) { fallback_calls++; return true; }
static unsigned capture_calls, fail_capture_at;
static void *favorite;
static size_t favorite_size;
static size_t capture(void *data, size_t capacity) {
    capture_calls++;
    if (fail_capture || capture_calls == fail_capture_at || capacity < sizeof ram) return 0;
    memcpy(data, ram, sizeof ram); return sizeof ram;
}
static bool restore(const void *data, size_t size) {
    if (fail_restore || size != sizeof ram) return false;
    memcpy(ram, data, size); restores++; return true;
}
bool issd_save_match_favorite(const void *data, size_t size, const char *label) {
    assert(label && size == sizeof ram);
    free(favorite); favorite = malloc(size); assert(favorite);
    memcpy(favorite, data, size); favorite_size = size; return true;
}
bool issd_save_read_match_favorite(void **out, size_t *size) {
    *out = NULL; *size = 0;
    if (!favorite) return false;
    *out = malloc(favorite_size); assert(*out);
    memcpy(*out, favorite, favorite_size); *size = favorite_size; return true;
}
const char *issd_save_error(void) { return "Favorite unavailable"; }
static void word(unsigned address, unsigned value) { ram[address] = value; ram[address+1] = value >> 8; }
static unsigned get(unsigned address) { return ram[address] | (unsigned)ram[address+1] << 8; }
static void setup(void) {
    memset(ram, 0, sizeof ram);
    word(0x32, 6); word(0x70, 15);
    word(0xda0, 60); word(0xea0, 8);
    word(0xda4, 8); word(0xea4, 0);
    word(0x1e4c, 2); word(0x86, 3); word(0x11e6, 4);
    word(0x1e5a, 2); word(0x1e54, 4);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    IssdConfig cfg, loaded;
    issd_config_init_defaults(&cfg);
    assert(cfg.match_preset == ISSD_MATCH_ORIGINAL);
    assert(cfg.match_custom.duration == 1 && cfg.match_custom.difficulty == 2);
    issd_match_init(ram, &cfg, capture, restore);
    setup();
    assert(!issd_match_rematch());
    assert(!issd_match_tick(false)); assert(!issd_match_has_setup());
    word(0x1648, 4); assert(!issd_match_tick(true)); assert(!issd_match_has_setup());
    word(0x1648, 0); word(0x72, 2);
    assert(!issd_match_tick(true)); assert(!issd_match_has_setup());
    word(0x72, 0);
    assert(issd_match_tick(true) == ISSD_MATCH_CAPTURE_SETUP);
    assert(get(0x1e5a) == 2 && get(0x1e54) == 4); /* Original untouched */
    assert(!issd_match_mark_drill(true));
    word(0x70, 8); word(0x16d0, 0x700);
    word(0x1648, 0x03a0); /* reused player scratch; live identity is DE07 */
    assert(issd_match_tick(true) == ISSD_MATCH_CAPTURE_KICKOFF);
    word(0x32, 5); word(0x1648, 4);
    assert(!issd_match_has_kickoff() && !issd_match_rematch());
    word(0x32, 6); word(0x1648, 0x03a0);
    issd_match_tick(false); assert(!issd_match_rematch());
    issd_match_tick(true);
    word(0xda2, 3); word(0x16d0, 0x234); word(0x1234, 77);
    assert(issd_match_mark_drill(true));
    word(0x1234, 99);
    fail_capture = true; assert(!issd_match_mark_drill(true)); fail_capture = false;
    assert(issd_match_restart_drill()); assert(get(0x1234) == 77 && get(0xda2) == 3);
    issd_match_tick(true);
    assert(issd_match_rematch()); assert(get(0xda2) == 0 && get(0x16d0) == 0x700);
    issd_match_tick(true);
    assert(get(0xda0) == 60 && get(0xea0) == 8 && get(0xda4) == 8);
    assert(get(0x1e4c) == 2 && get(0x86) == 3 && get(0x11e6) == 4);
    fail_restore = true; word(0x1234, 88);
    assert(!issd_match_rematch()); assert(get(0x1234) == 88);
    fail_restore = false;
    assert(issd_match_save_favorite());
    cfg.match_preset = ISSD_MATCH_CASUAL;
    fail_capture_at = capture_calls + 2; /* fail replacement after successful restore */
    assert(!issd_match_start_rules());
    assert(get(0x70) == 8 && get(0x1e5a) == 2 && issd_match_has_setup() && issd_match_has_kickoff());
    fail_capture_at = 0; issd_match_tick(true);
    assert(issd_match_start_rules());
    assert(get(0x70) == 15 && get(0x72) == 0);
    assert(get(0x1e5a) == 0 && get(0x1f88) == 0 && get(0x1e54) == 0);
    assert(get(0x1e4a) == 1 && get(0x1e50) == 1 && get(0x1e52) == 1 && get(0x1e64) == 1);
    assert(get(0x1e4c) == 2 && get(0x1e5c) == 0 && get(0x11e6) == 4);
    word(0x70, 8); assert(issd_match_tick(true) == ISSD_MATCH_CAPTURE_KICKOFF);
    assert(!issd_match_has_drill());
    word(0x1648, 0x20); word(0xde07, 0x20); unsigned before = restores;
    assert(!issd_match_rematch() && !issd_match_restart_drill() && !issd_match_play_favorite());
    assert(restores == before);
    issd_match_reset(); assert(!issd_match_has_setup() && !issd_match_has_kickoff());
    setup(); issd_match_tick(true); assert(issd_match_play_favorite());
    /* Favorite retains its recorded rules despite current Casual selection. */
    assert(get(0x1e5a) == 2 && get(0x1e54) == 4);
    assert(!issd_match_tick(true)); assert(get(0x1e5a) == 2);
    cfg.match_preset = ISSD_MATCH_CLASSIC; assert(issd_match_start_rules());
    assert(get(0x1e5a) == 1 && get(0x1e54) == 2 && get(0x1e4a) == 0 && get(0x1e64) == 1);
    cfg.match_preset = ISSD_MATCH_CUSTOM;
    cfg.match_custom = (IssdMatchRules){2, 4, 1, 0, 1, 0};
    issd_match_tick(true);
    assert(issd_match_start_rules());
    assert(get(0x1e5a) == 2 && get(0x1e54) == 4 && get(0x1e64) == 0);
    assert(issd_config_save(&cfg, argv[1])); assert(issd_config_load(&loaded, argv[1]));
    assert(loaded.match_preset == ISSD_MATCH_CUSTOM);
    assert(!memcmp(&loaded.match_custom, &cfg.match_custom, sizeof cfg.match_custom));
    FILE *f = fopen(argv[1], "w"); assert(f);
    fputs("match_preset=999\nmatch_duration=3\nmatch_difficulty=-100\nmatch_offside=-1\nmatch_fouls=100\nmatch_cards=oops\nmatch_extra_time=0junk\n", f); fclose(f);
    assert(issd_config_load(&loaded, argv[1]));
    assert(loaded.match_preset == ISSD_MATCH_CUSTOM && loaded.match_custom.duration == 2);
    assert(loaded.match_custom.difficulty == 0 && loaded.match_custom.offside == 0);
    assert(loaded.match_custom.fouls == 1 && loaded.match_custom.cards == 0 && loaded.match_custom.extra_time == 1);
    issd_match_reset();
    setup();
    word(0x70, 12); word(0x1538, 0x9d72); word(0x153a, 0xa4);
    issd_match_tick(false); assert(!issd_match_has_main_menu());
    issd_match_tick(true); assert(issd_match_has_main_menu());
    word(0x70, 15); word(0x1648, 0x20);
    issd_match_tick(true);
    word(0x70, 8); word(0xde07, 0x20); word(0x1234, 42);
    issd_match_tick(true); assert(issd_match_can_restart());
    word(0xda2, 5); word(0x1234, 99);
    assert(issd_match_restart()); assert(get(0xda2) == 0 && get(0x1234) == 42);
    assert(get(0xde07) == 0x20); /* Competition identity is retained. */
    assert(!issd_match_rematch()); /* Exhibition-only shortcuts remain restricted. */
    assert(!issd_match_back_main()); /* A host fallback can handle fresh paused loads. */
    issd_match_tick(true); assert(issd_match_back_main());
    assert(get(0x70) == 12 && get(0x1538) == 0x9d72);
    assert(!issd_match_can_restart()); /* Leaving the match invalidates its kickoff. */
    for (unsigned competition = 4; competition <= 0x20; competition += 0x1c) {
        issd_match_reset(); setup(); word(0x1648, competition);
        issd_match_tick(true); word(0x70, 8); word(0xde07, competition);
        word(0xdc00, 0x4567); issd_match_tick(true);
        word(0xdc00, 0x9999); word(0xda2, 3);
        assert(issd_match_restart());
        assert(get(0xdc00) == 0x4567 && get(0xda2) == 0);
        issd_match_tick(true); word(0x70, 15); word(0x1648, competition);
        issd_match_tick(true); assert(!issd_match_can_restart());
    }
    /* Prefer an existing healthy menu, then queue host navigation immediately
     * when a resident load has left the pause overlay unverified. */
    issd_match_set_main_menu_fallback(fallback);
    before = restores;
    assert(issd_match_back_main() && restores == before + 1 && fallback_calls == 0);
    assert(issd_match_back_main() && fallback_calls == 1);
    issd_match_set_main_menu_fallback(NULL);
    assert(!issd_match_back_main()); /* No host fallback keeps the health gate. */
    issd_match_reset_context(); assert(!issd_match_has_main_menu());
    assert(!issd_match_can_back_main());
    issd_match_set_main_menu_fallback(fallback);
    assert(issd_match_can_back_main());
    assert(issd_match_back_main() && fallback_calls == 2); /* Paused freshly loaded Continue. */
    issd_match_reset_context(); assert(issd_match_can_back_main());
    issd_match_set_main_menu_fallback(NULL); free(favorite);
    puts("Exhibition checkpoint and preset checks passed"); return 0;
}
