#include "issd_menu.h"
#include "issd_config.h"
#include "issd_save.h"
#include "issd_mod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "issd_hd.h"
#include "issd_android.h"

extern uint8_t g_ram[0x20000];

static const char *get_mods_dir(void) {
#ifdef ISSD_ANDROID
    return issd_android_mods_dir();
#else
    return "mods";
#endif
}

IssdOverlayMenu g_overlay_menu;

/* 8x8 Basic ASCII font (32-127) bitmap table */
const uint8_t g_issd_font8x8[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 32 ' ' */
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, /* 33 '!' */
    {0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}, /* 34 '"' */
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, /* 35 '#' */
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, /* 36 '$' */
    {0x00,0x66,0xA6,0xD8,0x1B,0x65,0x66,0x00}, /* 37 '%' */
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, /* 38 '&' */
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, /* 39 ''' */
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, /* 40 '(' */
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, /* 41 ')' */
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, /* 42 '*' */
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, /* 43 '+' */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, /* 44 ',' */
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, /* 45 '-' */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, /* 46 '.' */
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, /* 47 '/' */
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, /* 48 '0' */
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, /* 49 '1' */
    {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00}, /* 50 '2' */
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, /* 51 '3' */
    {0x06,0x0E,0x1E,0x36,0x66,0x7F,0x06,0x00}, /* 52 '4' */
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, /* 53 '5' */
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, /* 54 '6' */
    {0x7E,0x66,0x06,0x0C,0x18,0x18,0x18,0x00}, /* 55 '7' */
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, /* 56 '8' */
    {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, /* 57 '9' */
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, /* 58 ':' */
    {0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}, /* 59 ';' */
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, /* 60 '<' */
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, /* 61 '=' */
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, /* 62 '>' */
    {0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00}, /* 63 '?' */
    {0x3C,0x66,0x6E,0x6E,0x60,0x62,0x3C,0x00}, /* 64 '@' */
    {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00}, /* 65 'A' */
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, /* 66 'B' */
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, /* 67 'C' */
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, /* 68 'D' */
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00}, /* 69 'E' */
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}, /* 70 'F' */
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, /* 71 'G' */
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, /* 72 'H' */
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* 73 'I' */
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00}, /* 74 'J' */
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, /* 75 'K' */
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, /* 76 'L' */
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, /* 77 'M' */
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, /* 78 'N' */
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, /* 79 'O' */
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, /* 80 'P' */
    {0x3C,0x66,0x66,0x66,0x6E,0x3C,0x0E,0x00}, /* 81 'Q' */
    {0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00}, /* 82 'R' */
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, /* 83 'S' */
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, /* 84 'T' */
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, /* 85 'U' */
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, /* 86 'V' */
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, /* 87 'W' */
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, /* 88 'X' */
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, /* 89 'Y' */
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, /* 90 'Z' */
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, /* 91 '[' */
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, /* 92 '\' */
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, /* 93 ']' */
    {0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00}, /* 94 '^' */
    {0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00}, /* 95 '_' */
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, /* 96 '`' */
    {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3B,0x00}, /* 97 'a' */
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, /* 98 'b' */
    {0x00,0x00,0x3C,0x66,0x60,0x66,0x3C,0x00}, /* 99 'c' */
    {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, /* 100 'd' */
    {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, /* 101 'e' */
    {0x1C,0x30,0x7C,0x30,0x30,0x30,0x30,0x00}, /* 102 'f' */
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x3C}, /* 103 'g' */
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, /* 104 'h' */
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, /* 105 'i' */
    {0x0C,0x00,0x1C,0x0C,0x0C,0x0C,0x6C,0x38}, /* 106 'j' */
    {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}, /* 107 'k' */
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* 108 'l' */
    {0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00}, /* 109 'm' */
    {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, /* 110 'n' */
    {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, /* 111 'o' */
    {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, /* 112 'p' */
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x07}, /* 113 'q' */
    {0x00,0x00,0x5C,0x66,0x60,0x60,0x60,0x00}, /* 114 'r' */
    {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, /* 115 's' */
    {0x18,0x18,0x7E,0x18,0x18,0x18,0x0C,0x00}, /* 116 't' */
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, /* 117 'u' */
    {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, /* 118 'v' */
    {0x00,0x00,0x63,0x6B,0x7F,0x3E,0x36,0x00}, /* 119 'w' */
    {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, /* 120 'x' */
    {0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x3C}, /* 121 'y' */
    {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, /* 122 'z' */
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, /* 123 '{' */
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, /* 124 '|' */
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, /* 125 '}' */
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}, /* 126 '~' */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}  /* 127 */
};

#ifdef ISSD_ANDROID
#define MENU_ITEM_PICK_ROM 15
#define MENU_ITEM_PICK_MODS_FOLDER 16
#define MENU_ITEM_RESTART 17
#define MENU_ITEM_QUIT 18
#define MENU_TOTAL_ITEMS 19
#else
#define MENU_ITEM_RESTART 15
#define MENU_ITEM_QUIT 16
#define MENU_TOTAL_ITEMS 17
#endif

/* The mod list is one row per pack, roster packs first, then tile packs,
 * then the actions. Everything is addressed by row index so the same
 * navigation code serves both kinds. */
static int mods_roster_count(void) { return issd_mod_get_pack_count(); }
static int mods_tile_count(void)   { return issd_hd_available_count(); }
static int mods_row_count(void) {
    /* two headers, the packs, then Apply and Back */
    return 2 + mods_roster_count() + mods_tile_count() + 2;
}

/* Rows that are only labels: the cursor skips over them. */
static bool mods_row_is_header(int row) {
    return row == 0 || row == 1 + mods_roster_count();
}

static int mods_row_apply(void)  { return mods_row_count() - 2; }
static int mods_row_back(void)   { return mods_row_count() - 1; }

/* Remember the stack in the config. It is what a restart reads, and a
 * restart is the only moment a roster mod can take effect. */
static void mods_store_selection(void) {
    issd_mod_enabled_list(g_issd_config.active_mod_packs,
                          sizeof(g_issd_config.active_mod_packs));
    issd_hd_enabled_list(g_issd_config.hd_texture_packs,
                         sizeof(g_issd_config.hd_texture_packs));
}

static void mods_toggle_row(int row) {
    const int rosters = mods_roster_count();
    if (row >= 1 && row < 1 + rosters) {
        const int i = row - 1;
        issd_mod_set_pack_enabled(i, !issd_mod_is_pack_enabled(i));
    } else {
        const int i = row - (2 + rosters);
        if (i < 0 || i >= mods_tile_count()) return;
        issd_hd_set_enabled(i, !issd_hd_is_enabled(i));
        /* Tiles are only images, so they can be swapped without a restart
         * and the change is visible the moment the menu closes. */
        issd_hd_apply(get_mods_dir());
        issd_mod_result_note_tiles(issd_hd_texture_count());
    }
    mods_store_selection();
}

static void mods_open(void) {
    issd_hd_scan_packs(get_mods_dir());   /* pick up anything dropped in since boot */
    issd_hd_enable_from_list(g_issd_config.hd_texture_packs);
    g_overlay_menu.page = ISSD_MENU_PAGE_MODS;
    g_overlay_menu.current_item = 1;
    g_overlay_menu.scroll = 0;
}

/* Replacements are drawn into the enlarged frame, so at 1x there is nothing
 * to put them in. The row says so rather than appearing to do nothing. */
static const char *issd_menu_hd_pack_label(void) {
    if (!issd_hd_active()) return "OFF";
    if (g_issd_config.internal_res == ISSD_RES_1X) return "NEEDS 2X+";
    return issd_hd_pack_name();
}

/* What the main menu's Mods row says: enough to know whether anything is
 * on without opening the page. */
static const char *issd_menu_mods_label(void) {
    static char label[40];
    const int rosters = issd_mod_enabled_count();
    const int tiles = issd_hd_enabled_count();
    if (!rosters && !tiles) return "none";
    if (rosters && tiles) snprintf(label, sizeof label, "%d + %d tile", rosters, tiles);
    else if (rosters)     snprintf(label, sizeof label, "%d pack%s", rosters, rosters == 1 ? "" : "s");
    else                  snprintf(label, sizeof label, "%d tile pack%s", tiles, tiles == 1 ? "" : "s");
    return label;
}

static void DrawChar(uint32_t *fb, int fb_w, int fb_h, int x, int y, char c, uint32_t color) {
    if (c < 32 || c > 126) c = ' ';
    const uint8_t *glyph = g_issd_font8x8[c - 32];
    for (int row = 0; row < 8; row++) {
        int py = y + row;
        if (py < 0 || py >= fb_h) continue;
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            int px = x + col;
            if (px < 0 || px >= fb_w) continue;
            if (bits & (0x80 >> col)) {
                fb[py * fb_w + px] = color;
            }
        }
    }
}

static void DrawString(uint32_t *fb, int fb_w, int fb_h, int x, int y, const char *str, uint32_t color) {
    if (!str) return;
    int cur_x = x;
    while (*str) {
        DrawChar(fb, fb_w, fb_h, cur_x, y, *str, color);
        cur_x += 8;
        str++;
    }
}

static void DrawBox(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t color) {
    for (int py = y; py < y + h; py++) {
        if (py < 0 || py >= fb_h) continue;
        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= fb_w) continue;
            if (py == y || py == y + h - 1 || px == x || px == x + w - 1) {
                fb[py * fb_w + px] = color;
            }
        }
    }
}

void issd_menu_init(void) {
    memset(&g_overlay_menu, 0, sizeof(g_overlay_menu));
    g_overlay_menu.is_open = false;
    g_overlay_menu.current_item = 0;
    g_overlay_menu.current_slot = 0;
    g_overlay_menu.control_schema = ISSD_SCHEMA_CLASSIC;
    g_overlay_menu.status_timer = 0;
    g_overlay_menu.page = ISSD_MENU_PAGE_MAIN;
    g_overlay_menu.scroll = 0;
}

void issd_menu_toggle(void) {
    g_overlay_menu.is_open = !g_overlay_menu.is_open;
    if (!g_overlay_menu.is_open) g_overlay_menu.page = ISSD_MENU_PAGE_MAIN;
    if (g_overlay_menu.is_open) {
        printf("[Overlay] Modern Menu opened.\n");
    } else {
        printf("[Overlay] Modern Menu closed.\n");
    }
}

void issd_menu_open(void) {
    g_overlay_menu.is_open = true;
}

void issd_menu_close(void) {
    g_overlay_menu.is_open = false;
}

bool issd_menu_is_open(void) {
    return g_overlay_menu.is_open;
}

/* Internal resolution only reaches the screen through the CRT filter, which
 * generates scanline darkening into a scaled buffer. Nearest and linear hand
 * the logical buffer straight to SDL, so the setting would be a label that
 * changes nothing -- keep the row inert rather than advertising six options. */
bool issd_menu_internal_res_applies(void) {
    return g_issd_config.scaling_filter == ISSD_FILTER_CRT;
}

/* Rows visible at once on the mods page; the list scrolls past that. */
#define MODS_VISIBLE_ROWS 16

static void mods_step(int direction) {
    const int rows = mods_row_count();
    int row = g_overlay_menu.current_item;
    /* Headers are labels, not choices, so the cursor passes over them.
     * The loop is bounded by the row count so an all-header list (no packs
     * installed at all) cannot spin. */
    for (int guard = 0; guard < rows; guard++) {
        row = (row + direction + rows) % rows;
        if (!mods_row_is_header(row)) break;
    }
    g_overlay_menu.current_item = row;

    if (row < g_overlay_menu.scroll)
        g_overlay_menu.scroll = row;
    else if (row >= g_overlay_menu.scroll + MODS_VISIBLE_ROWS)
        g_overlay_menu.scroll = row - MODS_VISIBLE_ROWS + 1;
    if (g_overlay_menu.scroll < 0) g_overlay_menu.scroll = 0;
}

bool issd_menu_navigate_up(void) {
    if (!g_overlay_menu.is_open) return false;
    if (g_overlay_menu.page == ISSD_MENU_PAGE_MODS) { mods_step(-1); return true; }
    g_overlay_menu.current_item = (g_overlay_menu.current_item - 1 + MENU_TOTAL_ITEMS) % MENU_TOTAL_ITEMS;
    return true;
}

bool issd_menu_navigate_down(void) {
    if (!g_overlay_menu.is_open) return false;
    if (g_overlay_menu.page == ISSD_MENU_PAGE_MODS) { mods_step(1); return true; }
    g_overlay_menu.current_item = (g_overlay_menu.current_item + 1) % MENU_TOTAL_ITEMS;
    return true;
}

static const int s_fps_presets[] = { 60, 120, 144, 165, 240, 0 };
#define TOTAL_FPS_PRESETS 6

bool issd_menu_navigate_left(void) {
    if (!g_overlay_menu.is_open) return false;
    if (g_overlay_menu.page == ISSD_MENU_PAGE_MODS) {
        mods_toggle_row(g_overlay_menu.current_item);
        return true;
    }
    switch (g_overlay_menu.current_item) {
        case 1: /* Schema */
            g_overlay_menu.control_schema = (IssdControlSchema)((g_overlay_menu.control_schema - 1 + 3) % 3);
            break;
        case 2: /* Mods page */
            mods_open();
            break;
        case 3: /* Aspect Ratio */
            g_issd_config.aspect_ratio = (IssdAspectRatio)((g_issd_config.aspect_ratio - 1 + ISSD_ASPECT_COUNT) % ISSD_ASPECT_COUNT);
            break;
        case 4: /* True Widescreen (FOV) */
            g_issd_config.true_widescreen = !g_issd_config.true_widescreen;
            break;
        case 5: /* Internal Resolution */
            if (!issd_menu_internal_res_applies()) break;
            g_issd_config.internal_res = (IssdInternalResolution)((g_issd_config.internal_res - 1 + 6) % 6);
            break;
        case 6: /* Filter */
            g_issd_config.scaling_filter = (IssdScalingFilter)((g_issd_config.scaling_filter - 1 + 3) % 3);
            break;
        case 7: { /* Target FPS */
            int idx = 0;
            for (int i = 0; i < TOTAL_FPS_PRESETS; i++) {
                if (g_issd_config.target_fps == s_fps_presets[i]) { idx = i; break; }
            }
            idx = (idx - 1 + TOTAL_FPS_PRESETS) % TOTAL_FPS_PRESETS;
            g_issd_config.target_fps = s_fps_presets[idx];
            break;
        }
        case 8: /* VSync */
            g_issd_config.vsync = !g_issd_config.vsync;
            break;
        case 9: /* Save Slot */
        case 10: /* Load Slot */
            g_overlay_menu.current_slot = (g_overlay_menu.current_slot - 1 + 8) % 8;
            break;
        case 11: /* Master Volume */
            if (g_issd_config.master_volume >= 10) g_issd_config.master_volume -= 10;
            break;
        case 12: /* Engine Mode */
            g_issd_config.engine_mode = (IssdEngineMode)!g_issd_config.engine_mode;
            break;
        case 14: /* HD Tiles: shown here, chosen on the Mods page */
            mods_open();
            break;
        case 13: /* Debug & Japanese Unhooked Code */
            g_issd_config.debug_unhooked_code = !g_issd_config.debug_unhooked_code;
            if (g_issd_config.debug_unhooked_code) {
                g_ram[0x1D854] = 1; g_ram[0x1D855] = 0;
                g_ram[0x1D856] = 1; g_ram[0x1D857] = 0;
                g_ram[0x1D858] = 1; g_ram[0x1D859] = 0;
            } else {
                g_ram[0x1D854] = 0; g_ram[0x1D855] = 0;
                g_ram[0x1D856] = 0; g_ram[0x1D857] = 0;
                g_ram[0x1D858] = 0; g_ram[0x1D859] = 0;
            }
            break;
        default:
            break;
    }
    return true;
}

bool issd_menu_navigate_right(void) {
    if (!g_overlay_menu.is_open) return false;
    if (g_overlay_menu.page == ISSD_MENU_PAGE_MODS) {
        mods_toggle_row(g_overlay_menu.current_item);
        return true;
    }
    switch (g_overlay_menu.current_item) {
        case 1: /* Schema */
            g_overlay_menu.control_schema = (IssdControlSchema)((g_overlay_menu.control_schema + 1) % 3);
            break;
        case 2: /* Mods page */
            mods_open();
            break;
        case 3: /* Aspect Ratio */
            g_issd_config.aspect_ratio = (IssdAspectRatio)((g_issd_config.aspect_ratio + 1) % ISSD_ASPECT_COUNT);
            break;
        case 4: /* True Widescreen (FOV) */
            g_issd_config.true_widescreen = !g_issd_config.true_widescreen;
            break;
        case 5: /* Internal Resolution */
            if (!issd_menu_internal_res_applies()) break;
            g_issd_config.internal_res = (IssdInternalResolution)((g_issd_config.internal_res + 1) % 6);
            break;
        case 6: /* Filter */
            g_issd_config.scaling_filter = (IssdScalingFilter)((g_issd_config.scaling_filter + 1) % 3);
            break;
        case 7: { /* Target FPS */
            int idx = 0;
            for (int i = 0; i < TOTAL_FPS_PRESETS; i++) {
                if (g_issd_config.target_fps == s_fps_presets[i]) { idx = i; break; }
            }
            idx = (idx + 1) % TOTAL_FPS_PRESETS;
            g_issd_config.target_fps = s_fps_presets[idx];
            break;
        }
        case 8: /* VSync */
            g_issd_config.vsync = !g_issd_config.vsync;
            break;
        case 9: /* Save Slot */
        case 10: /* Load Slot */
            g_overlay_menu.current_slot = (g_overlay_menu.current_slot + 1) % 8;
            break;
        case 11: /* Master Volume */
            if (g_issd_config.master_volume <= 90) g_issd_config.master_volume += 10;
            break;
        case 12: /* Engine Mode */
            g_issd_config.engine_mode = (IssdEngineMode)!g_issd_config.engine_mode;
            break;
        case 14: /* HD Tiles: shown here, chosen on the Mods page */
            mods_open();
            break;
        case 13: /* Debug & Japanese Unhooked Code */
            g_issd_config.debug_unhooked_code = !g_issd_config.debug_unhooked_code;
            if (g_issd_config.debug_unhooked_code) {
                g_ram[0x1D854] = 1; g_ram[0x1D855] = 0;
                g_ram[0x1D856] = 1; g_ram[0x1D857] = 0;
                g_ram[0x1D858] = 1; g_ram[0x1D859] = 0;
            } else {
                g_ram[0x1D854] = 0; g_ram[0x1D855] = 0;
                g_ram[0x1D856] = 0; g_ram[0x1D857] = 0;
                g_ram[0x1D858] = 0; g_ram[0x1D859] = 0;
            }
            break;
        default:
            break;
    }
    return true;
}

bool issd_menu_confirm(void) {
    if (!g_overlay_menu.is_open) return false;
    if (g_overlay_menu.page == ISSD_MENU_PAGE_MODS) {
        const int row = g_overlay_menu.current_item;
        if (row == mods_row_back()) {
            g_overlay_menu.page = ISSD_MENU_PAGE_MAIN;
            g_overlay_menu.current_item = 2;
        } else if (row == mods_row_apply()) {
            mods_store_selection();
            issd_config_save(&g_issd_config, NULL);
            issd_restart_application();
        } else {
            mods_toggle_row(row);
        }
        return true;
    }
    switch (g_overlay_menu.current_item) {
        case 0: /* Resume */
            issd_menu_close();
            break;
        case 2: /* Mods page */
            mods_open();
            break;
        case 1: /* Next Schema */
        case 3: /* Aspect */
        case 4: /* True Widescreen */
        case 5: /* Res */
        case 6: /* Filter */
        case 7: /* FPS */
        case 8: /* VSync */
            issd_menu_navigate_right();
            break;
        case 9: { /* Save State */
            if (issd_save_to_slot(g_overlay_menu.current_slot, NULL)) {
                snprintf(g_overlay_menu.status_message, sizeof(g_overlay_menu.status_message),
                         "State Saved: Slot %d", g_overlay_menu.current_slot + 1);
                issd_menu_notify(g_overlay_menu.status_message, 120);
            } else {
                snprintf(g_overlay_menu.status_message, sizeof(g_overlay_menu.status_message), "Save Failed!");
                issd_menu_notify("Save State Failed!", 120);
            }
            g_overlay_menu.status_timer = 120;
            break;
        }
        case 10: { /* Load State */
            if (issd_load_from_slot(g_overlay_menu.current_slot)) {
                snprintf(g_overlay_menu.status_message, sizeof(g_overlay_menu.status_message),
                         "State Loaded: Slot %d", g_overlay_menu.current_slot + 1);
                issd_menu_notify(g_overlay_menu.status_message, 120);
                issd_menu_close();
            } else {
                snprintf(g_overlay_menu.status_message, sizeof(g_overlay_menu.status_message),
                         "Slot %d is Empty!", g_overlay_menu.current_slot + 1);
                issd_menu_notify(g_overlay_menu.status_message, 120);
            }
            g_overlay_menu.status_timer = 120;
            break;
        }
        case 11: /* Volume */
        case 12: /* Engine Mode */
        case 13: /* Debug & JPN Mode */
        case 14: /* HD Tiles */
            issd_menu_navigate_right();
            break;
        case MENU_ITEM_RESTART: /* Save & Restart */
            /* A mod pack is applied to the cartridge image before the
             * engine boots, and rosters are cached as a match loads, so
             * choosing one mid-session changes nothing until the game
             * starts again. This is that restart. */
            issd_config_save(&g_issd_config, NULL);
            issd_restart_application();
            break;
        case MENU_ITEM_QUIT: /* Save & Quit */
            issd_config_save(&g_issd_config, NULL);
            issd_request_quit();
            break;
#ifdef ISSD_ANDROID
        case MENU_ITEM_PICK_ROM:
            issd_android_pick_rom();
            issd_menu_notify("Choose ROM in Android picker", 180);
            break;
        case MENU_ITEM_PICK_MODS_FOLDER:
            issd_android_pick_mods_folder();
            issd_menu_notify("Choose mods folder in Android picker", 180);
            break;
#endif
        default:
            break;
    }
    return true;
}

bool issd_menu_cancel(void) {
    if (!g_overlay_menu.is_open) return false;
    if (g_overlay_menu.page == ISSD_MENU_PAGE_MODS) {
        g_overlay_menu.page = ISSD_MENU_PAGE_MAIN;
        g_overlay_menu.current_item = 2;
        return true;
    }
    issd_menu_close();
    return true;
}

/* One row per pack, with the stack position shown so the order two packs
 * are applied in - which decides who wins where they overlap - is visible
 * rather than something to be inferred. */
static void issd_menu_render_mods(uint32_t *fb, int width, int height,
                                  int box_x, int box_y, int box_h) {
    DrawString(fb, width, height, box_x + 70, box_y + 4, "MODS", 0xFFFFD700);

    const int rosters = mods_roster_count();
    const int tiles = mods_tile_count();
    const int rows = mods_row_count();
    const int first = g_overlay_menu.scroll;
    int y = box_y + 16;

    for (int row = first; row < rows && row < first + MODS_VISIBLE_ROWS; row++, y += 11) {
        char text[44];
        uint32_t colour = 0xFFE0E0E0;

        if (row == 0) {
            snprintf(text, sizeof text, "-- TEAMS, STATS, FORMATIONS --");
            colour = 0xFF66CCFF;
        } else if (row == 1 + rosters) {
            snprintf(text, sizeof text, "-- HD TILES --");
            colour = 0xFF66CCFF;
        } else if (row == mods_row_apply()) {
            snprintf(text, sizeof text, "SAVE & RESTART (apply)");
            colour = 0xFFFFD700;
        } else if (row == mods_row_back()) {
            snprintf(text, sizeof text, "Back");
        } else if (row < 1 + rosters) {
            const int i = row - 1;
            const IssdModPack *pack = issd_mod_get_pack(i);
            const bool on = issd_mod_is_pack_enabled(i);
            snprintf(text, sizeof text, "[%c] %.30s", on ? 'x' : ' ',
                     pack && pack->name[0] ? pack->name : "(unnamed)");
            if (!on) colour = 0xFF909090;
        } else {
            const int i = row - (2 + rosters);
            const bool on = (i >= 0 && i < tiles) && issd_hd_is_enabled(i);
            snprintf(text, sizeof text, "[%c] %.30s", on ? 'x' : ' ',
                     issd_hd_available_name(i));
            if (!on) colour = 0xFF909090;
        }

        if (row == g_overlay_menu.current_item) {
            colour = 0xFF00FF66;
            DrawChar(fb, width, height, box_x + 4, y, '>', colour);
        }
        DrawString(fb, width, height, box_x + 14, y, text, colour);
    }

    if (!rosters && !tiles)
        DrawString(fb, width, height, box_x + 14, box_y + 30,
                   "Nothing in mods/ yet.", 0xFF909090);

    char headline[32], detail[32];
    issd_mod_result_lines(headline, sizeof headline, detail, sizeof detail);
    const IssdModResult *res = issd_mod_last_result();
    const uint32_t tone = res->errors ? 0xFFFF5555 :
                          res->warnings ? 0xFFFFAA00 : 0xFF88FF88;
    DrawString(fb, width, height, box_x + 8, box_y + box_h - 33, headline, tone);
    DrawString(fb, width, height, box_x + 8, box_y + box_h - 22, detail, 0xFFB0B0B0);
    DrawString(fb, width, height, box_x + 8, box_y + box_h - 11,
               "A/< >:Toggle  ESC:Back", 0xFF888888);
}

void issd_menu_render(uint32_t *fb, int width, int height) {
    if (!g_overlay_menu.is_open || !fb) return;

    /* 1. Darken background (translucent alpha blend) */
    for (int i = 0; i < width * height; i++) {
        uint32_t p = fb[i];
        uint32_t r = ((p >> 16) & 0xFF) / 3;
        uint32_t g = ((p >> 8) & 0xFF) / 3;
        uint32_t b = (p & 0xFF) / 3;
        fb[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
    }

    /* 2. Menu Window Box (center) */
    int box_w = 240;
    int box_h = 220;
    int box_x = (width - box_w) / 2;
    int box_y = (height - box_h) / 2;
    if (box_x < 0) box_x = 0;
    if (box_y < 0) box_y = 0;

    DrawBox(fb, width, height, box_x, box_y, box_w, box_h, 0xFF00E5FF);
    DrawBox(fb, width, height, box_x + 1, box_y + 1, box_w - 2, box_h - 2, 0xFF002244);

    if (g_overlay_menu.page == ISSD_MENU_PAGE_MODS) {
        issd_menu_render_mods(fb, width, height, box_x, box_y, box_h);
        return;
    }

    /* 3. Title */
    DrawString(fb, width, height, box_x + 44, box_y + 4, "ISSD NATIVE MENU", 0xFFFFD700);

    /* Display Strings */
    const char *schema_str = (g_overlay_menu.control_schema == ISSD_SCHEMA_CLASSIC) ? "CLASSIC" :
                             (g_overlay_menu.control_schema == ISSD_SCHEMA_FIFA)    ? "FIFA" : "PES";


    const char *aspect_str = (g_issd_config.aspect_ratio == ISSD_ASPECT_4_3)     ? "4:3 CRT" :
                             (g_issd_config.aspect_ratio == ISSD_ASPECT_8_7)     ? "8:7 PIXEL" :
                             (g_issd_config.aspect_ratio == ISSD_ASPECT_16_9)    ? "16:9 WIDE" :
                             (g_issd_config.aspect_ratio == ISSD_ASPECT_16_10)   ? "16:10 PC" :
                             (g_issd_config.aspect_ratio == ISSD_ASPECT_21_9)    ? "21:9 ULTRA" :
                             (g_issd_config.aspect_ratio == ISSD_ASPECT_AUTHENTIC) ? "AUTHENTIC 320" : "INTEGER";

    const char *res_str = (g_issd_config.internal_res == ISSD_RES_1X)     ? "1X (256x224)" :
                          (g_issd_config.internal_res == ISSD_RES_2X)     ? "2X (512x448)" :
                          (g_issd_config.internal_res == ISSD_RES_3X)     ? "3X (720p HD)" :
                          (g_issd_config.internal_res == ISSD_RES_4X)     ? "4X (1080p FHD)" :
                          (g_issd_config.internal_res == ISSD_RES_6X)     ? "6X (1440p QHD)" : "8X (4K UHD)";

    const char *filter_str = (g_issd_config.scaling_filter == ISSD_FILTER_NEAREST) ? "NEAREST (SHARP)" :
                             (g_issd_config.scaling_filter == ISSD_FILTER_LINEAR)  ? "LINEAR (SMOOTH)" : "CRT SCANLINES";

    char fps_str[24];
    if (g_issd_config.target_fps > 0) {
        snprintf(fps_str, sizeof(fps_str), "%d Hz", g_issd_config.target_fps);
    } else {
        snprintf(fps_str, sizeof(fps_str), "UNCAPPED");
    }

    const char *mode_str = (g_issd_config.engine_mode == ISSD_MODE_ENHANCED) ? "ENHANCED 60Hz" : "CLASSIC SNES";

    char items[MENU_TOTAL_ITEMS][44];
    snprintf(items[0], sizeof(items[0]), "Resume Match");
    snprintf(items[1], sizeof(items[1]), "Controls:   <%s>", schema_str);
    snprintf(items[2], sizeof(items[2]), "Mods...     <%s>", issd_menu_mods_label());
    snprintf(items[3], sizeof(items[3]), "Aspect:     <%s>", aspect_str);
    snprintf(items[4], sizeof(items[4]), "Widescreen: <%s>", g_issd_config.true_widescreen ? "ON (TRUE FOV)" : "OFF (4:3 NATIVE)");
    snprintf(items[5], sizeof(items[5]), "Internal:   <%s>",
             issd_menu_internal_res_applies() ? res_str : "CRT FILTER ONLY");
    snprintf(items[6], sizeof(items[6]), "Filter:     <%s>", filter_str);
    snprintf(items[7], sizeof(items[7]), "Target FPS: <%s>", fps_str);
    snprintf(items[8], sizeof(items[8]), "VSync:      <%s>", g_issd_config.vsync ? "ON" : "OFF");
    char slot_info[64];
    bool has_save = issd_save_get_info(g_overlay_menu.current_slot, slot_info, sizeof(slot_info));
    snprintf(items[9], sizeof(items[9]), "Save State: <Slot %d>", g_overlay_menu.current_slot + 1);
    snprintf(items[10], sizeof(items[10]), "Load State: <Slot %d%s>", g_overlay_menu.current_slot + 1, has_save ? "" : " (Empty)");
    snprintf(items[11], sizeof(items[11]), "Volume:     <%d%%>", g_issd_config.master_volume);
    snprintf(items[12], sizeof(items[12]), "Engine:     <%s>", mode_str);
    snprintf(items[13], sizeof(items[13]), "Debug/JPN:  <%s>", g_issd_config.debug_unhooked_code ? "ENABLED" : "DISABLED");
    snprintf(items[14], sizeof(items[14]), "HD Tiles:   <%s>", issd_menu_hd_pack_label());
#ifdef ISSD_ANDROID
    snprintf(items[MENU_ITEM_PICK_ROM], sizeof(items[MENU_ITEM_PICK_ROM]), "Choose ROM File...");
    snprintf(items[MENU_ITEM_PICK_MODS_FOLDER], sizeof(items[MENU_ITEM_PICK_MODS_FOLDER]), "Choose Mods Folder...");
#endif
    snprintf(items[MENU_ITEM_RESTART], sizeof(items[MENU_ITEM_RESTART]), "Save & Restart (applies mods)");
#ifdef ISSD_ANDROID
    snprintf(items[MENU_ITEM_QUIT], sizeof(items[MENU_ITEM_QUIT]), "Save & Quit");
#else
    snprintf(items[MENU_ITEM_QUIT], sizeof(items[MENU_ITEM_QUIT]), "Save & Quit to Desktop");
#endif

    int start_y = box_y + 14;
    int row_h = MENU_TOTAL_ITEMS > 17 ? 10 : 11;
    for (int i = 0; i < MENU_TOTAL_ITEMS; i++) {
        uint32_t color = (i == g_overlay_menu.current_item) ? 0xFF00FF66 : 0xFFE0E0E0;
        int item_y = start_y + i * row_h;
        if (i == g_overlay_menu.current_item) {
            DrawChar(fb, width, height, box_x + 4, item_y, '>', 0xFF00FF66);
        }
        DrawString(fb, width, height, box_x + 14, item_y, items[i], color);
    }

    /* 4. Footer info / Status Message */
    if (g_overlay_menu.status_timer > 0) {
        g_overlay_menu.status_timer--;
        DrawString(fb, width, height, box_x + 8, box_y + box_h - 11, g_overlay_menu.status_message, 0xFFFFAA00);
    } else {
        DrawString(fb, width, height, box_x + 8, box_y + box_h - 11, "A:Select  < >:Change  ESC:Back", 0xFF888888);
    }
}

/* A line over the game itself, for things the player must see even with
 * the menu closed - above all whether the mods they just restarted for
 * actually applied. */
static char s_notice[96];
static int  s_notice_frames;

void issd_menu_notify(const char *message, int frames) {
    if (!message) return;
    snprintf(s_notice, sizeof s_notice, "%s", message);
    s_notice_frames = frames;
}

void issd_menu_render_notification(uint32_t *fb, int width, int height) {
    if (!fb || s_notice_frames <= 0 || !s_notice[0]) return;
    s_notice_frames--;

    /* The message is as long as it needs to be to say what happened, and
     * the screen is 256 pixels wide in 4:3 - so it wraps rather than
     * running off the edge, which would cut off exactly the part that says
     * something went wrong. */
    const int glyph = 8;
    int fit = (width - 16) / glyph;
    if (fit < 8) fit = 8;

    char line[2][96];
    int lines = 1;
    const int len = (int)strlen(s_notice);
    if (len <= fit) {
        snprintf(line[0], sizeof line[0], "%s", s_notice);
    } else {
        int split = fit;
        while (split > 0 && s_notice[split] != ' ') split--;
        if (split == 0) split = fit;          /* one long word: hard break */
        snprintf(line[0], sizeof line[0], "%.*s", split, s_notice);
        const char *rest = s_notice + split;
        while (*rest == ' ') rest++;
        snprintf(line[1], sizeof line[1], "%.*s", fit, rest);
        lines = 2;
    }

    int text_w = 0;
    for (int i = 0; i < lines; i++) {
        const int w = (int)strlen(line[i]) * glyph;
        if (w > text_w) text_w = w;
    }
    const int w = text_w + 12;
    const int h = 6 + lines * 9;
    int x = (width - w) / 2;
    const int y = height - h - 6;
    if (x < 2) x = 2;

    /* Darken behind the text rather than filling it: the message sits over
     * whatever is on screen at boot, which is rarely a flat colour. */
    for (int py = y; py < y + h && py < height; py++)
        for (int px = x; px < x + w && px < width; px++) {
            const uint32_t p = fb[py * width + px];
            fb[py * width + px] = 0xFF000000 |
                ((((p >> 16) & 0xFF) / 4) << 16) |
                ((((p >> 8) & 0xFF) / 4) << 8) |
                (((p & 0xFF) / 4));
        }

    const IssdModResult *r = issd_mod_last_result();
    const uint32_t colour = r->errors ? 0xFFFF5555 :
                            r->warnings ? 0xFFFFAA00 : 0xFF88FF88;
    for (int i = 0; i < lines; i++)
        DrawString(fb, width, height, x + 6, y + 3 + i * 9, line[i], colour);
}

/* ------------------------------------------- stadium name plate ------ */

/* The cartridge draws a stadium's name on the select screen from a list of
 * pre-rendered plate graphics, chosen by slot number. There are more of
 * them than there are stadiums - the ninth reads ALL STAR - but only so
 * many, and none of them says what a mod pack called its stadium. That is
 * the one thing that capped how many stadiums were worth adding.
 *
 * So the host repaints the plate. The rectangle, the grey gradient down it
 * and the blue of its lettering were all measured off the real screen.
 * Everything else about the screen is left exactly as the game drew it.
 */
#define PLATE_X0      56
#define PLATE_X1     127
#define PLATE_Y0      48
#define PLATE_Y1      63
#define PLATE_INK   0xFF1039B5u

/* Row shades sampled from a letter-free column of the real plate. */
static const uint8_t kPlateRow[PLATE_Y1 - PLATE_Y0 + 1] = {
    255, 231, 231, 214, 214, 214, 231, 231,
    198, 198, 198, 173, 173, 173, 198,  99
};

/* Identifying the screen took two attempts. $7E0076 looked like a screen
 * id across one pair of captures and turned out to be a frame counter.
 * The four background scroll positions are the real signature: 52, 44, 48
 * and 40 on this screen, at every frame and for every stadium, and
 * different on team select - which shares the same game mode - as well as
 * on the pre-match screen and in play. */
#define STADIUM_SELECTOR 0x154Cu

static bool on_stadium_select(void) {
    static const uint8_t kScroll[4] = { 52, 44, 48, 40 };
    if (g_ram[0x32] != 0x06 || g_ram[0x70] != 0x0C) return false;
    for (int i = 0; i < 4; i++)
        if (g_ram[0x18 + i * 2] != kScroll[i]) return false;
    return true;
}

void issd_menu_render_stadium_plate(uint32_t *fb, int width, int height,
                                    int margin) {
    if (!fb) return;
    if (!on_stadium_select()) return;

    const int slot = g_ram[STADIUM_SELECTOR];
    const char *name = issd_mod_stadium_plate_name(slot);
    if (!name || !name[0]) return;        /* the cartridge's own plate stands */

    /* Condense before truncating: the plate is nine characters wide at the
     * normal pitch and twelve at the tighter one, which is enough for the
     * kind of name a ground actually has. */
    int len = (int)strlen(name);
    const int room = PLATE_X1 - PLATE_X0 + 1;
    int advance = 8;
    if (len * advance > room) advance = 6;
    if (len * advance > room) len = room / advance;

    for (int y = PLATE_Y0; y <= PLATE_Y1; y++) {
        if (y < 0 || y >= height) continue;
        const uint8_t v = kPlateRow[y - PLATE_Y0];
        const uint32_t shade = 0xFF000000u | ((uint32_t)v << 16) |
                               ((uint32_t)v << 8) | v;
        for (int x = PLATE_X0; x <= PLATE_X1; x++) {
            const int px = margin + x;
            if (px < 0 || px >= width) continue;
            fb[(size_t)y * width + px] = shade;
        }
    }

    int x = margin + PLATE_X0 + (room - len * advance) / 2;
    const int y = PLATE_Y0 + 4;
    for (int i = 0; i < len; i++, x += advance)
        DrawChar(fb, width, height, x, y, name[i], PLATE_INK);
}

/* ---------------------------------------------- team name plate ------ */

/* The team select screen names the highlighted team on a blue plate beside
 * its flag, and like the stadium plate that name is a pre-rendered graphic:
 * there is one per team and no way to add a word the cartridge does not
 * already draw. A pack that turns Uruguay into Chivas gets everything else -
 * the squad, the shape, the strip - and a plate still reading URUGUAY.
 *
 * So the host repaints it. Measured off the real screen: the plate is x
 * 160-231, y 32-46, its blue runs as a gradient down the rows, and the
 * lettering is yellow with a magenta outline. The flag to its left is left
 * alone - it is the team's own and a club has no flag anyway.
 *
 * $7E1526 holds the highlighted team doubled, which is how the screen
 * indexes its tables; found by capturing the six cells of one group and
 * looking for the byte that counted 60, 62, 64, 66, 68, 70. */
#define TEAM_PLATE_X0   160
#define TEAM_PLATE_X1   231
#define TEAM_PLATE_Y0    32
#define TEAM_PLATE_Y1    46
#define TEAM_PLATE_INK    0xFFFFFF00u   /* yellow */
#define TEAM_PLATE_EDGE   0xFFEF0063u   /* magenta outline */
#define TEAM_SELECTOR   0x1526u

/* The plate's blue, row by row, sampled from a letter-free column. */
static const uint32_t kTeamPlateRow[TEAM_PLATE_Y1 - TEAM_PLATE_Y0 + 1] = {
    0xFF9CEFFFu, 0xFF31BDFFu, 0xFF31BDFFu, 0xFF4ACEFFu, 0xFF4ACEFFu,
    0xFF4ACEFFu, 0xFF31BDFFu, 0xFF31BDFFu, 0xFF31A5FFu, 0xFF31A5FFu,
    0xFF31A5FFu, 0xFF2184FFu, 0xFF2184FFu, 0xFF2184FFu, 0xFF31A5FFu
};

/* The two select screens share a game mode, so the scroll positions are what
 * tells them apart: 20, 36, 16, 32 here against the stadium screen's 52, 44,
 * 48, 40. */
static bool on_team_select(void) {
    static const uint8_t kScroll[4] = { 20, 36, 16, 32 };
    if (g_ram[0x32] != 0x06 || g_ram[0x70] != 0x0C) return false;
    for (int i = 0; i < 4; i++)
        if (g_ram[0x18 + i * 2] != kScroll[i]) return false;
    return true;
}

void issd_menu_render_team_plate(uint32_t *fb, int width, int height,
                                 int margin) {
    if (!fb) return;
    if (!on_team_select()) return;

    const int team = g_ram[TEAM_SELECTOR] / 2;
    const char *name = issd_mod_team_plate_name(team);
    if (!name || !name[0]) return;      /* the cartridge's own plate stands */

    int len = (int)strlen(name);
    const int room = TEAM_PLATE_X1 - TEAM_PLATE_X0 + 1;
    int advance = 8;
    if (len * advance > room) advance = 6;
    if (len * advance > room) len = room / advance;

    for (int y = TEAM_PLATE_Y0; y <= TEAM_PLATE_Y1; y++) {
        if (y < 0 || y >= height) continue;
        const uint32_t shade = kTeamPlateRow[y - TEAM_PLATE_Y0];
        for (int x = TEAM_PLATE_X0; x <= TEAM_PLATE_X1; x++) {
            const int px = margin + x;
            if (px < 0 || px >= width) continue;
            fb[(size_t)y * width + px] = shade;
        }
    }

    /* Outline first, then the letter on top: that is what the cartridge's
     * own plates look like, and it keeps yellow legible on pale blue. */
    const int x0 = margin + TEAM_PLATE_X0 + (room - len * advance) / 2;
    const int y = TEAM_PLATE_Y0 + 4;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (!dx && !dy) continue;
            int x = x0 + dx;
            for (int i = 0; i < len; i++, x += advance)
                DrawChar(fb, width, height, x, y + dy, name[i], TEAM_PLATE_EDGE);
        }
    }
    int x = x0;
    for (int i = 0; i < len; i++, x += advance)
        DrawChar(fb, width, height, x, y, name[i], TEAM_PLATE_INK);
}

