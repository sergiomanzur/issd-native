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
bool issd_save_to_slot(int s, const char *l) { (void)s; (void)l; return true; }
bool issd_load_from_slot(int s) { (void)s; return true; }
bool issd_save_get_info(int s, char *o, size_t n) { (void)s; snprintf(o, n, "Empty Slot"); return false; }
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

    return 0;
}
