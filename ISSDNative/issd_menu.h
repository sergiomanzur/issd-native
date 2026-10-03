#ifndef ISSD_MENU_H
#define ISSD_MENU_H

#include <stdint.h>
#include <stdbool.h>
#include "issd_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ISSD_SCHEMA_CLASSIC = 0,   /* SNES ISSD: B=Pass, A=Shoot, Y=Dash, X=Through */
    ISSD_SCHEMA_FIFA = 1,      /* Modern FIFA: A=Pass, B=Shoot, X=Cross, Y=Through, RT/RB=Sprint */
    ISSD_SCHEMA_PES = 2        /* Modern PES: A=Pass, X=Shoot, B=Cross, Y=Through, RB=Sprint */
} IssdControlSchema;

/* The menu has two pages. Mods needs a list of its own: one row per pack,
 * more of them than fit, and a cursor that has to skip headers. */
typedef enum {
    ISSD_MENU_PAGE_MAIN = 0,
    ISSD_MENU_PAGE_MODS = 1,
    ISSD_MENU_PAGE_PASSWORD = 2,
    ISSD_MENU_PAGE_GAMEPLAY = 3,
    ISSD_MENU_PAGE_CONTROLS = 4,
    ISSD_MENU_PAGE_KEYBOARD = 5,
    ISSD_MENU_PAGE_TOUCH = 6,
    ISSD_MENU_PAGE_MATCH = 7,
    ISSD_MENU_PAGE_GRAPHICS = 8,
    ISSD_MENU_PAGE_GRAPHICS_PREVIEW = 9
} IssdMenuPage;

typedef struct {
    bool is_open;
    IssdMenuPage page;
    int scroll;               /* first visible row on the mods page */
    int current_item;
    int current_slot;
    IssdControlSchema control_schema;
    char status_message[64];
    int status_timer;
} IssdOverlayMenu;

extern IssdOverlayMenu g_overlay_menu;

/* 8x8 Basic ASCII font (32-127) bitmap table */
extern const uint8_t g_issd_font8x8[96][8];

void issd_menu_init(void);
void issd_menu_toggle(void);
void issd_menu_open(void);
/* Open at Continue only when a compatible checkpoint exists; never auto-load. */
void issd_menu_offer_continue(void);
void issd_menu_refresh_continue(void);
void issd_menu_set_save_context_callback(void (*callback)(void));
void issd_menu_set_input_reset_callback(void (*callback)(void));
void issd_menu_close(void);
bool issd_menu_is_open(void);

/* Internal intermediates apply for CRT and sharp scaling. */
bool issd_menu_internal_res_applies(void);

/* Navigation: returns true if handled */
bool issd_menu_navigate_up(void);
bool issd_menu_navigate_down(void);
bool issd_menu_navigate_left(void);
bool issd_menu_navigate_right(void);
bool issd_menu_confirm(void);
bool issd_menu_cancel(void);
bool issd_menu_handle_click(int fb_x, int fb_y, int width, int height);
bool issd_menu_binding_capture(void);
bool issd_menu_capture_key(int scancode);
void issd_menu_capture_pad(int player, uint64_t raw);

/* Render overlay on top of 256x224 32-bit ARGB framebuffer */
void issd_menu_render(uint32_t *framebuffer, int width, int height);

/* Transparent output-resolution overlay. Integer glyph scaling is independent
 * of the game filter; click coordinates use the identical centered transform. */
void issd_menu_render_display(uint32_t *argb, int width, int height);
bool issd_menu_handle_display_click(int x, int y, int width, int height);
void issd_menu_set_display_metrics(int output_width, int output_height,
                                    int native_width, int native_height);
/* Dimensions of the actual intermediate selected for this output. */
void issd_menu_set_intermediate_metrics(int width, int height);
/* Physical output pixels reserved by display cutouts/system UI. */
void issd_menu_set_safe_insets(int left, int top, int right, int bottom);

/* A short line shown over the game for `frames` frames, whether or not the
 * menu is open. Mods apply during a restart, when nobody is looking at a
 * console, so the outcome has to arrive on screen. */
void issd_menu_notify(const char *message, int frames);
void issd_menu_render_notification(uint32_t *framebuffer, int width, int height);
/* Paint after render_display, retaining transparent pixels around the notice. */
void issd_menu_render_notification_display(uint32_t *argb, int width, int height);
/* Output render APIs never advance timers. Call once per 60 Hz tick, also
 * while paused; legacy native render APIs retain their per-call timing. */
void issd_menu_tick_notification(void);
bool issd_menu_has_notification(void);

/* Repaint the stadium select screen's name plate from the mod data.
 * `margin` is the widescreen column count before the authentic 256. */
void issd_menu_render_stadium_plate(uint32_t *framebuffer, int width,
                                    int height, int margin);

/* Repaints the team select screen's name plate when a pack renamed the
 * highlighted team. A no-op on every other screen. */
void issd_menu_render_team_plate(uint32_t *fb, int width, int height,
                                 int margin);

/* Draws a pack's own squad photograph over the select screen's frame.
 * A no-op when the highlighted team has none. */
void issd_menu_render_team_photo(uint32_t *fb, int width, int height,
                                 int margin);

/* Labels the six cells of the select screen's grid for any team a pack
 * renamed, so the grid agrees with the plate above it. */
void issd_menu_render_team_grid(uint32_t *fb, int width, int height,
                                int margin);

/* Renders custom team flags over the select screen (top panel and 6-cell grid)
 * and in-match HUD for any team a pack provides a flag for. */
void issd_menu_render_team_flags(uint32_t *fb, int width, int height,
                                 int margin);

/* Sprite identity replacements clip to the cartridge viewport inside wide views. */
bool issd_menu_team_flag_available(int team_id);
void issd_menu_draw_team_identity(uint32_t *fb,int width,int height,int margin,
                                  int team,int x,int y,int room,int tall,bool flag);

/* Provided by the host: save settings and start the process again, so a
 * newly chosen mod pack is applied to a fresh cartridge image. */
void issd_restart_application(void);
void issd_request_quit(void);

#ifdef __cplusplus
}
#endif

#endif /* ISSD_MENU_H */
