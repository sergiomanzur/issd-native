/* Internal resolution only changes anything under the CRT filter: nearest and
 * linear present the logical buffer and let SDL scale it once. The menu row
 * must therefore be inert unless CRT is selected, rather than cycling six
 * labels that do nothing. */
#include "issd_menu.h"
#include "issd_config.h"
#include "issd_mod.h"
#include "issd_save.h"
#include "issd_match.h"
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
static int input_resets;
static void reset_input(void) { input_resets++; }
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
void issd_touch_set_layout(const int *x,const int *y,const int *s) { (void)x;(void)y;(void)s; }
int issd_touch_rects(const void **out) { (void)out; return 0; }
bool issd_touch_center(int c,int *x,int *y) { (void)c;(void)x;(void)y;return false; }
void issd_request_quit(void) { }
void issd_restart_application(void) { }
bool issd_save_match_favorite(const void *p,size_t n,const char *l) { (void)p;(void)n;(void)l; return false; }
bool issd_save_read_match_favorite(void **p,size_t *n) { *p=NULL; *n=0; return false; }
bool issd_save_match_favorite_info(char *o,size_t n) { snprintf(o,n,"No favorite"); return false; }

#define ROW_INTERNAL_RES 5

static void select_internal_res_row(void) {
    issd_menu_open();
    g_overlay_menu.current_item = ROW_INTERNAL_RES;
}

int main(void) {
    issd_config_init_defaults(&g_issd_config);
    issd_match_init(g_ram, &g_issd_config, NULL, NULL);
    issd_menu_init();
    issd_menu_set_input_reset_callback(reset_input);

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

    /* Sharp uses the integer intermediate before final display scaling. */
    g_issd_config.scaling_filter = ISSD_FILTER_SHARP;
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
    g_overlay_menu.current_item = 3;
    assert(!g_issd_config.gameplay_bug_fixes);
    issd_menu_confirm(); assert(g_issd_config.gameplay_bug_fixes);
    issd_menu_navigate_left(); assert(!g_issd_config.gameplay_bug_fixes);
    issd_menu_handle_click(30, 105, 256, 224); assert(g_issd_config.gameplay_bug_fixes);
    issd_menu_navigate_down(); assert(g_overlay_menu.current_item == 4);
    issd_menu_navigate_down(); assert(g_overlay_menu.current_item == 0);
    issd_menu_navigate_up(); assert(g_overlay_menu.current_item == 4);
    issd_menu_confirm();
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_MAIN && g_overlay_menu.current_item == 17);
    issd_menu_confirm();
    issd_menu_cancel();
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_MAIN && g_overlay_menu.current_item == 17);
    issd_menu_confirm();
    g_overlay_menu.current_item = 2; issd_menu_confirm();
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_MATCH);
    g_overlay_menu.current_item = 0; issd_menu_confirm();
    assert(issd_menu_is_open()); /* no invented kickoff checkpoint */
    assert(strstr(g_overlay_menu.status_message, "checkpoint"));
    issd_menu_render(framebuffer, 256, 224);
    bool visible_error = false;
    for (int y = 192; y < 200; ++y) for (int x = 16; x < 240; ++x)
        visible_error |= framebuffer[y * 256 + x] == 0xffffaa00;
    assert(visible_error); /* failures must be readable while this page is paused */
    g_overlay_menu.current_item = 5;
    issd_menu_navigate_right(); assert(g_issd_config.match_preset == ISSD_MATCH_CLASSIC);
    issd_menu_navigate_right(); assert(g_issd_config.match_preset == ISSD_MATCH_CASUAL);
    g_overlay_menu.current_item = 7;
    issd_menu_navigate_right();
    assert(g_issd_config.match_preset == ISSD_MATCH_CUSTOM && g_issd_config.match_custom.duration == 1);
    g_overlay_menu.current_item = 12; issd_menu_navigate_down();
    assert(g_overlay_menu.current_item == 13 && g_overlay_menu.scroll > 0);
    issd_menu_render(framebuffer, 256, 224);
    issd_menu_cancel(); assert(g_overlay_menu.page == ISSD_MENU_PAGE_GAMEPLAY);
    issd_menu_cancel(); assert(g_overlay_menu.page == ISSD_MENU_PAGE_MAIN);
    g_overlay_menu.current_item = 1;
    issd_menu_confirm();
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_CONTROLS);
    g_overlay_menu.current_item = 2;
    int deadzone = g_issd_config.player_profiles[0].stick_deadzone;
    issd_menu_navigate_right();
    assert(g_issd_config.player_profiles[0].stick_deadzone > deadzone);
    assert(g_issd_config.player_profiles[1].stick_deadzone == 12000);
    g_overlay_menu.current_item = 4;
    issd_menu_confirm();
    assert(issd_menu_binding_capture());
    issd_menu_open(); /* lifecycle pause cancels an armed binding capture */
    assert(!issd_menu_binding_capture());
    issd_menu_confirm();
    assert(issd_menu_binding_capture());
    issd_menu_capture_pad(0, UINT64_C(1)); /* held confirm is ignored */
    issd_menu_capture_pad(0, 0);
    issd_menu_capture_pad(1, UINT64_C(2)); /* another player is ignored */
    assert(issd_menu_binding_capture());
    issd_menu_capture_pad(0, UINT64_C(2));
    assert(!issd_menu_binding_capture());
    assert(g_issd_config.player_profiles[0].bindings[0] == UINT64_C(2));
    g_overlay_menu.current_item = 16;
    issd_menu_confirm();
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_KEYBOARD);
    g_overlay_menu.current_item = 0;
    issd_menu_confirm();
    assert(issd_menu_capture_key(5));
    assert(g_issd_config.key_p1_b == 5);
    issd_menu_cancel();
    g_overlay_menu.current_item = 17;
    issd_menu_confirm();
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_TOUCH);
    g_overlay_menu.current_item = 3;
    issd_menu_navigate_right();
    assert(g_issd_config.touch_size[0] == 105);
    issd_menu_cancel();
    issd_menu_cancel();
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_MAIN && g_overlay_menu.current_item == 1);
    int before_close = input_resets;
    issd_menu_close();
    assert(input_resets == before_close + 1);
    issd_menu_open();
    before_close = input_resets;
    issd_menu_toggle();
    assert(input_resets == before_close + 1);
    return 0;
}
