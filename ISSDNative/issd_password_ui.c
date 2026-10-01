#include "issd_password_ui.h"
#include <stdio.h>
#include <string.h>

enum {
    GRID_KEYS = 64, DELETE_KEY = 64, IMPORT_KEY, EXPORT_KEY, BACK_KEY,
    BOX_WIDTH = 240, BOX_HEIGHT = 220, GRID_LEFT = 24, GRID_TOP = 80,
    CELL_WIDTH = 24, CELL_HEIGHT = 12, ACTION_LEFT = 8, ACTION_TOP = 181,
    ACTION_WIDTH = 56, ACTION_HEIGHT = 12
};
static IssdPasswordUiCallbacks s_callbacks;
static uint8_t s_symbols[ISSD_PASSWORD_UI_CAPACITY];
static size_t s_count;
static unsigned s_selected;
static char s_message[96];

static void message(const char *text) {
    snprintf(s_message, sizeof(s_message), "%s", text ? text : "");
}
static void callback_failure(const char *fallback) {
    const char *error = s_callbacks.error ? s_callbacks.error() : NULL;
    message(error && error[0] ? error : fallback);
}

void issd_password_ui_set_callbacks(const IssdPasswordUiCallbacks *callbacks) {
    if (callbacks) s_callbacks = *callbacks;
    else memset(&s_callbacks, 0, sizeof(s_callbacks));
}
void issd_password_ui_open(void) {
    memset(s_symbols, 0, sizeof(s_symbols));
    s_count = s_selected = 0;
    message("Choose keys or type a password.");
}
const uint8_t *issd_password_ui_symbols(size_t *count) {
    if (count) *count = s_count;
    return s_symbols;
}
const char *issd_password_ui_message(void) { return s_message; }

void issd_password_ui_up(void) {
    if (s_selected >= GRID_KEYS) s_selected = 56 + 2 * (s_selected - GRID_KEYS);
    else if (s_selected < 8) s_selected = GRID_KEYS + s_selected / 2;
    else s_selected -= 8;
}
void issd_password_ui_down(void) {
    if (s_selected >= GRID_KEYS) s_selected = 2 * (s_selected - GRID_KEYS);
    else if (s_selected >= 56) s_selected = GRID_KEYS + (s_selected % 8) / 2;
    else s_selected += 8;
}
void issd_password_ui_left(void) {
    if (s_selected >= GRID_KEYS) s_selected = GRID_KEYS + (s_selected - GRID_KEYS + 3) % 4;
    else s_selected = (s_selected / 8) * 8 + (s_selected + 7) % 8;
}
void issd_password_ui_right(void) {
    if (s_selected >= GRID_KEYS) s_selected = GRID_KEYS + (s_selected - GRID_KEYS + 1) % 4;
    else s_selected = (s_selected / 8) * 8 + (s_selected + 1) % 8;
}
bool issd_password_ui_backspace(void) {
    if (!s_count) return false;
    s_symbols[--s_count] = 0;
    message("");
    return true;
}

IssdPasswordUiResult issd_password_ui_confirm(void) {
    if (s_selected < GRID_KEYS) {
        if (s_count == sizeof(s_symbols)) message("Password is full (60 keys).");
        else { s_symbols[s_count++] = (uint8_t)s_selected; message(""); }
    } else if (s_selected == DELETE_KEY) {
        if (!issd_password_ui_backspace()) message("Password is empty.");
    } else if (s_selected == IMPORT_KEY) {
        if (!s_count) message("Enter a password first.");
        else if (!s_callbacks.import_symbols) message("Password import is unavailable.");
        else if (!s_callbacks.import_symbols(s_symbols, s_count)) callback_failure("Password import failed.");
        else return ISSD_PASSWORD_UI_IMPORTED;
    } else if (s_selected == EXPORT_KEY) {
        uint8_t exported[ISSD_PASSWORD_UI_CAPACITY] = {0};
        size_t count = sizeof(exported);
        if (!s_callbacks.export_symbols) message("Password export is unavailable.");
        else if (!s_callbacks.export_symbols(exported, &count)) callback_failure("Password export failed.");
        else {
            bool valid = count > 0 && count <= sizeof(exported);
            if (valid) for (size_t i = 0; i < count; ++i) if (exported[i] >= GRID_KEYS) valid = false;
            if (!valid) message("Export returned invalid password keys.");
            else {
                memcpy(s_symbols, exported, count);
                memset(s_symbols + count, 0, sizeof(s_symbols) - count);
                s_count = count;
                message("Password exported. Record these keys.");
            }
        }
    } else if (s_selected == BACK_KEY) return ISSD_PASSWORD_UI_BACK;
    return ISSD_PASSWORD_UI_STAY;
}
IssdPasswordUiResult issd_password_ui_cancel(void) { return ISSD_PASSWORD_UI_BACK; }

