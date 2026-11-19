/* Cartridge packs, pure C side: retain the retail image, apply an IPS patch to
 * it, and check that an image is something the engine can actually run.
 *
 * The decisions come from
 * docs/superpowers/specs/2026-11-19-rom-hack-packs-design.md: a 2,097,664-byte
 * buffer is the launcher's accepted copier-headered shape and is normalized
 * here; a patch is refused unless its base hashes to the retail SHA-256, on
 * every path and not only in the launcher; and anything that would expand or
 * truncate the 2 MB image is refused with a reason. A refused pack leaves the
 * retail cartridge in place, because corrupting the base image is worse than
 * declining to start a hack.
 *
 * The SHA-256 routine is the runner's reference implementation, the same one
 * the launcher and save code already use. */
#include "issd_cartridge.h"
#include "sha256.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* One copier header, in the shape the launcher has always accepted. */
#define COPIER_HEADER_SIZE 512u

/* SNES cartridge header fields inside a LoROM image. The internal header sits
 * at file 0x7FC0; the mapping byte is the last byte of the 21-byte title. */
#define HEADER_MAPPING 0x7FD5u
#define HEADER_CART_TYPE 0x7FD6u

#define IPS_MAGIC "PATCH"
#define IPS_MAGIC_SIZE 5u

/* The only mutable state: the retained retail image, owned by the caller. */
static const uint8_t *retail_base;
static size_t retail_size;

static bool fail(char *why, size_t cap, const char *fmt, ...) {
    if (why && cap) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(why, cap, fmt, args);
        va_end(args);
    }
    return false;
}

static uint32_t read_be16(const uint8_t *p) {
    return ((uint32_t)p[0] << 8) | p[1];
}

static uint32_t read_be24(const uint8_t *p) {
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
}

bool issd_cartridge_stash_retail(uint8_t *rom, size_t size,
                                 char *why, size_t why_cap) {
    retail_base = NULL;
    retail_size = 0;

    if (!rom || size == 0)
        return fail(why, why_cap, "No cartridge image to retain.");

    if (size == (size_t)ISSD_RETAIL_SIZE + COPIER_HEADER_SIZE) {
        /* The 512-byte prefix is copier metadata, not cartridge data. */
        retail_base = rom + COPIER_HEADER_SIZE;
        retail_size = ISSD_RETAIL_SIZE;
        return true;
    }
    if (size == (size_t)ISSD_RETAIL_SIZE) {
        retail_base = rom;
        retail_size = size;
        return true;
    }
    return fail(why, why_cap,
                "Unsupported cartridge size %zu bytes; expected 2,097,152 "
                "bytes, or 2,097,664 with a copier header.", size);
}

const uint8_t *issd_cartridge_retail(size_t *size) {
    if (size) *size = retail_size;
    return retail_base;
}

bool issd_cartridge_sha256_matches(const uint8_t *rom, size_t size,
                                   const char *expected_hex) {
    if (!rom || !expected_hex || strlen(expected_hex) != 64) return false;

    uint8_t digest[32];
    char hex[65];
    sha256_compute(rom, size, digest);
    for (int i = 0; i < 32; i++)
        snprintf(hex + i * 2, 3, "%02x", digest[i]);

    for (int i = 0; i < 64; i++) {
        char c = expected_hex[i];
        if (c >= 'A' && c <= 'F') c = (char)(c - 'A' + 'a');
        if (c != hex[i]) return false;
    }
    return true;
}

bool issd_cartridge_validate_image(const uint8_t *rom, size_t size,
                                   char *why, size_t why_cap) {
    if (!rom)
        return fail(why, why_cap, "No cartridge image to validate.");
    if (size != (size_t)ISSD_RETAIL_SIZE)
        return fail(why, why_cap,
                    "Cartridge image is %zu bytes; exactly 2,097,152 bytes "
                    "(headerless) is required.", size);

    if (rom[HEADER_MAPPING] & 0x01u)
        return fail(why, why_cap,
                    "HiROM cartridge images are not supported; this port "
                    "recompiles the LoROM USA release.");

    if (rom[HEADER_CART_TYPE] != 0x00)
        return fail(why, why_cap,
                    "Cartridge type 0x%02X claims a coprocessor or extra "
                    "hardware; only a ROM-only (0x00) cartridge is supported.",
                    rom[HEADER_CART_TYPE]);

    if (why && why_cap) why[0] = 0;
    return true;
}

/* Walks the IPS records. With `rom_out` NULL this only checks the structure,
 * so apply_patch can refuse before touching the caller's buffer; a second walk
 * with `rom_out` set writes records the first walk already accepted. Keeping
 * one walker means the arithmetic of validation and application cannot drift
 * apart. */
