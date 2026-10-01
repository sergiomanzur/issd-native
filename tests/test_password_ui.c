#include "issd_password_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned imported;
static uint8_t submitted[60];
static size_t submitted_count;
static bool accept_import, accept_export;
static unsigned export_kind;
static bool use_glyph;
static const char *first_name = "B";
static const uint8_t icon[8] = {0x80, 0x40, 0x20, 0x10, 8, 4, 2, 1};
static uint8_t font[96][8];
static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static bool import_symbols(const uint8_t *symbols, size_t count) {
    imported++;
    submitted_count = count;
    memcpy(submitted, symbols, count);
    return accept_import;
}
static bool export_symbols(uint8_t *symbols, size_t *count) {
    if (!accept_export) return false;
    if (export_kind == 1) { symbols[0] = 64; *count = 1; }
    else if (export_kind == 2) *count = 61;
    else { symbols[0] = 63; symbols[1] = 0; symbols[2] = 31; *count = 3; }
    return true;
}
static const char *error(void) { return "Original Password screen required"; }
static char ascii(uint8_t symbol) { return alphabet[symbol]; }
static const char *name(uint8_t symbol) { return symbol == 0 ? first_name : "Cartridge key"; }
static const uint8_t *glyph(uint8_t symbol) { return use_glyph && symbol == 0 ? icon : NULL; }
static const IssdPasswordUiCallbacks callbacks = {
    import_symbols, export_symbols, error, ascii, glyph, name
};

static void expect_symbols(const uint8_t *expected, size_t count) {
    size_t actual;
    const uint8_t *symbols = issd_password_ui_symbols(&actual);
    assert(actual == count);
    if (count) assert(memcmp(symbols, expected, count) == 0);
}
static void open_page(void) {
    issd_password_ui_set_callbacks(&callbacks);
    issd_password_ui_open();
}
static void select_action(unsigned action) {
    /* Palette row zero wraps upwards to Delete. */
    issd_password_ui_up();
    while (action--) issd_password_ui_right();
}

