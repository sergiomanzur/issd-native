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

/* Copies the normalized retail image into memory the module owns and keeps it
 * for later patching. A 512-byte copier header is dropped here; only the
 * headerless 2 MB shape is retained. The caller keeps its own buffer and may
 * keep writing to it - the mod engine patches the image the game plays - which
 * cannot touch the retained copy. Every call replaces the previous copy, and a
 * refusal (including a NULL or zero-size buffer) clears it. `why` receives
 * prose on refusal and is untouched on success. */
bool issd_cartridge_stash_retail(const uint8_t *rom, size_t size,
                                 char *why, size_t why_cap);

/* The retained retail image and its size, or NULL/0 when nothing is stashed.
 * `size` may be NULL. The pointer belongs to the module. */
const uint8_t *issd_cartridge_retail(size_t *size);

/* Frees every image the module owns: the retained retail copy and the pack
 * image currently in play. Called once by the process exit path; safe to call
 * when nothing was stashed. */
void issd_cartridge_release(void);

/* The base image the enabled stack asks for, decided once at boot and again
 * after every in-process restart. `file` is issd_mod_cartridge_resolve's full
 * path, or NULL/empty when no cartridge pack is active; `is_patch` and
 * `sha256` are its other outputs (an empty hash is not checked).
 *
 * Returns a borrowed pointer to the image to play and, in `size_out`, its
 * size: the retained retail image when no pack is active, otherwise a buffer
 * this module owns and keeps alive until the base changes again. `changed_out`
 * is true when the returned base is not the one this call last returned, which
 * is the caller's signal to rebase the live image and the pristine copy.
 *
 * An image that cannot be opened, is not exactly 2 MB and LoROM/ROM-only, or
 * does not match the pack's hash is refused with prose in `why`; the retail
 * cartridge is returned instead so a broken pack never leaves a half-built
 * image in play. Asking for the same file, mode and hash as the current base
 * returns that same buffer and reports no change, so a restart that toggled
 * another pack does not re-read two megabytes. */
const uint8_t *issd_cartridge_select_base(const char *file, bool is_patch,
                                          const char *sha256, size_t *size_out,
                                          bool *changed_out,
                                          char *why, size_t why_cap);

/* Applies an IPS patch to the stashed retail image and writes the result to
 * `rom_out`. The whole patch is validated before a single byte is written, so
 * on refusal `rom_out` is untouched and the caller keeps playing the retail
 * cartridge. The base must hash to ISSD_RETAIL_SHA256 (design ruling 3), the
 * output must be at least ISSD_RETAIL_SIZE bytes, and the patch may not write
 * at or past the end of the image. Expansion, truncation and malformed
 * records are refused with a reason. `rom_out` must not overlap the retained
 * retail image: an in-place apply is refused because a failed patch would
 * otherwise corrupt the base. */
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
