/* Mods stack: several packs on at once, in an order the player controls.
 *
 * The things that must hold are all about not lying to the player. Nothing is
 * enabled unless it was asked for. The order shown is the order applied. Two
 * packs that touch the same team say so. And a pack that failed is still
 * reported as failed after anything else is toggled.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "issd_menu.h"
#include "issd_config.h"
#include "issd_mod.h"
#include "issd_hd.h"

/* issd_menu.c reaches for saves and for a restart on confirm; neither belongs
 * in a test, and navigation never touches them. */
uint8_t g_ram[0x20000];
bool issd_save_to_slot(int s, const char *l) { (void)s; (void)l; return true; }
bool issd_load_from_slot(int s) { (void)s; return true; }
bool issd_save_get_info(int s, char *o, size_t n) { (void)s; snprintf(o, n, "Empty"); return false; }
static int g_restarts;
void issd_restart_application(void) { g_restarts++; }

#define FORMATION_PTRS 0x05EF48u
static uint8_t rom[3 * 1024 * 1024];

static int find_pack(const char *name) {
    for (int i = 0; i < issd_mod_get_pack_count(); i++) {
        const IssdModPack *p = issd_mod_get_pack(i);
        if (p && strcmp(p->name, name) == 0) return i;
    }
    return -1;
}

