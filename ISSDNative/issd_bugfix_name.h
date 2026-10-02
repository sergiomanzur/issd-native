#ifndef ISSD_BUGFIX_NAME_H
#define ISSD_BUGFIX_NAME_H
#include <stdbool.h>
#include <stdint.h>

/* Before interpreted $8AB3FE, redirect to the original RTS at $8AB3FD when
 * true. The interpreter bridge owns the redirect; leave all CPU registers,
 * flags and WRAM unchanged. DP must be the original name menu, $1500. */
bool issd_bugfix_name_skip_caret(const uint8_t *ram, uint16_t direct_page, bool enabled);
#endif
