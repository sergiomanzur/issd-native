#include "issd_match.h"
#include "issd_save.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SNAPSHOT_LIMIT (4u * 1024u * 1024u)
typedef struct { void *data; size_t size; } Checkpoint;
static uint8_t *ram;
static const IssdConfig *config;
static size_t (*save_memory)(void *, size_t);
static bool (*load_memory)(const void *, size_t);
static bool (*main_menu_fallback)(void);
static Checkpoint setup, kickoff, drill, restart_point, main_menu;
static bool restart_waiting, restart_boundary;
static unsigned restart_flags;
static bool boundary_seen, waiting_kickoff;
static bool frame_healthy;
static char error[96];
static unsigned word(unsigned address) { return ram[address] | (unsigned)ram[address+1] << 8; }
static void put(unsigned address, unsigned value) { ram[address] = value; ram[address+1] = value >> 8; }
static bool fail(const char *message) { snprintf(error, sizeof error, "%s", message); return false; }
static void clear(Checkpoint *point) { free(point->data); memset(point, 0, sizeof *point); }
static bool allowed(void) {
    if (!ram || word(0x32) != 6) return false;
    /* $1648 is player scratch during play. CODE_85B947 backs the settings up
     * to $DDFF; $DE07 remains the match identity until original menu restore. */
    unsigned flags = word(0x32) == 6 && word(0x70) == 12 ? word(0x1648) : word(0xde07);
    return flags == 0;
}
static bool at_setup(void) {
    return allowed() && word(0x1648) == 0 && word(0x32) == 6 && word(0x70) == 15 && word(0x72) == 0;
}
bool issd_match_is_live(void) { return allowed() && word(0x32) == 6 && word(0x70) == 8; }
bool issd_match_frame_healthy(void) { return frame_healthy; }
static bool capture(Checkpoint *point) {
    void *data = malloc(SNAPSHOT_LIMIT);
    if (!data || !save_memory) { free(data); return fail("Match snapshot unavailable"); }
    size_t size = save_memory(data, SNAPSHOT_LIMIT);
    if (!size || size > SNAPSHOT_LIMIT) { free(data); return fail("Match snapshot capture failed"); }
    void *small = realloc(data, size);
    clear(point); point->data = small ? small : data; point->size = size;
    return true;
}
static bool admit_restore(const Checkpoint *point) {
    if (!point->data) return fail("No exhibition checkpoint captured yet");
    if (!allowed()) return fail("Match shortcuts require exhibition mode");
    if (!frame_healthy) return fail("Wait for a healthy exhibition frame");
    return true;
}
static bool restore_data(const Checkpoint *point) {
    if (!load_memory || !load_memory(point->data, point->size)) return fail("Match snapshot restore failed");
    frame_healthy = false;
    error[0] = 0; return true;
}
static bool restore(const Checkpoint *point) { return admit_restore(point) && restore_data(point); }
void issd_match_init(uint8_t *memory, const IssdConfig *cfg,
                     size_t (*save)(void *, size_t), bool (*load)(const void *, size_t)) {
    issd_match_reset_context(); ram = memory; config = cfg; save_memory = save; load_memory = load;
}
void issd_match_reset(void) {
    clear(&setup); clear(&kickoff); clear(&drill); clear(&restart_point);
    restart_waiting = restart_boundary = false;
    boundary_seen = waiting_kickoff = false; error[0] = 0;
    frame_healthy = false;
}
void issd_match_reset_context(void) { clear(&main_menu); issd_match_reset(); }
bool issd_match_has_setup(void) { return allowed() && setup.data != NULL; }
bool issd_match_has_kickoff(void) { return allowed() && kickoff.data != NULL; }
bool issd_match_has_drill(void) { return allowed() && drill.data != NULL; }
const char *issd_match_error(void) { return error; }
void issd_match_selected_rules(const IssdConfig *cfg, IssdMatchRules *rules) {
    *rules = cfg->match_custom;
    if (cfg->match_preset == ISSD_MATCH_CLASSIC) *rules = (IssdMatchRules){1,2,0,0,0,1};
    if (cfg->match_preset == ISSD_MATCH_CASUAL) *rules = (IssdMatchRules){0,0,1,1,1,1};
}
static void apply_rules(void) {
    if (!config || config->match_preset == ISSD_MATCH_ORIGINAL) return;
    IssdMatchRules rules; issd_match_selected_rules(config, &rules);
    const unsigned live[] = {0x1e5a,0x1e54,0x1e4a,0x1e50,0x1e52,0x1e64};
    const unsigned menu[] = {0x1f88,0x1f9c,0x1f92,0x1f98,0x1f9a,0x1fe0};
    const int values[] = {rules.duration,rules.difficulty,rules.offside,rules.fouls,rules.cards,rules.extra_time};
    const unsigned maxima[] = {2,4,1,1,1,1};
    for (unsigned i = 0; i < 6; ++i) {
        unsigned value = values[i] < 0 ? 0 : (unsigned)values[i];
        if (value > maxima[i]) value = maxima[i];
        put(live[i], value); put(menu[i], value);
    }
}
int issd_match_tick(bool healthy) {
    frame_healthy = healthy;
    if (!healthy || !ram) return 0;
    /* $A49D72 is the original eight-item main-menu input loop. Other
     * settings use the same global mode/submode and must never be captured. */
    if (!main_menu.data && issd_match_at_main_menu())
        capture(&main_menu);
    bool start = word(0x32) == 6 && word(0x70) == 15 && word(0x72) == 0;
    if (start && !restart_boundary) {
        clear(&restart_point);
        restart_flags = word(0x1648);
        /* Mask $24 identifies International Cup/World Series. Scenario and training
         * constructors have different result semantics and are excluded. */
        restart_waiting = restart_flags == 0 || (restart_flags & 0x24) == 4 || (restart_flags & 0x24) == 0x20;
    }
    restart_boundary = start;
    if (restart_waiting && word(0x32) == 6 && word(0x70) == 8 &&
        word(0xa8) == 0 && word(0xde07) == restart_flags) {
        if (capture(&restart_point)) restart_waiting = false;
    }
    bool boundary = at_setup();
    if (boundary && !boundary_seen) {
        boundary_seen = true;
        /* Stale checkpoints must never masquerade as the newly selected game. */
        clear(&setup); clear(&kickoff); clear(&drill); waiting_kickoff = false;
        apply_rules();
        if (capture(&setup)) { waiting_kickoff = true; return ISSD_MATCH_CAPTURE_SETUP; }
    }
    if (!boundary) boundary_seen = false;
    if (waiting_kickoff && issd_match_is_live() && word(0xa8) == 0) {
        if (capture(&kickoff)) { waiting_kickoff = false; return ISSD_MATCH_CAPTURE_KICKOFF; }
    }
    return 0;
}
bool issd_match_rematch(void) { return restore(&kickoff); }
bool issd_match_can_restart(void) {
    return ram && restart_point.data && word(0x32) == 6 && word(0x70) == 8 &&
           word(0xde07) == restart_flags;
}
bool issd_match_restart(void) {
    if (!issd_match_can_restart()) return fail("No kickoff captured for this match");
    if (!frame_healthy) return fail("Wait for a healthy match frame");
    return restore_data(&restart_point);
}
bool issd_match_has_main_menu(void) { return main_menu.data != NULL; }
bool issd_match_at_main_menu(void) {
    return ram && word(0x32) == 6 && word(0x70) == 12 &&
           word(0x1538) == 0x9d72 && word(0x153a) == 0xa4;
}
bool issd_match_can_back_main(void) { return main_menu.data != NULL || main_menu_fallback != NULL; }
void issd_match_set_main_menu_fallback(bool (*request)(void)) { main_menu_fallback = request; }
bool issd_match_back_main(void) {
    /* A loaded Continue stays paused before its first verified frame. Returning
     * replaces that resident state with a separately healthy captured menu or
     * boots the original menu through the host; it never captures current RAM. */
    if (main_menu_fallback && (!main_menu.data || !frame_healthy)) {
        if (!main_menu_fallback()) return fail("Main menu return could not start");
        error[0] = 0; return true;
    }
    if (!main_menu.data) return fail("Visit the game main menu first");
    if (!frame_healthy) return fail("Wait for a healthy frame");
    if (!restore_data(&main_menu)) return false;
    /* Host restore resets campaign observation, not the saved checkpoint.
     * Reset resident shortcuts so a departed match cannot be replayed. */
    issd_match_reset();
    return true;
}
bool issd_match_mark_drill(bool healthy) {
    if (!healthy || !issd_match_is_live()) return fail("Mark a drill during a live exhibition");
    if (!capture(&drill)) return false;
    error[0] = 0; return true;
}
bool issd_match_restart_drill(void) { return restore(&drill); }
bool issd_match_start_rules(void) {
    if (!admit_restore(&setup)) return false;
    Checkpoint previous = {0}, replacement = {0};
    if (!capture(&previous)) return false;
    if (!restore_data(&setup)) { clear(&previous); return false; }
    if (!at_setup()) {
        load_memory(previous.data, previous.size); clear(&previous);
        return fail("Exhibition setup boundary unavailable");
    }
    apply_rules();
    if (!capture(&replacement)) {
        bool reverted = load_memory(previous.data, previous.size); clear(&previous);
        return fail(reverted ? "Match setup capture failed; previous state restored" : "Match setup rollback failed");
    }
    clear(&previous); clear(&setup); setup = replacement;
    clear(&kickoff); clear(&drill);
    boundary_seen = waiting_kickoff = true;
    error[0] = 0;
    return true;
}
bool issd_match_save_favorite(void) {
    if (!issd_match_has_setup()) return fail("Start an exhibition before saving a favorite");
    if (!frame_healthy) return fail("Wait for a healthy exhibition frame");
    if (!issd_save_match_favorite(setup.data, setup.size, "Favorite exhibition setup"))
        return fail(issd_save_error());
    error[0] = 0; return true;
}
bool issd_match_play_favorite(void) {
    /* Boot/title has no resident match; a persisted favorite may start there.
     * Retained rematch/drill checkpoints never receive this exception. */
    if (!allowed() && !(ram && word(0x32) <= 1 && word(0xde07) == 0))
        return fail("Play favorites from exhibition or title");
    if (!frame_healthy) return fail("Wait for a healthy frame before playing favorite");
    Checkpoint point = {0}, previous = {0};
    if (!issd_save_read_match_favorite(&point.data, &point.size)) return fail(issd_save_error());
    /* A valid runtime payload is not necessarily an exhibition setup. Roll
     * back a semantically incorrect favorite rather than accepting a campaign. */
    if (!capture(&previous)) { clear(&point); return false; }
    if (!restore_data(&point)) { clear(&point); clear(&previous); return false; }
    if (!at_setup()) {
        bool reverted = load_memory(previous.data, previous.size);
        clear(&point); clear(&previous);
        return fail(reverted ? "Favorite is not an exhibition setup" : "Favorite rollback failed");
    }
    clear(&previous); clear(&setup); clear(&kickoff); clear(&drill);
    setup = point; boundary_seen = waiting_kickoff = true;
    error[0] = 0; return true;
}
