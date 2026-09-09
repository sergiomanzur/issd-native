#include "issd_menu.h"
#include "issd_config.h"
#include "issd_save.h"
#include "issd_mod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint8_t g_ram[0x20000];

IssdOverlayMenu g_overlay_menu;

/* 8x8 Basic ASCII font (32-127) bitmap table */
static const uint8_t s_font8x8[96][8] = {
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

#define MENU_TOTAL_ITEMS 15

static void DrawChar(uint32_t *fb, int fb_w, int fb_h, int x, int y, char c, uint32_t color) {
    if (c < 32 || c > 126) c = ' ';
    const uint8_t *glyph = s_font8x8[c - 32];
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
}

void issd_menu_toggle(void) {
    g_overlay_menu.is_open = !g_overlay_menu.is_open;
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

bool issd_menu_navigate_up(void) {
    if (!g_overlay_menu.is_open) return false;
    g_overlay_menu.current_item = (g_overlay_menu.current_item - 1 + MENU_TOTAL_ITEMS) % MENU_TOTAL_ITEMS;
    return true;
}

bool issd_menu_navigate_down(void) {
    if (!g_overlay_menu.is_open) return false;
    g_overlay_menu.current_item = (g_overlay_menu.current_item + 1) % MENU_TOTAL_ITEMS;
    return true;
}

static const int s_fps_presets[] = { 60, 120, 144, 165, 240, 0 };
#define TOTAL_FPS_PRESETS 6

bool issd_menu_navigate_left(void) {
    if (!g_overlay_menu.is_open) return false;
    switch (g_overlay_menu.current_item) {
        case 1: /* Schema */
            g_overlay_menu.control_schema = (IssdControlSchema)((g_overlay_menu.control_schema - 1 + 3) % 3);
            break;
        case 2: { /* Active Mod Pack */
            int pack_count = issd_mod_get_pack_count();
            if (pack_count > 0) {
                int cur_act = issd_mod_get_active_pack_index();
                /* Cycle from -1 (None) to pack_count - 1 */
                cur_act--;
                if (cur_act < -1) cur_act = pack_count - 1;
                issd_mod_set_active_pack(cur_act);
            }
            break;
        }
        case 3: /* Aspect Ratio */
            g_issd_config.aspect_ratio = (IssdAspectRatio)((g_issd_config.aspect_ratio - 1 + 6) % 6);
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
    switch (g_overlay_menu.current_item) {
        case 1: /* Schema */
            g_overlay_menu.control_schema = (IssdControlSchema)((g_overlay_menu.control_schema + 1) % 3);
            break;
        case 2: { /* Active Mod Pack */
            int pack_count = issd_mod_get_pack_count();
            if (pack_count > 0) {
                int cur_act = issd_mod_get_active_pack_index();
                cur_act++;
                if (cur_act >= pack_count) cur_act = -1;
                issd_mod_set_active_pack(cur_act);
            }
            break;
        }
        case 3: /* Aspect Ratio */
            g_issd_config.aspect_ratio = (IssdAspectRatio)((g_issd_config.aspect_ratio + 1) % 6);
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
    switch (g_overlay_menu.current_item) {
        case 0: /* Resume */
            issd_menu_close();
            break;
        case 1: /* Next Schema */
        case 2: /* Mod Pack */
        case 3: /* Aspect */
        case 4: /* True Widescreen */
        case 5: /* Res */
        case 6: /* Filter */
        case 7: /* FPS */
        case 8: /* VSync */
            issd_menu_navigate_right();
            break;
        case 9: /* Save */
            if (issd_save_to_slot(g_overlay_menu.current_slot, NULL)) {
                snprintf(g_overlay_menu.status_message, sizeof(g_overlay_menu.status_message),
                         "Saved Slot %d OK!", g_overlay_menu.current_slot + 1);
            } else {
                snprintf(g_overlay_menu.status_message, sizeof(g_overlay_menu.status_message), "Save Failed!");
            }
            g_overlay_menu.status_timer = 120;
            break;
        case 10: /* Load */
            if (issd_load_from_slot(g_overlay_menu.current_slot)) {
                snprintf(g_overlay_menu.status_message, sizeof(g_overlay_menu.status_message),
                         "Loaded Slot %d OK!", g_overlay_menu.current_slot + 1);
                issd_menu_close();
            } else {
                snprintf(g_overlay_menu.status_message, sizeof(g_overlay_menu.status_message), "Empty / Invalid Slot!");
            }
            g_overlay_menu.status_timer = 120;
            break;
        case 11: /* Volume */
        case 12: /* Engine Mode */
        case 13: /* Debug & JPN Mode */
            issd_menu_navigate_right();
            break;
        case 14: /* Save & Quit */
            issd_config_save(&g_issd_config, NULL);
            exit(0);
            break;
        default:
            break;
    }
    return true;
}

bool issd_menu_cancel(void) {
    if (!g_overlay_menu.is_open) return false;
    issd_menu_close();
    return true;
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

    /* 3. Title */
    DrawString(fb, width, height, box_x + 44, box_y + 4, "ISSD NATIVE MENU", 0xFFFFD700);

    /* Display Strings */
    const char *schema_str = (g_overlay_menu.control_schema == ISSD_SCHEMA_CLASSIC) ? "CLASSIC" :
                             (g_overlay_menu.control_schema == ISSD_SCHEMA_FIFA)    ? "FIFA" : "PES";

    /* Mod pack display */
    char mod_str[24];
    int active_mod = issd_mod_get_active_pack_index();
    if (active_mod >= 0) {
        IssdModPack *pack = issd_mod_get_pack(active_mod);
        if (pack) {
            snprintf(mod_str, sizeof(mod_str), "%.12s", pack->name);
        } else {
            snprintf(mod_str, sizeof(mod_str), "MOD #%d", active_mod + 1);
        }
    } else {
        snprintf(mod_str, sizeof(mod_str), "NONE (VANILLA)");
    }

    const char *aspect_str = (g_issd_config.aspect_ratio == ISSD_ASPECT_4_3)     ? "4:3 CRT" :
                             (g_issd_config.aspect_ratio == ISSD_ASPECT_8_7)     ? "8:7 PIXEL" :
                             (g_issd_config.aspect_ratio == ISSD_ASPECT_16_9)    ? "16:9 WIDE" :
                             (g_issd_config.aspect_ratio == ISSD_ASPECT_16_10)   ? "16:10 PC" :
                             (g_issd_config.aspect_ratio == ISSD_ASPECT_21_9)    ? "21:9 ULTRA" : "INTEGER";

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
    snprintf(items[2], sizeof(items[2]), "Mod Pack:   <%s>", mod_str);
    snprintf(items[3], sizeof(items[3]), "Aspect:     <%s>", aspect_str);
    snprintf(items[4], sizeof(items[4]), "Widescreen: <%s>", g_issd_config.true_widescreen ? "ON (TRUE FOV)" : "OFF (4:3 NATIVE)");
    snprintf(items[5], sizeof(items[5]), "Internal:   <%s>",
             issd_menu_internal_res_applies() ? res_str : "CRT FILTER ONLY");
    snprintf(items[6], sizeof(items[6]), "Filter:     <%s>", filter_str);
    snprintf(items[7], sizeof(items[7]), "Target FPS: <%s>", fps_str);
    snprintf(items[8], sizeof(items[8]), "VSync:      <%s>", g_issd_config.vsync ? "ON" : "OFF");
    snprintf(items[9], sizeof(items[9]), "Save Slot:  <Slot %d>", g_overlay_menu.current_slot + 1);
    snprintf(items[10], sizeof(items[10]), "Load Slot:  <Slot %d>", g_overlay_menu.current_slot + 1);
    snprintf(items[11], sizeof(items[11]), "Volume:     <%d%%>", g_issd_config.master_volume);
    snprintf(items[12], sizeof(items[12]), "Engine:     <%s>", mode_str);
    snprintf(items[13], sizeof(items[13]), "Debug/JPN:  <%s>", g_issd_config.debug_unhooked_code ? "ENABLED" : "DISABLED");
    snprintf(items[14], sizeof(items[14]), "Save & Quit to Desktop");

    int start_y = box_y + 14;
    for (int i = 0; i < MENU_TOTAL_ITEMS; i++) {
        uint32_t color = (i == g_overlay_menu.current_item) ? 0xFF00FF66 : 0xFFE0E0E0;
        int item_y = start_y + i * 12;
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
