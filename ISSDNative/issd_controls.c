#include "issd_controls.h"
#include "issd_menu.h"
#include "issd_touch.h"
#include <stdio.h>
#include <string.h>

static int player, control, capture = -1;
static bool keyboard_capture, capture_armed;
static uint64_t previous;
static const char *(*key_label)(int);
void issd_controls_set_key_label(const char *(*label)(int)) { key_label = label; }
static const char *sources[21] = { "A", "B", "X", "Y", "Back", "Guide", "Start",
    "Left stick", "Right stick", "LB", "RB", "D-pad Up", "D-pad Down",
    "D-pad Left", "D-pad Right", "Misc", "Paddle 1", "Paddle 2", "Paddle 3", "Paddle 4", "Touchpad" };
static const char *axes[6] = { "Stick Up", "Stick Down", "Stick Left", "Stick Right", "LT", "RT" };
static const char *actions[12] = { "B / Pass", "Y / Dash", "Select", "Start",
    "Up", "Down", "Left", "Right", "A / Shoot", "X / Through", "L", "R" };
static const char *touch_names[11] = { "D-pad", "A", "B", "X", "Y", "L", "R", "Start", "Select", "Hide", "Menu" };
static const char *presets[4] = { "CLASSIC", "FIFA", "PES", "CUSTOM" };
static int *keyboard_keys[12];