/* --------------------------------------------- team photograph ------- */

/* Beside the plate the select screen shows the squad lined up for a
 * photograph. Those are per-team graphics in the cartridge, compressed, and
 * there is no spare one to point a new club at - so the host draws over the
 * frame instead, from a BMP the pack ships.
 *
 * The frame's inside was measured off the screen: x 24-119, y 40-111,
 * ninety-six by seventy-two, with the cartridge's own grey mount left
 * showing around it. Any size of BMP is accepted and sampled to fit, which
 * keeps the authoring end forgiving.
 */
#define PHOTO_X0   24
#define PHOTO_Y0   40
#define PHOTO_W    96
#define PHOTO_H    72

#pragma pack(push, 1)
typedef struct {
    uint16_t type; uint32_t size; uint16_t r1, r2; uint32_t offset;
} PhotoFileHeader;
typedef struct {
    uint32_t size; int32_t w, h; uint16_t planes, bits;
    uint32_t compression, image_bytes;
    int32_t  xppm, yppm; uint32_t used, important;
} PhotoInfoHeader;
#pragma pack(pop)

/* One photograph is cached at a time: the screen shows one at a time, and
 * re-reading the file every frame while a player scrolls the grid would be
 * a silly amount of disk work. */
static uint32_t s_photo[PHOTO_W * PHOTO_H];
static int      s_photo_team = -1;
static bool     s_photo_ok;