int main(void) {
    issd_config_init_defaults(&g_issd_config);
    issd_menu_init();
    assert(issd_mod_init());

    const int packs = issd_mod_scan_and_load("tests/fixtures/mods");
    if (packs < 2) {
        puts("mod stack tests skipped: need two fixture packs");
        return 0;
    }
    const int base = find_pack("Fixture Pack");
    const int overlay = find_pack("Fixture Overlay");
    assert(base >= 0 && overlay >= 0);

    /* --- nothing runs unless it was asked for ------------------------- */
    for (int i = 0; i < packs; i++)
        assert(!issd_mod_is_pack_enabled(i) &&
               "a freshly scanned pack must be off");
    assert(issd_mod_enabled_count() == 0);

    /* --- the list is the stack, in order ------------------------------ */
    issd_mod_set_pack_enabled(base, true);
    issd_mod_set_pack_enabled(overlay, true);
    assert(issd_mod_enabled_count() == 2);

    char list[512];
    issd_mod_enabled_list(list, sizeof list);
    assert(strcmp(list, "Fixture Pack|Fixture Overlay") == 0);

    /* Turning one off and on again moves it to the top, which is how a
     * modder says "let this one win". */
    issd_mod_set_pack_enabled(base, false);
    issd_mod_set_pack_enabled(base, true);
    issd_mod_enabled_list(list, sizeof list);
    assert(strcmp(list, "Fixture Overlay|Fixture Pack") == 0);

    /* And the list rebuilds the same stack, which is what survives a
     * restart - the only moment a roster mod takes effect. */
    issd_mod_enable_from_list("Fixture Pack|Fixture Overlay");
    issd_mod_enabled_list(list, sizeof list);
    assert(strcmp(list, "Fixture Pack|Fixture Overlay") == 0);
    assert(issd_mod_is_pack_enabled(base) && issd_mod_is_pack_enabled(overlay));

    /* A name that is no longer installed is simply not enabled. */
    issd_mod_enable_from_list("Fixture Pack|Something Uninstalled");
    assert(issd_mod_enabled_count() == 1);
    assert(issd_mod_is_pack_enabled(base) && !issd_mod_is_pack_enabled(overlay));

    /* --- applying the stack ------------------------------------------- */
    memset(rom, 0xEE, sizeof rom);
    rom[FORMATION_PTRS + 0] = 0x00;
    rom[FORMATION_PTRS + 1] = 0x90;          /* team 0's record -> $8B:9000 */
    issd_mod_rom_set_image(rom, sizeof rom);

    issd_mod_enable_from_list("Fixture Pack|Fixture Overlay");
    issd_mod_reapply();
    const IssdModResult *r = issd_mod_last_result();
    assert(r->packs_applied == 2);
    assert(r->players_patched > 0);
    /* Two things are wrong here and both must be counted: the fixture names a
     * team that does not exist, and both packs name team 0. `detail` holds
     * the first of them, so the menu has something specific to show. */
    assert(r->warnings >= 2);
    assert(r->detail[0] && "a warning must say what it was about");

    /* Later wins: the overlay's shape is the one in the cartridge. */
    const size_t rec = (size_t)0x0B * 0x8000u + (0x9000u - 0x8000u);
    assert(rom[rec] == 12 && "5-3-2 is label 12");

    /* Reversed, the other pack wins. Fixture Pack asks for 4-2-3-1, which
     * the screen can only print as 4-5-1. */
    issd_mod_enable_from_list("Fixture Overlay|Fixture Pack");
    issd_mod_reapply();
    assert(rom[rec] == 0 && "4-5-1 is label 0");

    /* --- a broken pack stays broken ----------------------------------- */
    issd_mod_result_note_error("Broken Pack has no teams");
    assert(issd_mod_last_result()->errors == 1);
    issd_mod_reapply();
    assert(issd_mod_last_result()->errors == 1 &&
           "re-applying must not make a broken pack look fine");
    char summary[96];
    issd_mod_result_summary(summary, sizeof summary);
    assert(strstr(summary, "FAILED") != NULL);

    /* --- the menu page ------------------------------------------------ */
    issd_mod_init();
    issd_mod_scan_and_load("tests/fixtures/mods");
    issd_menu_init();
    issd_menu_open();
    g_overlay_menu.current_item = 2;          /* the Mods row */
    assert(issd_menu_confirm());
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_MODS);

    /* The cursor lands on a pack, never on a heading. */
    assert(g_overlay_menu.current_item == 1);
    const int rows = 2 + issd_mod_get_pack_count() + issd_hd_available_count() + 2;
    int seen_headers = 0;
    for (int i = 0; i < rows * 2; i++) {
        issd_menu_navigate_down();
        const int row = g_overlay_menu.current_item;
        if (row == 0 || row == 1 + issd_mod_get_pack_count()) seen_headers++;
    }
    assert(seen_headers == 0 && "the cursor must skip headings");

    /* Toggling a row records the stack in the config, because that is what a
     * restart reads back. */
    g_overlay_menu.current_item = 1;
    g_issd_config.active_mod_packs[0] = '\0';
    assert(issd_menu_confirm());
    assert(g_issd_config.active_mod_packs[0] != '\0');
    assert(issd_mod_is_pack_enabled(0));
    assert(issd_menu_confirm());
    assert(!issd_mod_is_pack_enabled(0));
    assert(g_issd_config.active_mod_packs[0] == '\0');

    /* Left and right toggle too, so the row behaves like every other one. */
    issd_menu_navigate_right();
    assert(issd_mod_is_pack_enabled(0));
    issd_menu_navigate_left();
    assert(!issd_mod_is_pack_enabled(0));

    /* Back returns to the main menu without closing it. */
    g_overlay_menu.current_item = rows - 1;
    assert(issd_menu_confirm());
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_MAIN);
    assert(issd_menu_is_open());

    /* Apply restarts, which is the only way a roster mod reaches the game. */
    g_overlay_menu.current_item = 2;
    issd_menu_confirm();
    g_overlay_menu.current_item = rows - 2;
    assert(issd_menu_confirm());
    assert(g_restarts == 1);

    /* --- the notification --------------------------------------------- */
    static uint32_t fb[256 * 224];
    memset(fb, 0, sizeof fb);
    issd_menu_render_notification(fb, 256, 224);
    bool drawn = false;
    for (size_t i = 0; i < sizeof fb / sizeof fb[0]; i++) if (fb[i]) drawn = true;
    assert(!drawn && "nothing to say means nothing on screen");

    issd_menu_notify("Mods applied: 2 packs", 30);
    issd_menu_render_notification(fb, 256, 224);
    for (size_t i = 0; i < sizeof fb / sizeof fb[0]; i++) if (fb[i]) drawn = true;
    assert(drawn && "a notification must actually appear");

    /* It goes away on its own rather than sitting over the game forever. */
    for (int i = 0; i < 40; i++) issd_menu_render_notification(fb, 256, 224);
    memset(fb, 0, sizeof fb);
    issd_menu_render_notification(fb, 256, 224);
    drawn = false;
    for (size_t i = 0; i < sizeof fb / sizeof fb[0]; i++) if (fb[i]) drawn = true;
    assert(!drawn && "a notification must expire");

    puts("mod stack tests passed");
    return 0;
}