static void keys(void) {
    int *values[12] = { &g_issd_config.key_p1_b, &g_issd_config.key_p1_y,
        &g_issd_config.key_p1_select, &g_issd_config.key_p1_start,
        &g_issd_config.key_p1_up, &g_issd_config.key_p1_down,
        &g_issd_config.key_p1_left, &g_issd_config.key_p1_right,
        &g_issd_config.key_p1_a, &g_issd_config.key_p1_x,
        &g_issd_config.key_p1_l, &g_issd_config.key_p1_r };
    memcpy(keyboard_keys, values, sizeof values);
}
static int bounded(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static void save(void) {
    if (!issd_config_save(&g_issd_config, NULL)) issd_menu_notify("Settings could not be saved", 240);
}
static void layout(void) {
    issd_touch_set_layout(g_issd_config.touch_x, g_issd_config.touch_y, g_issd_config.touch_size);
    save();
}
bool issd_controls_active(void) {
    return g_overlay_menu.page >= ISSD_MENU_PAGE_CONTROLS && g_overlay_menu.page <= ISSD_MENU_PAGE_TOUCH;
}
void issd_controls_end_capture(void) { capture = -1; capture_armed = false; }
bool issd_menu_binding_capture(void) { return g_overlay_menu.is_open && issd_controls_active() && capture >= 0; }
void issd_controls_open(void) {
    issd_controls_end_capture();
    g_overlay_menu.page = ISSD_MENU_PAGE_CONTROLS;
    g_overlay_menu.current_item = g_overlay_menu.scroll = 0;
}
static int rows(void) { return g_overlay_menu.page == ISSD_MENU_PAGE_CONTROLS ? 20 : g_overlay_menu.page == ISSD_MENU_PAGE_KEYBOARD ? 14 : 7; }
void issd_controls_step(int direction) {
    if (issd_menu_binding_capture()) return;
    int count = rows();
    int row = (g_overlay_menu.current_item + direction + count) % count;
    g_overlay_menu.current_item = row;
    if (row < g_overlay_menu.scroll) g_overlay_menu.scroll = row;
    if (row >= g_overlay_menu.scroll + 13) g_overlay_menu.scroll = row - 12;
}
void issd_controls_cancel(void) {
    if (issd_menu_binding_capture()) { issd_controls_end_capture(); return; }
    if (g_overlay_menu.page == ISSD_MENU_PAGE_CONTROLS) {
        g_overlay_menu.page = ISSD_MENU_PAGE_MAIN;
        g_overlay_menu.current_item = 1;
    } else {
        g_overlay_menu.current_item = g_overlay_menu.page == ISSD_MENU_PAGE_KEYBOARD ? 16 : 17;
        g_overlay_menu.page = ISSD_MENU_PAGE_CONTROLS;
    }
    g_overlay_menu.scroll = g_overlay_menu.current_item >= 13 ? g_overlay_menu.current_item - 12 : 0;
}
void issd_controls_adjust(int direction) {
    if (issd_menu_binding_capture()) return;
    int row = g_overlay_menu.current_item;
    IssdPlayerProfile *profile = &g_issd_config.player_profiles[player];
    if (g_overlay_menu.page == ISSD_MENU_PAGE_CONTROLS) {
        if (row == 0) player = (player + direction + 4) % 4;
        else if (row == 1) {
            int schema = profile->schema == 3 ? 0 : (profile->schema + direction + 3) % 3;
            issd_config_player_preset(&g_issd_config, player, schema);
            if (player == 0) g_overlay_menu.control_schema = (IssdControlSchema)schema;
        } else if (row == 2) profile->stick_deadzone = bounded(profile->stick_deadzone + direction * 1000, 0, 30000);
        else if (row == 3) profile->trigger_deadzone = bounded(profile->trigger_deadzone + direction * 1000, 0, 30000);
        save();
    } else if (g_overlay_menu.page == ISSD_MENU_PAGE_TOUCH) {
        if (row == 0) control = (control + direction + 11) % 11;
        else if (row == 1 || row == 2) {
            int *position = row == 1 ? &g_issd_config.touch_x[control] : &g_issd_config.touch_y[control];
            if (*position < 0) {
                int cx = 500, cy = 500;
                issd_touch_center(control, &cx, &cy);
                *position = row == 1 ? cx : cy;
            }
            *position = bounded(*position + direction * 25, 0, 1000);
        } else if (row == 3) g_issd_config.touch_size[control] = bounded(g_issd_config.touch_size[control] + direction * 5, 50, 200);
        layout();
    }
}
void issd_controls_confirm(void) {
    if (issd_menu_binding_capture()) return;
    int row = g_overlay_menu.current_item;
    if (g_overlay_menu.page == ISSD_MENU_PAGE_CONTROLS) {
        if (row <= 3) { issd_controls_adjust(1); return; }
        if (row >= 4 && row <= 15) { capture = row - 4; keyboard_capture = false; capture_armed = false; return; }
        if (row == 16 || row == 17) {
            g_overlay_menu.page = row == 16 ? ISSD_MENU_PAGE_KEYBOARD : ISSD_MENU_PAGE_TOUCH;
            g_overlay_menu.current_item = g_overlay_menu.scroll = 0; return;
        }
        if (row == 18) {
            issd_config_player_preset(&g_issd_config, player, 0);
            g_issd_config.player_profiles[player].stick_deadzone = 12000;
            g_issd_config.player_profiles[player].trigger_deadzone = 12000;
            if (player == 0) g_overlay_menu.control_schema = ISSD_SCHEMA_CLASSIC;
            save(); return;
        }
    } else if (g_overlay_menu.page == ISSD_MENU_PAGE_KEYBOARD) {
        if (row < 12) { capture = row; keyboard_capture = true; return; }
        if (row == 12) {
            const int defaults[12] = {29,6,44,40,26,22,4,7,27,25,20,8};
            keys(); for (int i = 0; i < 12; i++) *keyboard_keys[i] = defaults[i];
            save(); return;
        }
    } else if (g_overlay_menu.page == ISSD_MENU_PAGE_TOUCH) {
        if (row <= 3) { issd_controls_adjust(1); return; }
        if (row == 4 || row == 5) {
            for (int i = 0; i < 11; i++) if (row == 5 || i == control) {
                g_issd_config.touch_x[i] = g_issd_config.touch_y[i] = -1;
                g_issd_config.touch_size[i] = 100;
            }
            layout(); return;
        }
    }
    issd_controls_cancel();
}
bool issd_menu_capture_key(int scancode) {
    if (!issd_menu_binding_capture() || !keyboard_capture) return false;
    if (scancode == 41) { issd_controls_end_capture(); return true; } /* Escape */
    /* Fixed host shortcuts cannot be remapped into unreachable gameplay keys. */
    if (scancode <= 0 || scancode >= 512 || (scancode >= 58 && scancode <= 68) ||
        (scancode >= 30 && scancode <= 37) || scancode == 43) return true;
    keys(); *keyboard_keys[capture] = scancode;
    issd_controls_end_capture(); save(); return true;
}
void issd_menu_capture_pad(int slot, uint64_t raw) {
    if (!issd_menu_binding_capture() || keyboard_capture || slot != player) return;
    if (!capture_armed) { previous = raw; capture_armed = true; return; }
    uint64_t pressed = raw & ~previous;
    previous = raw;
    pressed &= ((UINT64_C(1) << 21) - 1) | (UINT64_C(63) << 32);
    pressed &= ~(UINT64_C(1) << 5); /* Guide always reaches the overlay. */
    if (!pressed) return;
    uint64_t first = pressed & (~pressed + 1);
    g_issd_config.player_profiles[player].bindings[capture] = first;
    g_issd_config.player_profiles[player].schema = 3;
    issd_controls_end_capture(); save();
}
void issd_controls_render(uint32_t *fb, int w, int h, int x, int y,
    void (*text)(uint32_t *, int, int, int, int, const char *, uint32_t)) {
    const char *title = g_overlay_menu.page == ISSD_MENU_PAGE_CONTROLS ? "PLAYER CONTROLS" :
        g_overlay_menu.page == ISSD_MENU_PAGE_KEYBOARD ? "P1 KEYBOARD" : "TOUCH LAYOUT";
    text(fb,w,h,x+16,y+4,title,0xffffd700);
    IssdPlayerProfile *p = &g_issd_config.player_profiles[player];
    keys();
    for (int row = g_overlay_menu.scroll; row < rows() && row < g_overlay_menu.scroll + 13; row++) {
        char line[29] = "";
        if (g_overlay_menu.page == ISSD_MENU_PAGE_CONTROLS) {
            if (row == 0) snprintf(line,sizeof line,"Player: P%d",player+1);
            else if (row == 1) snprintf(line,sizeof line,"Preset: %s",presets[bounded(p->schema,0,3)]);
            else if (row == 2) snprintf(line,sizeof line,"Stick deadzone: %d%%",p->stick_deadzone*100/32767);
            else if (row == 3) snprintf(line,sizeof line,"Trigger zone: %d%%",p->trigger_deadzone*100/32767);
            else if (row < 16) {
                uint64_t b = p->bindings[row-4]; int source=0;
                while (source<64 && !(b & (UINT64_C(1)<<source))) source++;
                const char *label = source < 21 ? sources[source] : source >= 32 && source < 38 ? axes[source-32] : "None";
                snprintf(line,sizeof line,"%s: %s%s",actions[row-4],label,
                    b && (b & (b-1)) ? "+" : "");
            } else snprintf(line,sizeof line,"%s",row==16?"P1 Keyboard...":row==17?"Touch Layout...":row==18?"Reset this player":"Back");
        } else if (g_overlay_menu.page == ISSD_MENU_PAGE_KEYBOARD) {
            if (row < 12) {
                int scan = *keyboard_keys[row];
                const char *label = key_label && scan > 0 && scan < 512 ? key_label(scan) : NULL;
                if (label && label[0]) snprintf(line,sizeof line,"%s: %.14s",actions[row],label);
                else snprintf(line,sizeof line,"%s: key %d",actions[row],scan);
            }
            else snprintf(line,sizeof line,"%s",row==12?"Reset keyboard":"Back");
        } else {
            if (row==0) snprintf(line,sizeof line,"Control: %s",touch_names[control]);
            else if (row==1) snprintf(line,sizeof line,"Horizontal: %d",g_issd_config.touch_x[control]);
            else if (row==2) snprintf(line,sizeof line,"Vertical: %d",g_issd_config.touch_y[control]);
            else if (row==3) snprintf(line,sizeof line,"Size: %d%%",g_issd_config.touch_size[control]);
            else snprintf(line,sizeof line,"%s",row==4?"Reset this control":row==5?"Reset all touch":"Back");
        }
        bool selected = row == g_overlay_menu.current_item;
        text(fb,w,h,x+14,y+22+(row-g_overlay_menu.scroll)*12,line,selected?0xff00ff66:0xffe0e0e0);
        if(selected) text(fb,w,h,x+4,y+22+(row-g_overlay_menu.scroll)*12,">",0xff00ff66);
    }
    text(fb,w,h,x+8,y+190,issd_menu_binding_capture() ? "Press input / ESC cancels" : "A: Set  < >: Adjust",0xffb0b0b0);
    text(fb,w,h,x+8,y+205,g_overlay_menu.page==ISSD_MENU_PAGE_TOUCH?"-1=Auto; X/Y 0..1000":"Saved by player slot P1-P4",0xff888888);
}
