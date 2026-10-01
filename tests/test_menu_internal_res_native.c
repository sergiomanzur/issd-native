/* Internal resolution only changes anything under the CRT filter: nearest and
 * linear present the logical buffer and let SDL scale it once. The menu row
 * must therefore be inert unless CRT is selected, rather than cycling six
 * labels that do nothing. */
#include "issd_menu.h"
#include "issd_config.h"
#include "issd_mod.h"
#include "issd_save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* issd_menu.c reaches for saves and for a restart on confirm; navigation
 * never does, so these keep the link closed without touching anything.
 * The mod and tile registries are linked for real: stubbing them drifted
 * out of date every time the menu grew a row. */
uint8_t g_ram[0x20000];
static bool campaign_available, continue_ok, legacy;
static int continue_calls, confirmed_loads;
static int prepared_contexts;
static int slot_queries;
static void prepare_context(void) { prepared_contexts++; }
bool issd_save_to_slot(int s, const char *l) { (void)s; (void)l; return true; }
bool issd_load_from_slot(int s) { (void)s; return true; }
bool issd_save_get_info(int s, char *o, size_t n) { (void)s; slot_queries++; snprintf(o, n, "Empty Slot"); return false; }
bool issd_save_continue_info(char *o, size_t n) { snprintf(o, n, "Cup checkpoint"); return campaign_available; }
bool issd_save_continue(void) { continue_calls++; return continue_ok; }
const char *issd_save_error(void) { return "Invalid autosave"; }
bool issd_save_is_legacy(int s) { (void)s; return legacy; }
bool issd_load_from_slot_confirmed(int s, bool allow) { (void)s; if (legacy && !allow) return false; confirmed_loads++; return true; }
void issd_touch_set_pad_visible(bool visible) { (void)visible; }
void issd_request_quit(void) { }
void issd_restart_application(void) { }

#define ROW_INTERNAL_RES 5

static void select_internal_res_row(void) {
    issd_menu_open();
    g_overlay_menu.current_item = ROW_INTERNAL_RES;
}

int main(void) {
    issd_config_init_defaults(&g_issd_config);
    issd_menu_init();

    /* Nearest: the row must not move, in either direction. */
    g_issd_config.scaling_filter = ISSD_FILTER_NEAREST;
    g_issd_config.internal_res = ISSD_RES_1X;
    select_internal_res_row();
    issd_menu_navigate_right();
    assert(g_issd_config.internal_res == ISSD_RES_1X);
    issd_menu_navigate_left();
    assert(g_issd_config.internal_res == ISSD_RES_1X);

    /* Linear is the same story. */
    g_issd_config.scaling_filter = ISSD_FILTER_LINEAR;
    issd_menu_navigate_right();
    assert(g_issd_config.internal_res == ISSD_RES_1X);

    /* CRT genuinely renders into the scaled buffer, so it still cycles. */
    g_issd_config.scaling_filter = ISSD_FILTER_CRT;
    issd_menu_navigate_right();
    assert(g_issd_config.internal_res == ISSD_RES_2X);
    issd_menu_navigate_left();
    assert(g_issd_config.internal_res == ISSD_RES_1X);

    /* Neighbouring rows must keep working while the row above is inert. */
    g_issd_config.scaling_filter = ISSD_FILTER_NEAREST;
    g_overlay_menu.current_item = 6; /* Filter */
    issd_menu_navigate_right();
    assert(g_issd_config.scaling_filter != ISSD_FILTER_NEAREST);

    /* Startup never loads automatically; an unavailable campaign stays closed. */
    issd_menu_close();
    campaign_available = false;
    issd_menu_offer_continue();
    assert(!issd_menu_is_open());
    campaign_available = true;
    issd_menu_offer_continue();
    issd_menu_set_save_context_callback(prepare_context);
    assert(issd_menu_is_open());
    assert(continue_calls == 0);
    continue_ok = false;
    issd_menu_confirm();
    assert(issd_menu_is_open());
    assert(continue_calls == 1);
    assert(prepared_contexts == 1);
    assert(strstr(g_overlay_menu.status_message, "Invalid autosave"));
    continue_ok = true;
    issd_menu_confirm();
    assert(!issd_menu_is_open());
    assert(continue_calls == 2);

    /* A legacy load needs two deliberate confirms; navigation cancels it. */
    issd_menu_open();
    legacy = true;
    g_overlay_menu.current_item = 10;
    issd_menu_confirm();
    assert(confirmed_loads == 0);
    assert(strstr(g_overlay_menu.status_message, "Legacy"));
    issd_menu_navigate_down();
    issd_menu_navigate_up();
    issd_menu_confirm();
    assert(confirmed_loads == 0);
    issd_menu_confirm();
    assert(confirmed_loads == 1);
    assert(!issd_menu_is_open());

    /* The password page keeps letter keys separate from main-menu rows. */
    issd_menu_open();
    g_overlay_menu.current_item = 16;
    issd_menu_confirm();
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_PASSWORD);
    issd_menu_navigate_right();
    issd_menu_confirm();
    issd_menu_cancel();
    assert(issd_menu_is_open());
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_MAIN);
    assert(g_overlay_menu.current_item == 16);

    /* Paused rendering must not rehash a full manual snapshot every frame.
     * Changing the slot or context refreshes eligibility; loading revalidates. */
    static uint32_t framebuffer[256 * 224];
    g_overlay_menu.current_item = 10;
    issd_menu_render(framebuffer, 256, 224);
    issd_menu_render(framebuffer, 256, 224);
    assert(slot_queries == 1);
    g_overlay_menu.current_slot = 1;
    issd_menu_render(framebuffer, 256, 224);
    assert(slot_queries == 2);
    issd_menu_refresh_continue();
    issd_menu_render(framebuffer, 256, 224);
    assert(slot_queries == 3);

    /* Independent gameplay toggles stack and return to their own main row. */
    g_overlay_menu.current_item = 17;
    issd_menu_confirm();
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_GAMEPLAY);
    assert(g_overlay_menu.current_item == 0);
    issd_menu_confirm();
    assert(g_issd_config.gameplay_goalkeeper_ai);
    issd_menu_navigate_down();
    issd_menu_navigate_right();
    assert(g_issd_config.gameplay_player_ai && g_issd_config.gameplay_goalkeeper_ai);
    issd_menu_navigate_left();
    assert(!g_issd_config.gameplay_player_ai && g_issd_config.gameplay_goalkeeper_ai);
    /* Mouse/touch shares the same toggle and Back behavior as controllers. */
    issd_menu_handle_click(30, 56, 256, 224);
    assert(g_issd_config.gameplay_player_ai);
    for (unsigned i = 0; i < sizeof framebuffer / sizeof framebuffer[0]; i++) framebuffer[i] = 0xffffffff;
    issd_menu_render(framebuffer, 256, 224);
    assert(framebuffer[102 * 256 + 20] == 0xff002244); /* Old page text must be erased. */
    issd_menu_cancel();
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_MAIN && g_overlay_menu.current_item == 17);
    return 0;
}