static void navigation(void) {
    open_page();
    issd_password_ui_left();
    assert(issd_password_ui_confirm() == ISSD_PASSWORD_UI_STAY);
    issd_password_ui_right();
    issd_password_ui_down();
    assert(issd_password_ui_confirm() == ISSD_PASSWORD_UI_STAY);
    const uint8_t expected[] = {7, 8};
    expect_symbols(expected, 2);
    for (unsigned i = 0; i < 6; ++i) issd_password_ui_down();
    issd_password_ui_down(); /* From the bottom palette row to Delete. */
    assert(issd_password_ui_confirm() == ISSD_PASSWORD_UI_STAY);
    expect_symbols(expected, 1);
    issd_password_ui_right(); issd_password_ui_right(); issd_password_ui_right();
    assert(issd_password_ui_confirm() == ISSD_PASSWORD_UI_BACK);
    assert(issd_password_ui_cancel() == ISSD_PASSWORD_UI_BACK);
}
static void text(void) {
    open_page();
    assert(issd_password_ui_text("AB CD\n+/"));
    const uint8_t expected[] = {0, 1, 2, 3, 62, 63};
    expect_symbols(expected, 6);
    assert(!issd_password_ui_text("EF@")); /* A paste rejects atomically. */
    expect_symbols(expected, 6);
    assert(issd_password_ui_message()[0]);
    assert(issd_password_ui_backspace());
    expect_symbols(expected, 5);
    open_page();
    char sixty[61]; memset(sixty, 'A', 60); sixty[60] = 0;
    assert(issd_password_ui_text(sixty));
    size_t count; issd_password_ui_symbols(&count); assert(count == 60);
    assert(!issd_password_ui_text("B"));
    issd_password_ui_symbols(&count); assert(count == 60);
    assert(issd_password_ui_confirm() == ISSD_PASSWORD_UI_STAY);
    issd_password_ui_symbols(&count); assert(count == 60);
    while (issd_password_ui_backspace()) {}
    expect_symbols(NULL, 0);
    assert(!issd_password_ui_text("\xc3\xa9"));
    expect_symbols(NULL, 0);
    /* Unsupported typed keys remain selectable from the exact palette. */
    IssdPasswordUiCallbacks no_ascii = callbacks; no_ascii.symbol_ascii = NULL;
    issd_password_ui_set_callbacks(&no_ascii);
    assert(!issd_password_ui_text("A"));
    assert(issd_password_ui_confirm() == ISSD_PASSWORD_UI_STAY);
    const uint8_t zero = 0; expect_symbols(&zero, 1);
}
static void callback_results(void) {
    open_page();
    select_action(1);
    assert(issd_password_ui_confirm() == ISSD_PASSWORD_UI_STAY);
    assert(imported == 0);
    assert(issd_password_ui_text("AB"));
    assert(issd_password_ui_confirm() == ISSD_PASSWORD_UI_STAY);
    assert(strstr(issd_password_ui_message(), "Password screen"));
    assert(imported == 1 && submitted_count == 2 && submitted[0] == 0 && submitted[1] == 1);
    accept_import = true;
    assert(issd_password_ui_confirm() == ISSD_PASSWORD_UI_IMPORTED);
    open_page();
    assert(issd_password_ui_text("AB"));
    select_action(2);
    assert(issd_password_ui_confirm() == ISSD_PASSWORD_UI_STAY);
    const uint8_t old[] = {0, 1}; expect_symbols(old, 2);
    assert(strstr(issd_password_ui_message(), "Password screen"));
    accept_export = true;
    export_kind = 1;
    assert(issd_password_ui_confirm() == ISSD_PASSWORD_UI_STAY);
    expect_symbols(old, 2);
    export_kind = 2;
    assert(issd_password_ui_confirm() == ISSD_PASSWORD_UI_STAY);
    expect_symbols(old, 2);
    export_kind = 0;
    assert(issd_password_ui_confirm() == ISSD_PASSWORD_UI_STAY);
    const uint8_t exported[] = {63, 0, 31}; expect_symbols(exported, 3);
    issd_password_ui_set_callbacks(NULL);
    assert(issd_password_ui_confirm() == ISSD_PASSWORD_UI_STAY);
    assert(issd_password_ui_message()[0]);
}
static void touch(void) {
    open_page();
    /* Box starts at (8,2), palette at (32,82), cells measure24x12. */
    assert(issd_password_ui_click(33, 83, 256, 224) == ISSD_PASSWORD_UI_STAY);
    assert(issd_password_ui_click(201, 167, 256, 224) == ISSD_PASSWORD_UI_STAY);
    const uint8_t expected[] = {0, 63}; expect_symbols(expected, 2);
    assert(issd_password_ui_click(17, 184, 256, 224) == ISSD_PASSWORD_UI_STAY);
    expect_symbols(expected, 1);
    assert(issd_password_ui_click(0, 0, 256, 224) == ISSD_PASSWORD_UI_BACK);
    assert(issd_password_ui_click(0, 0, -1, 0) == ISSD_PASSWORD_UI_STAY);
    open_page();
    /* The central box shifts96 pixels in a448pixelwide framebuffer. */
    assert(issd_password_ui_click(129, 83, 448, 224) == ISSD_PASSWORD_UI_STAY);
    expect_symbols(expected, 1);
    assert(issd_password_ui_click(185, 184, 256, 224) == ISSD_PASSWORD_UI_BACK);
}
static void render(void) {
    static uint32_t guarded[256 * 224 + 2];
    static uint32_t second[256 * 224];
    for (unsigned i = 0; i < 96; ++i) for (unsigned j = 0; j < 8; ++j) font[i][j] = (uint8_t)(i + j);
    guarded[0] = guarded[256 * 224 + 1] = 0x12345678;
    open_page();
    assert(issd_password_ui_text("A"));
    use_glyph = true;
    issd_password_ui_render(guarded + 1, 256, 224, font);
    assert(guarded[0] == 0x12345678 && guarded[256 * 224 + 1] == 0x12345678);
    memcpy(second, guarded + 1, sizeof(second));
    use_glyph = false;
    issd_password_ui_render(guarded + 1, 256, 224, font);
    assert(memcmp(second, guarded + 1, sizeof(second)) != 0);
    select_action(1);
    assert(issd_password_ui_confirm() == ISSD_PASSWORD_UI_STAY);
    memcpy(second, guarded + 1, sizeof(second));
    issd_password_ui_render(guarded + 1, 256, 224, font);
    assert(memcmp(second, guarded + 1, sizeof(second)) != 0); /* Visible callback failure. */
    issd_password_ui_render(NULL, 256, 224, font);
    issd_password_ui_render(guarded + 1, 1, 1, font);
    issd_password_ui_render(guarded + 1, -1, 224, font);
    assert(guarded[0] == 0x12345678 && guarded[256 * 224 + 1] == 0x12345678);
    open_page();
    first_name = "DIV";
    issd_password_ui_render(guarded + 1, 256, 224, font);
    /* The cartridge division key gets a recognizable division icon even when
     * there is no printable ASCII or exact bitmap for that symbol. */
    assert(guarded[1 + 85 * 256 + 43] == 0xff00ff66);
    assert(guarded[1 + 87 * 256 + 41] == 0xff00ff66);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    if (!strcmp(argv[1], "navigation")) navigation();
    else if (!strcmp(argv[1], "text")) text();
    else if (!strcmp(argv[1], "callbacks")) callback_results();
    else if (!strcmp(argv[1], "touch")) touch();
    else if (!strcmp(argv[1], "render")) render();
    else assert(false);
    return 0;
}