static bool whitespace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
bool issd_password_ui_text(const char *text) {
    if (!text) return false;
    uint8_t additions[ISSD_PASSWORD_UI_CAPACITY];
    size_t count = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (whitespace(*p)) continue;
        int symbol = -1;
        if (s_callbacks.symbol_ascii && *p >= 32 && *p < 127) {
            for (unsigned i = 0; i < GRID_KEYS; ++i) {
                if ((unsigned char)s_callbacks.symbol_ascii((uint8_t)i) == *p) {
                    symbol = (int)i;
                    break;
                }
            }
        }
        if (symbol < 0) { message("Unknown key. Choose it from the palette."); return false; }
        if (count >= sizeof(s_symbols) - s_count) { message("Password is full (60 keys)."); return false; }
        additions[count++] = (uint8_t)symbol;
    }
    if (count) { memcpy(s_symbols + s_count, additions, count); s_count += count; message(""); }
    return true;
}

static void box_origin(int width, int height, int *x, int *y) {
    *x = width > BOX_WIDTH ? (width - BOX_WIDTH) / 2 : 0;
    *y = height > BOX_HEIGHT ? (height - BOX_HEIGHT) / 2 : 0;
}
IssdPasswordUiResult issd_password_ui_click(int x, int y, int width, int height) {
    if (width <= 0 || height <= 0) return ISSD_PASSWORD_UI_STAY;
    int bx, by;
    box_origin(width, height, &bx, &by);
    int dx = x - bx, dy = y - by;
    if (dx < 0 || dx >= BOX_WIDTH || dy < 0 || dy >= BOX_HEIGHT) return ISSD_PASSWORD_UI_BACK;
    if (dx >= GRID_LEFT && dx < GRID_LEFT + 8 * CELL_WIDTH &&
        dy >= GRID_TOP && dy < GRID_TOP + 8 * CELL_HEIGHT) {
        s_selected = (unsigned)((dy - GRID_TOP) / CELL_HEIGHT * 8 + (dx - GRID_LEFT) / CELL_WIDTH);
        return issd_password_ui_confirm();
    }
    if (dx >= ACTION_LEFT && dx < ACTION_LEFT + 4 * ACTION_WIDTH &&
        dy >= ACTION_TOP && dy < ACTION_TOP + ACTION_HEIGHT) {
        s_selected = GRID_KEYS + (unsigned)((dx - ACTION_LEFT) / ACTION_WIDTH);
        return issd_password_ui_confirm();
    }
    return ISSD_PASSWORD_UI_STAY;
}

static void rectangle(uint32_t *fb, int width, int height,
                      int x, int y, int w, int h, uint32_t color) {
    for (int row = y < 0 ? 0 : y; row < y + h && row < height; ++row)
        for (int col = x < 0 ? 0 : x; col < x + w && col < width; ++col)
            fb[(size_t)row * (size_t)width + (size_t)col] = color;
}
static void bitmap(uint32_t *fb, int width, int height, int x, int y,
                   const uint8_t glyph[8], uint32_t color) {
    if (!glyph) return;
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
            if ((glyph[row] & (0x80u >> col)) && x + col >= 0 && x + col < width && y + row >= 0 && y + row < height)
                fb[(size_t)(y + row) * (size_t)width + (size_t)(x + col)] = color;
}
static void character(uint32_t *fb, int width, int height, int x, int y,
                      unsigned char c, const uint8_t font[96][8], uint32_t color) {
    if (font) bitmap(fb, width, height, x, y, font[(c >= 32 && c < 128 ? c : '?') - 32], color);
}
static void string(uint32_t *fb, int width, int height, int x, int y,
                   const char *text, const uint8_t font[96][8], uint32_t color) {
    if (!text) return;
    for (size_t i = 0; text[i] && i < 28; ++i)
        character(fb, width, height, x + (int)i * 8, y, (unsigned char)text[i], font, color);
}

/* Semantic fallback icons use callback names, never guessed cartridge indices.
 * An exact cartridge bitmap supplied by the host takes precedence. */
