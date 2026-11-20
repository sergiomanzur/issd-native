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
#include <stdlib.h>
#include <string.h>

/* One copier header, in the shape the launcher has always accepted. */
#define COPIER_HEADER_SIZE 512u

/* SNES cartridge header fields inside a LoROM image. The internal header sits
 * at file 0x7FC0; the mapping byte is the last byte of the 21-byte title. */
#define HEADER_MAPPING 0x7FD5u
#define HEADER_CART_TYPE 0x7FD6u

#define IPS_MAGIC "PATCH"
#define IPS_MAGIC_SIZE 5u

/* The immutable retail image, and the work copy the engine may write. They are
 * deliberately different buffers: the mod engine patches whatever image is in
 * play, so handing it the stash would poison the IPS base (design finding from
 * fix round 2). `retail_work` is refreshed from the stash on every selection
 * that plays retail. */
static uint8_t *retail_base;
static size_t retail_size;
static uint8_t *retail_work;
static bool base_in_play_is_work;

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

bool issd_cartridge_stash_retail(const uint8_t *rom, size_t size,
                                 char *why, size_t why_cap) {
    /* A boot-time operation: dropping the previous images invalidates whatever
     * the caller last selected, so it must run before anything is in play. */
    free(retail_base);
    retail_base = NULL;
    retail_size = 0;
    free(retail_work);
    retail_work = NULL;
    base_in_play_is_work = false;

    if (!rom || size == 0)
        return fail(why, why_cap, "No cartridge image to retain.");

    const uint8_t *source;
    if (size == (size_t)ISSD_RETAIL_SIZE + COPIER_HEADER_SIZE) {
        /* The 512-byte prefix is copier metadata, not cartridge data. */
        source = rom + COPIER_HEADER_SIZE;
    } else if (size == (size_t)ISSD_RETAIL_SIZE) {
        source = rom;
    } else {
        return fail(why, why_cap,
                    "Unsupported cartridge size %zu bytes; expected 2,097,152 "
                    "bytes, or 2,097,664 with a copier header.", size);
    }

    uint8_t *copy = (uint8_t *)malloc(ISSD_RETAIL_SIZE);
    if (!copy)
        return fail(why, why_cap,
                    "No memory to retain the retail cartridge image.");
    memcpy(copy, source, ISSD_RETAIL_SIZE);
    /* The engine's writable image is allocated once here, so selecting retail
     * later can never fail for want of memory after dropping a live pack. */
    uint8_t *work = (uint8_t *)malloc(ISSD_RETAIL_SIZE);
    if (!work) {
        free(copy);
        return fail(why, why_cap,
                    "No memory to prepare the retail cartridge work image.");
    }
    retail_base = copy;
    retail_size = ISSD_RETAIL_SIZE;
    retail_work = work;
    return true;
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

/* --- the base image in play --------------------------------------------- */

/* The pack currently supplying the base, and the buffer it produced. Every
 * buffer the module hands to the engine is owned here (or is the retail work
 * copy above); nothing borrowed is ever returned. `active_path`/`active_sha`
 * are compared string-for-string, which is enough to tell a restart that
 * toggled another pack from one that changed which cartridge is played. */
#define ACTIVE_PATH_SIZE 512u
#define ACTIVE_SHA_SIZE  73u
static uint8_t *active_image;
static char     active_path[ACTIVE_PATH_SIZE];
static char     active_sha[ACTIVE_SHA_SIZE];
static bool     active_is_patch;

/* Reads a whole file into a fresh buffer. `expect` of zero accepts any size. */
static bool read_cartridge_file(const char *path, size_t expect,
                                uint8_t **out, size_t *size,
                                char *why, size_t why_cap) {
    FILE *f = fopen(path, "rb");
    if (!f) return fail(why, why_cap, "Cannot open '%s'.", path);
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return fail(why, why_cap, "Cannot measure '%s'.", path);
    }
    const long length = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0 || length <= 0) {
        fclose(f);
        return fail(why, why_cap, "'%s' is empty or unreadable.", path);
    }
    if (expect && (size_t)length != expect) {
        fclose(f);
        return fail(why, why_cap,
                    "'%s' is %ld bytes; exactly %zu bytes (headerless) is "
                    "required.", path, length, expect);
    }
    uint8_t *buffer = (uint8_t *)malloc((size_t)length);
    if (!buffer) {
        fclose(f);
        return fail(why, why_cap, "No memory to load '%s'.", path);
    }
    const bool complete = fread(buffer, 1, (size_t)length, f) == (size_t)length;
    fclose(f);
    if (!complete) {
        free(buffer);
        return fail(why, why_cap, "Could not read all of '%s'.", path);
    }
    *out = buffer;
    *size = (size_t)length;
    return true;
}

/* The pack's optional hash, checked when it carries one. */
static bool check_expected_hash(const uint8_t *image, size_t size,
                                const char *sha256, char *why, size_t why_cap) {
    if (!sha256 || !sha256[0]) return true;
    if (issd_cartridge_sha256_matches(image, size, sha256)) return true;
    return fail(why, why_cap,
                "The image does not match the pack's sha256.");
}

/* Builds the 2 MB image a pack names: a full image is read as it stands, an
 * IPS patch is applied to the retained retail image. Everything is validated
 * before the caller sees it, so a refusal leaves nothing behind. */