static bool photo_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    PhotoFileHeader fh;
    PhotoInfoHeader ih;
    if (fread(&fh, sizeof fh, 1, f) != 1 || fread(&ih, sizeof ih, 1, f) != 1 ||
        fh.type != 0x4D42 || ih.bits != 32 || ih.compression > 3) {
        fclose(f);
        return false;
    }
    const int w = ih.w;
    const int h = ih.h < 0 ? -ih.h : ih.h;
    if (w <= 0 || h <= 0 || (long)w * h > 4L * 1024 * 1024) { fclose(f); return false; }

    uint32_t *src = (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));
    if (!src) { fclose(f); return false; }
    if (fseek(f, (long)fh.offset, SEEK_SET) != 0 ||
        fread(src, sizeof(uint32_t), (size_t)w * h, f) != (size_t)w * h) {
        free(src); fclose(f); return false;
    }
    fclose(f);

    for (int y = 0; y < PHOTO_H; y++) {
        int sy = y * h / PHOTO_H;
        if (ih.h > 0) sy = h - 1 - sy;          /* bottom-up, as most save */
        for (int x = 0; x < PHOTO_W; x++) {
            const int sx = x * w / PHOTO_W;
            s_photo[y * PHOTO_W + x] = 0xFF000000u | src[sy * w + sx];
        }
    }
    free(src);
    return true;
}

