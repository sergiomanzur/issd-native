#ifndef ISSD_PASSWORD_H
#define ISSD_PASSWORD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ISSD_PASSWORD_MAX_SYMBOLS 60
#define ISSD_PASSWORD_RAM_SIZE 0x20000
bool issd_password_set_context(const uint8_t *base, size_t base_size,
                               const uint8_t *effective, size_t effective_size,
                               uint32_t gameplay_flags);
bool issd_password_available(void);
bool issd_password_encode(const uint8_t *ram, uint8_t *symbols, size_t capacity, size_t *count);
/* Decode and validate in private WRAM. Output is published only on success;
 * it is diagnostic/staging state, not a replacement for original restore flow. */
bool issd_password_decode(const uint8_t *symbols, size_t count,
                          const uint8_t *ram, uint8_t *decoded_ram);
bool issd_password_input_ready(const uint8_t *ram);
/* Queue validated symbols in the original, settled cartridge Password task.
 * The cartridge performs the actual campaign restoration on its next frame. */
bool issd_password_submit(uint8_t *ram, const uint8_t *symbols, size_t count);
const char *issd_password_error(void);
const char *issd_password_symbol_label(unsigned symbol);
int issd_password_symbol_from_char(int character);
uint8_t issd_password_glyph(unsigned symbol);
#ifdef __cplusplus
}
#endif
#endif