static const uint8_t *semantic_icon(const char *name) {
    static const struct { const char *name; uint8_t rows[8]; } icons[] = {
        {"DIV",     {0, 0x18, 0, 0x7e, 0, 0x18, 0, 0}},
        {"PI",      {0, 0x7e, 0x24, 0x24, 0x24, 0x24, 0x46, 0}},
        {"DOWN",    {0x18, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0}},
        {"UP",      {0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x18, 0}},
        {"NOTE",    {0x0e, 0x0a, 0x0a, 0x0a, 0x0a, 0x3a, 0x36, 0}},
        {"STAR",    {0x10, 0x38, 0xfe, 0x7c, 0x38, 0x6c, 0x44, 0}},
        {"SPADE",   {0x18, 0x3c, 0x7e, 0xff, 0x7e, 0x18, 0x3c, 0}},
        {"DIAMOND", {0x18, 0x3c, 0x7e, 0xff, 0x7e, 0x3c, 0x18, 0}},
        {"CLUB",    {0x18, 0x3c, 0x18, 0x7e, 0xff, 0x7e, 0x18, 0}},
        {"HEART",   {0x66, 0xff, 0xff, 0xff, 0x7e, 0x3c, 0x18, 0}}
    };
    if (name && (!strcmp(name, "/") || !strcmp(name, "DIVISION"))) name = "DIV";
    if (name) for (size_t i = 0; i < sizeof(icons) / sizeof(icons[0]); ++i)
        if (!strcmp(name, icons[i].name)) return icons[i].rows;
    return NULL;
}
static void symbol(uint32_t *fb, int width, int height, int x, int y,
                   uint8_t key, const uint8_t font[96][8], uint32_t color) {
    const uint8_t *glyph = s_callbacks.symbol_glyph ? s_callbacks.symbol_glyph(key) : NULL;
    const char *name = s_callbacks.symbol_name ? s_callbacks.symbol_name(key) : NULL;
    if (!glyph) glyph = semantic_icon(name);
    if (glyph) bitmap(fb, width, height, x, y, glyph, color);
    else {
        unsigned char c = s_callbacks.symbol_ascii ? (unsigned char)s_callbacks.symbol_ascii(key) : 0;
        character(fb, width, height, x, y, c >= 32 && c < 127 ? c : '?', font, color);
    }
}
void issd_password_ui_render(uint32_t *fb, int width, int height, const uint8_t font[96][8]) {
    if (!fb || width <= 0 || height <= 0) return;
    int bx, by;
    box_origin(width, height, &bx, &by);
    rectangle(fb, width, height, bx, by, BOX_WIDTH, BOX_HEIGHT, 0xff00e5ff);
    rectangle(fb, width, height, bx + 1, by + 1, BOX_WIDTH - 2, BOX_HEIGHT - 2, 0xff002244);
    string(fb, width, height, bx + 56, by + 4, "PASSWORD BRIDGE", font, 0xffffd700);
    string(fb, width, height, bx + 8, by + 16, "Cartridge keys", font, 0xffb0b0b0);
    char counter[16]; snprintf(counter, sizeof(counter), "%zu/60", s_count);
    string(fb, width, height, bx + 176, by + 16, counter, font, 0xffb0b0b0);
    rectangle(fb, width, height, bx + 8, by + 28, 224, 35, 0xff001422);
    for (size_t i = 0; i < s_count; ++i)
        symbol(fb, width, height, bx + 12 + (int)(i % 20) * 10, by + 30 + (int)(i / 20) * 11,
               s_symbols[i], font, 0xffe0e0e0);
    if (s_count < sizeof(s_symbols))
        character(fb, width, height, bx + 12 + (int)(s_count % 20) * 10,
                  by + 30 + (int)(s_count / 20) * 11, '_', font, 0xff00ff66);
    const char *key_name = s_selected < GRID_KEYS && s_callbacks.symbol_name ? s_callbacks.symbol_name((uint8_t)s_selected) : NULL;
    char selected[64];
    if (s_selected < GRID_KEYS) snprintf(selected, sizeof(selected), "Key: %s", key_name && key_name[0] ? key_name : "Choose from palette");
    else snprintf(selected, sizeof(selected), "%s", "A: Select   B/ESC: Back");
    string(fb, width, height, bx + 8, by + 67, selected, font, 0xffb0b0b0);
    for (unsigned i = 0; i < GRID_KEYS; ++i) {
        int x = bx + GRID_LEFT + (int)(i % 8) * CELL_WIDTH;
        int y = by + GRID_TOP + (int)(i / 8) * CELL_HEIGHT;
        if (i == s_selected) rectangle(fb, width, height, x, y, CELL_WIDTH, CELL_HEIGHT, 0xff145544);
        symbol(fb, width, height, x + 8, y + 2, (uint8_t)i, font, i == s_selected ? 0xff00ff66 : 0xffe0e0e0);
    }
    static const char *actions[] = {"Delete", "Import", "Export", "Back"};
    for (unsigned i = 0; i < 4; ++i) {
        int x = bx + ACTION_LEFT + (int)i * ACTION_WIDTH;
        if (s_selected == GRID_KEYS + i) rectangle(fb, width, height, x, by + ACTION_TOP, ACTION_WIDTH, ACTION_HEIGHT, 0xff145544);
        string(fb, width, height, x + 2, by + ACTION_TOP + 2, actions[i], font,
               s_selected == GRID_KEYS + i ? 0xff00ff66 : 0xffe0e0e0);
    }
    if (s_message[0]) {
        char first[29], second[29];
        snprintf(first, sizeof(first), "%.28s", s_message);
        snprintf(second, sizeof(second), "%.28s", strlen(s_message) > 28 ? s_message + 28 : "");
        string(fb, width, height, bx + 8, by + 198, first, font, 0xffffaa00);
        string(fb, width, height, bx + 8, by + 209, second, font, 0xffffaa00);
    } else string(fb, width, height, bx + 8, by + 209, "A: Key   B/ESC: Back", font, 0xff888888);
}