void issd_menu_render_team_photo(uint32_t *fb, int width, int height,
                                 int margin) {
    if (!fb) return;
    if (!on_team_select()) return;

    const int team = g_ram[TEAM_SELECTOR] / 2;
    if (team != s_photo_team) {
        char path[512];
        s_photo_team = team;
        s_photo_ok = issd_mod_team_photo_path(team, path, sizeof path) &&
                     photo_load(path);
        if (!s_photo_ok && path[0] && issd_mod_team_photo_path(team, path, sizeof path))
            fprintf(stderr, "[Mods] cannot read the squad photograph '%s'.\n", path);
    }
    if (!s_photo_ok) return;                 /* the cartridge's own stands */

    for (int y = 0; y < PHOTO_H; y++) {
        const int py = PHOTO_Y0 + y;
        if (py < 0 || py >= height) continue;
        for (int x = 0; x < PHOTO_W; x++) {
            const int px = margin + PHOTO_X0 + x;
            if (px < 0 || px >= width) continue;
            fb[(size_t)py * width + px] = s_photo[y * PHOTO_W + x];
        }
    }
}

/* ------------------------------------------- team grid names --------- */

/* The plate beside the flag is not the only place a team is named: the
 * grid underneath labels all six cells of the group, and those are
 * graphics too. Renaming only the plate leaves a pack looking half
 * applied - the plate says CHIVAS and the cell below still says ALL STAR.
 *
 * The cell rectangles were measured off the screen: three columns centred
 * on x 71, 127 and 183, two rows of text at y 168 and 200, white on the
 * panel's dark blue. Which team is in which cell comes from the
 * cartridge's own table rather than from anything assumed here. */
