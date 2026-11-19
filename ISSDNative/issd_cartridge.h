/* Cartridge packs: a mod supplies either a complete replacement cartridge
 * image or an IPS patch applied to the user's own verified retail ROM. This
 * header carries the retail identity (one source of truth, also consumed by
 * the launcher) and the pure-C operations main.c needs at boot.
 *
 * The rulings are in docs/superpowers/specs/2026-11-19-rom-hack-packs-design.md:
 * copier headers are normalized here (2,097,664 bytes means a 512-byte header
 * on top of 2 MB), an IPS base must hash to the retail SHA-256 on every path,
 * and an image that is not exactly 2 MB, not LoROM or not ROM-only is refused
 * rather than guessed at. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* International Superstar Soccer Deluxe (USA), headerless. The launcher has
 * verified this size and hash since before cartridge packs existed; moving
 * them here keeps one definition for the launcher and the patch path. */
#define ISSD_RETAIL_SIZE 2097152
#define ISSD_RETAIL_SHA256 \
    "d2fe66c1ce66c65ce14e478c94be2e616f9e2cad374b5783a6a64d3c1a99cfa9"

/* Retains the retail buffer for later patching and normalizes a 512-byte
 * copier header away. The caller keeps ownership of `rom`; the effective
 * pointer is whatever issd_cartridge_retail() reports afterwards. Every call
 * replaces the previous stash, and a refusal (including a NULL or zero-size
 * buffer) clears it. `why` receives prose on refusal and is untouched on
 * success. */
bool issd_cartridge_stash_retail(uint8_t *rom, size_t size,
                                 char *why, size_t why_cap);

/* The normalized retail image and its size, or NULL/0 when nothing is stashed.
 * `size` may be NULL. */
const uint8_t *issd_cartridge_retail(size_t *size);

/* Applies an IPS patch to the stashed retail image and writes the result to
 * `rom_out`. The base must hash to ISSD_RETAIL_SHA256 (design ruling 3), the
 * output must be at least ISSD_RETAIL_SIZE bytes, and the patch may not write
 * at or past the end of the image. Expansion, truncation and malformed
 * records are refused with a reason. On false, `rom_out` is not a usable
 * image and the caller must keep playing the retail cartridge. */
bool issd_cartridge_apply_patch(const uint8_t *patch_data, size_t patch_size,
                                uint8_t *rom_out, size_t rom_cap,
                                char *why, size_t why_cap);

/* Structural checks for a full-image pack: exactly 2,097,152 bytes, LoROM
 * (bit 0 of file 0x7FD5 clear) and cart type 0x00 (ROM-only, no coprocessor).
 * The optional pack `sha256` check is the caller's, on top of this. */
bool issd_cartridge_validate_image(const uint8_t *rom, size_t size,
                                   char *why, size_t why_cap);

/* Compares the image against a 64-character hexadecimal SHA-256. Case is
 * accepted either way; a malformed or missing expected value is a miss. */
bool issd_cartridge_sha256_matches(const uint8_t *rom, size_t size,
                                   const char *expected_hex);

#ifdef __cplusplus
}
#endif
