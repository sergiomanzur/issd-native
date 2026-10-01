#ifndef ISSD_PASSWORD_UI_H
#define ISSD_PASSWORD_UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { ISSD_PASSWORD_UI_CAPACITY = 60 };
typedef enum {
    ISSD_PASSWORD_UI_STAY = 0,
    ISSD_PASSWORD_UI_BACK = 1,
    ISSD_PASSWORD_UI_IMPORTED = 2
} IssdPasswordUiResult;

/* The UI contains only cartridge symbol indices 0..63. Callbacks own all
 * compatibility, codec, and original guest-screen validation. Import success
 * means the host queued the original submit flow and should close the overlay.
 * On export entry, *count is the output capacity; on success it is the length.
 * Symbol ASCII returns 0 for keys requiring the palette. Glyph rows are 8-bit
 * bitmaps with bit 7 leftmost; a NULL glyph uses ASCII or a semantic icon.
 * Names identify exact cartridge keys, including icons, in the selected-key
 * footer. Callback tables are copied; returned glyph/name storage is borrowed. */
typedef struct {
    bool (*import_symbols)(const uint8_t *symbols, size_t count);
    bool (*export_symbols)(uint8_t *symbols, size_t *count);
    const char *(*error)(void);
    char (*symbol_ascii)(uint8_t symbol);
    const uint8_t *(*symbol_glyph)(uint8_t symbol);
    const char *(*symbol_name)(uint8_t symbol);
} IssdPasswordUiCallbacks;

void issd_password_ui_set_callbacks(const IssdPasswordUiCallbacks *callbacks);
void issd_password_ui_open(void);
void issd_password_ui_up(void);
void issd_password_ui_down(void);
void issd_password_ui_left(void);
void issd_password_ui_right(void);
IssdPasswordUiResult issd_password_ui_confirm(void);
IssdPasswordUiResult issd_password_ui_cancel(void);
IssdPasswordUiResult issd_password_ui_click(int x, int y, int width, int height);

/* Text input preserves case and ignores ASCII whitespace grouping. Unknown
 * keys or overflow reject the entire input event without modifying the field.
 * Backspace deletes the last symbol. Both functions report whether they edited
 * or accepted input, and keep failures visible in the page footer. */
bool issd_password_ui_text(const char *text);
bool issd_password_ui_backspace(void);
void issd_password_ui_render(uint32_t *framebuffer, int width, int height,
                             const uint8_t font[96][8]);
const uint8_t *issd_password_ui_symbols(size_t *count);
const char *issd_password_ui_message(void);

#ifdef __cplusplus
}
#endif
#endif