static const int kGridColumn[3] = { 71, 127, 183 };
static const int kGridRow[2]    = { 168, 200 };
#define GRID_CELL_W      54
#define GRID_TEXT_H       8
#define GRID_PANEL   0xFF00108Cu
#define GRID_INK     0xFFEFFFFFu

void issd_menu_render_team_grid(uint32_t *fb, int width, int height,
                                int margin) {
    if (!fb) return;
    if (!on_team_select()) return;

    /* Which page is showing: find the highlighted team's cell and round
     * down to its group of six. */
    const int here = issd_mod_team_cell(g_ram[TEAM_SELECTOR] / 2);
    if (here < 0) return;
    const int first = (here / 6) * 6;

    for (int slot = 0; slot < 6; slot++) {
        const int team = issd_mod_cell_team(first + slot);
        if (team < 0) continue;
        const char *name = issd_mod_team_plate_name(team);
        if (!name || !name[0]) continue;   /* the cartridge's own stands */

        const int cx = kGridColumn[slot % 3];
        const int ty = kGridRow[slot / 3];

        for (int y = ty; y < ty + GRID_TEXT_H; y++) {
            if (y < 0 || y >= height) continue;
            for (int x = cx - GRID_CELL_W / 2; x <= cx + GRID_CELL_W / 2; x++) {
                const int px = margin + x;
                if (px < 0 || px >= width) continue;
                fb[(size_t)y * width + px] = GRID_PANEL;
            }
        }

        /* The cartridge's own labels are a condensed font this one has no
         * match for, so the name is squeezed and then cut. */
        int len = (int)strlen(name);
        int advance = 6;
        if (len * advance > GRID_CELL_W) advance = 5;
        if (len * advance > GRID_CELL_W) len = GRID_CELL_W / advance;
        int x = margin + cx - (len * advance) / 2;
        for (int i = 0; i < len; i++, x += advance)
            DrawChar(fb, width, height, x, ty, name[i], GRID_INK);
    }
}