static bool prepare_cartridge(const char *file, bool is_patch,
                              const char *sha256, uint8_t **image_out,
                              size_t *size_out, char *why, size_t why_cap) {
    *image_out = NULL;
    *size_out = 0;

    if (is_patch) {
        /* The patch file is only metadata; the image comes from the retained
         * retail cartridge, which apply_patch verifies by hash. */
        uint8_t *patch = NULL;
        size_t patch_size = 0;
        if (!read_cartridge_file(file, 0, &patch, &patch_size, why, why_cap))
            return false;
        uint8_t *out = (uint8_t *)malloc(ISSD_RETAIL_SIZE);
        if (!out) {
            free(patch);
            return fail(why, why_cap, "No memory for the patched cartridge.");
        }
        const bool applied = issd_cartridge_apply_patch(
            patch, patch_size, out, ISSD_RETAIL_SIZE, why, why_cap);
        free(patch);
        if (!applied) {
            free(out);
            return false;
        }
        if (!issd_cartridge_validate_image(out, ISSD_RETAIL_SIZE, why, why_cap) ||
            !check_expected_hash(out, ISSD_RETAIL_SIZE, sha256, why, why_cap)) {
            free(out);
            return false;
        }
        *image_out = out;
        *size_out = ISSD_RETAIL_SIZE;
        return true;
    }

    uint8_t *out = NULL;
    size_t out_size = 0;
    if (!read_cartridge_file(file, ISSD_RETAIL_SIZE, &out, &out_size,
                             why, why_cap))
        return false;
    if (!issd_cartridge_validate_image(out, out_size, why, why_cap) ||
        !check_expected_hash(out, out_size, sha256, why, why_cap)) {
        free(out);
        return false;
    }
    *image_out = out;
    *size_out = out_size;
    return true;
}

/* Hands the engine a pristine copy of the immutable stash. The work buffer is
 * allocated with the stash, so this cannot fail once one exists. Calling it on
 * every retail selection is what makes repeated restarts correct after a
 * session of pack writes. */
static uint8_t *retail_work_refresh(void) {
    if (!retail_base || !retail_work) return NULL;
    memcpy(retail_work, retail_base, ISSD_RETAIL_SIZE);
    base_in_play_is_work = true;
    return retail_work;
}

const uint8_t *issd_cartridge_select_base(const char *file, bool is_patch,
                                          const char *sha256, size_t *size_out,
                                          bool *changed_out,
                                          char *why, size_t why_cap) {
    if (size_out) *size_out = 0;
    if (changed_out) *changed_out = false;
    if (why && why_cap) why[0] = 0;

    /* No pack active: the engine plays a work copy of the retail cartridge.
     * It must not be the stash itself, because the mod engine will write pack
     * bytes into whatever this returns and the IPS base has to stay retail. */
    if (!file || !*file) {
        const bool was_playing_hack = active_image != NULL;
        const bool work_was_live = base_in_play_is_work;
        free(active_image);
        active_image = NULL;
        active_path[0] = 0;
        active_sha[0] = 0;
        active_is_patch = false;
        uint8_t *work = retail_work_refresh();
        if (!work) {
            fail(why, why_cap, "No retail cartridge is stashed.");
            return NULL;
        }
        if (size_out) *size_out = ISSD_RETAIL_SIZE;
        /* The refresh discards pack writes that may already be live, so a
         * second retail selection is a change the caller must re-apply. */
        if (changed_out) *changed_out = was_playing_hack || work_was_live;
        return work;
    }

    /* Same cartridge as the one already in play: keep the buffer, say so. */
    if (active_image && active_is_patch == is_patch &&
        strcmp(active_path, file) == 0 &&
        strcmp(active_sha, sha256 ? sha256 : "") == 0) {
        if (size_out) *size_out = ISSD_RETAIL_SIZE;
        return active_image;
    }

    uint8_t *image = NULL;
    size_t image_size = 0;
    if (!prepare_cartridge(file, is_patch, sha256, &image, &image_size,
                           why, why_cap)) {
        /* Fail closed: a pack that cannot be built must not keep a previous
         * (or half-built) image in play. Fall back to a refreshed work copy,
         * never the stash. */
        const bool was_playing_hack = active_image != NULL;
        const bool work_was_live = base_in_play_is_work;
        free(active_image);
        active_image = NULL;
        active_path[0] = 0;
        active_sha[0] = 0;
        active_is_patch = false;
        uint8_t *work = retail_work_refresh();
        if (size_out) *size_out = work ? ISSD_RETAIL_SIZE : 0;
        if (changed_out) *changed_out = was_playing_hack || work_was_live;
        return work;
    }

    free(active_image);
    active_image = image;
    base_in_play_is_work = false;
    snprintf(active_path, sizeof active_path, "%s", file);
    snprintf(active_sha, sizeof active_sha, "%s", sha256 ? sha256 : "");
    active_is_patch = is_patch;
    if (changed_out) *changed_out = true;
    if (size_out) *size_out = image_size;
    return active_image;
}

void issd_cartridge_release(void) {
    free(retail_base);
    retail_base = NULL;
    retail_size = 0;
    free(retail_work);
    retail_work = NULL;
    base_in_play_is_work = false;
    free(active_image);
    active_image = NULL;
    active_path[0] = 0;
    active_sha[0] = 0;
    active_is_patch = false;
}