static bool walk_ips(const uint8_t *patch, size_t patch_size,
                     uint8_t *rom_out, char *why, size_t why_cap) {
    size_t at = IPS_MAGIC_SIZE;
    bool eof = false;
    while (at + 3u <= patch_size) {
        if (memcmp(patch + at, "EOF", 3) == 0) {
            at += 3u;
            eof = true;
            break;
        }
        if (at + 5u > patch_size)
            return fail(why, why_cap, "IPS patch is truncated: a record "
                                      "header stops early.");
        size_t offset = read_be24(patch + at);
        uint32_t length = read_be16(patch + at + 3u);
        at += 5u;

        if (offset >= (size_t)ISSD_RETAIL_SIZE)
            return fail(why, why_cap,
                        "IPS write at offset %zu is outside the 2,097,152-"
                        "byte cartridge; expansion is not supported.", offset);

        if (length == 0) {
            /* RLE: two bytes of run length and one fill byte. */
            if (at + 3u > patch_size)
                return fail(why, why_cap,
                            "IPS patch is truncated: an RLE run at offset "
                            "%zu has no fill byte.", offset);
            size_t run = read_be16(patch + at);
            uint8_t fill = patch[at + 2u];
            at += 3u;
            if (offset + run > (size_t)ISSD_RETAIL_SIZE)
                return fail(why, why_cap,
                            "IPS run at offset %zu ends outside the "
                            "2,097,152-byte cartridge; expansion is not "
                            "supported.", offset);
            if (rom_out) memset(rom_out + offset, fill, run);
        } else {
            if (at + length > patch_size)
                return fail(why, why_cap,
                            "IPS patch is truncated: the record at offset "
                            "%zu claims %u bytes but the file ends early.",
                            offset, (unsigned)length);
            if (offset + length > (size_t)ISSD_RETAIL_SIZE)
                return fail(why, why_cap,
                            "IPS record at offset %zu ends outside the "
                            "2,097,152-byte cartridge; expansion is not "
                            "supported.", offset);
            if (rom_out) memcpy(rom_out + offset, patch + at, length);
            at += length;
        }
    }

    if (!eof)
        return fail(why, why_cap,
                    "IPS patch has no EOF marker; a complete patch ends "
                    "with EOF.");

    size_t trailing = patch_size - at;
    if (trailing == 3u) {
        /* The IPS truncation extension would leave a shorter cartridge. */
        return fail(why, why_cap,
                    "IPS truncation to %u bytes refused; cartridge images "
                    "must stay 2,097,152 bytes long.",
                    (unsigned)read_be24(patch + at));
    }
    if (trailing != 0)
        return fail(why, why_cap,
                    "Unexpected trailing data after the IPS EOF marker.");
    return true;
}

bool issd_cartridge_apply_patch(const uint8_t *patch_data, size_t patch_size,
                                uint8_t *rom_out, size_t rom_cap,
                                char *why, size_t why_cap) {
    if (!retail_base || retail_size != (size_t)ISSD_RETAIL_SIZE)
        return fail(why, why_cap,
                    "No retail cartridge is stashed; an IPS patch has no "
                    "base to apply to.");
    if (!issd_cartridge_sha256_matches(retail_base, retail_size,
                                       ISSD_RETAIL_SHA256))
        return fail(why, why_cap,
                    "The stashed image is not the original USA ROM; IPS "
                    "patches apply to the retail cartridge only.");
    if (!rom_out || rom_cap < (size_t)ISSD_RETAIL_SIZE)
        return fail(why, why_cap,
                    "The output buffer must hold 2,097,152 bytes of "
                    "cartridge image.");

    /* Writing into or across the retained base would destroy the very image a
     * refusal is meant to preserve. */
    {
        uintptr_t out_start = (uintptr_t)rom_out;
        uintptr_t base_start = (uintptr_t)retail_base;
        if (out_start < base_start + retail_size &&
            base_start < out_start + rom_cap)
            return fail(why, why_cap,
                        "The output buffer overlaps the retained retail "
                        "image; apply into a separate buffer so a refused "
                        "patch cannot corrupt the base.");
    }

    if (!patch_data)
        return fail(why, why_cap, "No IPS patch data.");
    if (patch_size < IPS_MAGIC_SIZE + 3u ||
        memcmp(patch_data, IPS_MAGIC, IPS_MAGIC_SIZE) != 0)
        return fail(why, why_cap,
                    "Not an IPS patch: the PATCH signature is missing.");

    /* Every record is checked before a byte is written, so a refusal leaves
     * `rom_out` exactly as the caller passed it. */
    if (!walk_ips(patch_data, patch_size, NULL, why, why_cap)) return false;

    /* Start from the retail image so untouched regions keep retail bytes. */
    memmove(rom_out, retail_base, retail_size);

    /* The same walk just passed, so this one cannot fail; its writes are the
     * records the validation pass accepted. */
    walk_ips(patch_data, patch_size, rom_out, why, why_cap);

    if (why && why_cap) why[0] = 0;
    return true;
}