/* ------------------------------------------- team flags --------------- */

#define FLAG_W 24
#define FLAG_H 16

static uint32_t s_flags[42][FLAG_W * FLAG_H];
static bool     s_flag_loaded[42];
static bool     s_flag_ok[42];

static bool flag_load(int team_id, const char *path) {
    if (team_id < 0 || team_id >= 42 || !path || !path[0]) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    PhotoFileHeader fh;
    PhotoInfoHeader ih;
    if (fread(&fh, sizeof fh, 1, f) != 1 || fread(&ih, sizeof ih, 1, f) != 1 ||
        fh.type != 0x4D42 || ih.bits != 32 || ih.compression > 3) {
        fclose(f);
        return false;
    }
    const int w = ih.w;
    const int h = ih.h < 0 ? -ih.h : ih.h;
    if (w <= 0 || h <= 0 || (long)w * h > 4L * 1024 * 1024) { fclose(f); return false; }

    uint32_t *src = (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));
    if (!src) { fclose(f); return false; }
    if (fseek(f, (long)fh.offset, SEEK_SET) != 0 ||
        fread(src, sizeof(uint32_t), (size_t)w * h, f) != (size_t)w * h) {
        free(src); fclose(f); return false;
    }
    fclose(f);

    for (int y = 0; y < FLAG_H; y++) {
        int sy = y * h / FLAG_H;
        if (ih.h > 0) sy = h - 1 - sy;
        for (int x = 0; x < FLAG_W; x++) {
            const int sx = x * w / FLAG_W;
            s_flags[team_id][y * FLAG_W + x] = 0xFF000000u | src[sy * w + sx];
        }
    }
    free(src);
    return true;
}

static void ensure_flag_loaded(int team_id) {
    if (team_id < 0 || team_id >= 42 || s_flag_loaded[team_id]) return;
    s_flag_loaded[team_id] = true;
    char path[512];
    s_flag_ok[team_id] = issd_mod_team_flag_path(team_id, path, sizeof path) &&
                         flag_load(team_id, path);
}

static void draw_flag(uint32_t *fb, int width, int height, int dst_x, int dst_y, int team_id) {
    if (team_id < 0 || team_id >= 42) return;
    ensure_flag_loaded(team_id);
    if (!s_flag_ok[team_id]) return;
    for (int y = 0; y < FLAG_H; y++) {
        const int py = dst_y + y;
        if (py < 0 || py >= height) continue;
        for (int x = 0; x < FLAG_W; x++) {
            const int px = dst_x + x;
            if (px < 0 || px >= width) continue;
            uint32_t color = s_flags[team_id][y * FLAG_W + x];
            if ((color >> 24) != 0) {
                fb[(size_t)py * width + px] = color;
            }
        }
    }
}

static void draw_outlined_text(uint32_t *fb, int width, int height,
                               int x0, int y0, const char *text, int advance,
                               uint32_t text_color, uint32_t edge_color) {
    if (!text || !text[0]) return;
    int len = (int)strlen(text);
    if (edge_color != 0) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (!dx && !dy) continue;
                int x = x0 + dx;
                for (int i = 0; i < len; i++, x += advance)
                    DrawChar(fb, width, height, x, y0 + dy, text[i], edge_color);
            }
        }
    }
    int x = x0;
    for (int i = 0; i < len; i++, x += advance)
        DrawChar(fb, width, height, x, y0, text[i], text_color);
}

static void draw_gradient_plate(uint32_t *fb, int width, int height, int margin,
                                int x0, int x1, int y0, const char *name) {
    if (!name || !name[0]) return;
    int len = (int)strlen(name);
    const int room = x1 - x0 + 1;
    int advance = 8;
    if (len * advance > room) advance = 6;
    if (len * advance > room) len = room / advance;

    for (int y = y0; y <= y0 + (TEAM_PLATE_Y1 - TEAM_PLATE_Y0); y++) {
        if (y < 0 || y >= height) continue;
        const uint32_t shade = kTeamPlateRow[y - y0];
        for (int x = x0; x <= x1; x++) {
            const int px = margin + x;
            if (px < 0 || px >= width) continue;
            fb[(size_t)y * width + px] = shade;
        }
    }

    const int tx = margin + x0 + (room - len * advance) / 2;
    const int ty = y0 + 4;
    draw_outlined_text(fb, width, height, tx, ty, name, advance,
                       TEAM_PLATE_INK, TEAM_PLATE_EDGE);
}

static bool on_handicap_screen(void) {
    return (g_ram[0x32] == 0x06 && g_ram[0x70] == 0x0C &&
            g_ram[0x18] == 116 && g_ram[0x1A] == 28);
}

static bool on_tonights_game_screen(void) {
    return (g_ram[0x32] == 0x06 && g_ram[0x70] == 0x0C &&
            g_ram[0x18] == 116 && g_ram[0x1A] == 52);
}

static bool on_prematch_presentation_screen(void) {
    return (g_ram[0x32] == 0x06 && g_ram[0x70] == 0x0F && g_ram[0x1A] == 60);
}

void issd_menu_render_team_flags(uint32_t *fb, int width, int height, int margin) {
    if (!fb) return;

    /* 1. Team Selection Screen */
    if (on_team_select()) {
        const int sel_team = g_ram[TEAM_SELECTOR] / 2;
        if (sel_team >= 0 && sel_team < 42) {
            draw_flag(fb, width, height, margin + 135, 32, sel_team);
        }

        const int here = issd_mod_team_cell(sel_team);
        if (here >= 0) {
            const int first = (here / 6) * 6;
            static const int kFlagCol[3] = { 60, 116, 172 };
            for (int slot = 0; slot < 6; slot++) {
                const int team = issd_mod_cell_team(first + slot);
                if (team < 0 || team >= 42) continue;
                const int fx = margin + kFlagCol[slot % 3];
                const int fy = (slot < 3) ? 151 : 183;
                draw_flag(fb, width, height, fx, fy, team);
            }
        }
        return;
    }

    /* 2. Handicap Screen */
    if (on_handicap_screen()) {
        const int p1_team = g_ram[0x0DA0] / 2;
        const int p2_team = g_ram[0x0EA0] / 2;

        if (p1_team >= 0 && p1_team < 42) {
            draw_flag(fb, width, height, margin + 92, 31, p1_team);
            const char *p1_name = issd_mod_team_plate_name(p1_team);
            if (p1_name && p1_name[0]) {
                for (int y = 47; y <= 55; y++) {
                    for (int x = 76; x <= 124; x++) {
                        fb[(size_t)y * width + (margin + x)] = 0xFF2152BDu;
                    }
                }
                int len = (int)strlen(p1_name);
                int advance = 6;
                if (len * advance > 48) advance = 5;
                if (len * advance > 48) len = 48 / advance;
                int sx = margin + 104 - (len * advance) / 2;
                draw_outlined_text(fb, width, height, sx, 48, p1_name, advance,
                                   0xFFFFFFFFu, 0xFF000000u);
            }
        }

        if (p2_team >= 0 && p2_team < 42) {
            draw_flag(fb, width, height, margin + 140, 31, p2_team);
            const char *p2_name = issd_mod_team_plate_name(p2_team);
            if (p2_name && p2_name[0]) {
                for (int y = 47; y <= 55; y++) {
                    for (int x = 136; x <= 184; x++) {
                        fb[(size_t)y * width + (margin + x)] = 0xFF2152BDu;
                    }
                }
                int len = (int)strlen(p2_name);
                int advance = 6;
                if (len * advance > 48) advance = 5;
                if (len * advance > 48) len = 48 / advance;
                int sx = margin + 152 - (len * advance) / 2;
                draw_outlined_text(fb, width, height, sx, 48, p2_name, advance,
                                   0xFFFFFFFFu, 0xFF000000u);
            }
        }
        return;
    }

    /* 3. Tonight's Game Screen */
    if (on_tonights_game_screen()) {
        const int p1_team = g_ram[0x0DA0] / 2;
        const int p2_team = g_ram[0x0EA0] / 2;

        if (p1_team >= 0 && p1_team < 42) {
            draw_flag(fb, width, height, margin + 88, 55, p1_team);
            const char *p1_name = issd_mod_team_plate_name(p1_team);
            if (p1_name && p1_name[0]) {
                draw_gradient_plate(fb, width, height, margin, 17, 86, 56, p1_name);
            }
        }

        if (p2_team >= 0 && p2_team < 42) {
            draw_flag(fb, width, height, margin + 144, 55, p2_team);
            const char *p2_name = issd_mod_team_plate_name(p2_team);
            if (p2_name && p2_name[0]) {
                draw_gradient_plate(fb, width, height, margin, 169, 238, 56, p2_name);
            }
        }
        return;
    }

    /* 4. Pre-Match Presentation Screen (Stadium Fly-in / Vs Banner) */
    if (on_prematch_presentation_screen()) {
        const int p1_team = g_ram[0x0DA0] / 2;
        const int p2_team = g_ram[0x0EA0] / 2;

        if (p1_team >= 0 && p1_team < 42) {
            draw_flag(fb, width, height, margin + 104, 64, p1_team);
            const char *p1_name = issd_mod_team_plate_name(p1_team);
            if (p1_name && p1_name[0]) {
                for (int y = 63; y <= 82; y++) {
                    for (int x = 16; x <= 102; x++) {
                        fb[(size_t)y * width + (margin + x)] = 0xFF4A4AFFu;
                    }
                }
                int len = (int)strlen(p1_name);
                int advance = 7;
                if (len * advance > 84) advance = 6;
                if (len * advance > 84) len = 84 / advance;
                int sx = margin + 100 - len * advance;
                draw_outlined_text(fb, width, height, sx, 66, p1_name, advance,
                                   TEAM_PLATE_INK, TEAM_PLATE_EDGE);
            }
        }

        if (p2_team >= 0 && p2_team < 42) {
            draw_flag(fb, width, height, margin + 128, 64, p2_team);
            const char *p2_name = issd_mod_team_plate_name(p2_team);
            if (p2_name && p2_name[0]) {
                for (int y = 63; y <= 82; y++) {
                    for (int x = 153; x <= 239; x++) {
                        fb[(size_t)y * width + (margin + x)] = 0xFF4A4AFFu;
                    }
                }
                int len = (int)strlen(p2_name);
                int advance = 7;
                if (len * advance > 84) advance = 6;
                if (len * advance > 84) len = 84 / advance;
                int sx = margin + 156;
                draw_outlined_text(fb, width, height, sx, 66, p2_name, advance,
                                   TEAM_PLATE_INK, TEAM_PLATE_EDGE);
            }
        }
        return;
    }

    /* 5. In-Match HUD Scoreboard: Live play (Mode 0x08) */
    if (g_ram[0x70] == 0x08) {
        const int p1_team = g_ram[0x0DA0] / 2;
        const int p2_team = g_ram[0x0EA0] / 2;

        if (p1_team >= 0 && p1_team < 42) {
            draw_flag(fb, width, height, margin + 12, 8, p1_team);
            const char *p1_name = issd_mod_team_plate_name(p1_team);
            if (p1_name && p1_name[0]) {
                for (int y = 24; y < 32; y++) {
                    for (int x = 8; x < 44; x++) {
                        fb[(size_t)y * width + (margin + x)] = 0xFF00108Cu;
                    }
                }
                int len = (int)strlen(p1_name);
                int advance = 5;
                if (len * advance > 36) len = 36 / advance;
                int sx = margin + 26 - (len * advance) / 2;
                draw_outlined_text(fb, width, height, sx, 24, p1_name, advance,
                                   0xFFFFFFFFu, 0xFF000000u);
            }
        }

        if (p2_team >= 0 && p2_team < 42) {
            draw_flag(fb, width, height, margin + 84, 8, p2_team);
            const char *p2_name = issd_mod_team_plate_name(p2_team);
            if (p2_name && p2_name[0]) {
                for (int y = 24; y < 32; y++) {
                    for (int x = 80; x < 116; x++) {
                        fb[(size_t)y * width + (margin + x)] = 0xFF00108Cu;
                    }
                }
                int len = (int)strlen(p2_name);
                int advance = 5;
                if (len * advance > 36) len = 36 / advance;
                int sx = margin + 98 - (len * advance) / 2;
                draw_outlined_text(fb, width, height, sx, 24, p2_name, advance,
                                   0xFFFFFFFFu, 0xFF000000u);
            }
        }
    }
}
