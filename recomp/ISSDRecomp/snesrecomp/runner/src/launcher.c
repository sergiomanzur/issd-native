/*
 * launcher.c - ROM discovery, verification, and cached path selection.
 *
 * Executable-relative paths, cache persistence, native picking, and ROM image
 * verification live in focused modules. This file owns only the shared
 * resolution policy used by the public entry points.
 */
#include "launcher.h"
#include "host_paths.h"
#include "launcher_cache.h"
#include "launcher_picker.h"
#include "rom_image_verify.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ---- Shared resolution policy ---- */

typedef int (*RomCandidateValidator)(const char *path, int positional,
                                     const void *context);

static int has_positional_rom(int argc, char **argv) {
    return argc >= 2 && argv && argv[1] &&
           argv[1][0] != '-' && argv[1][0] != '\0';
}

static void copy_path_fallback(const char *path,
                               char *out_path, size_t max_len) {
    strncpy(out_path, path, max_len - 1);
    out_path[max_len - 1] = '\0';
}

static int resolve_rom_common(int argc, char **argv,
                              char *out_path, size_t max_len,
                              RomCandidateValidator validate,
                              const void *context) {
    if (!out_path || max_len == 0) return 0;
    out_path[0] = '\0';

    if (has_positional_rom(argc, argv)) {
        if (!snesrecomp_abspath(argv[1], out_path, max_len))
            copy_path_fallback(argv[1], out_path, max_len);
        /* Positional validators preserve the historical warning-and-continue
         * behavior for explicit command-line overrides. */
        if (!validate(out_path, 1, context)) return 0;
        snesrecomp_rom_cache_write(out_path);
        printf("[Launcher] ROM: %s\n", out_path);
        return 1;
    }

    snesrecomp_rom_cache_read(out_path, max_len);
    for (;;) {
        if (out_path[0] == '\0') {
            if (!snesrecomp_pick_rom_file(out_path, max_len)) {
                fprintf(stderr, "[Launcher] No ROM selected - exiting.\n");
                out_path[0] = '\0';
                return 0;
            }
        }
        if (validate(out_path, 0, context)) {
            snesrecomp_rom_cache_write(out_path);
            printf("[Launcher] ROM: %s\n", out_path);
            return 1;
        }
        out_path[0] = '\0';
    }
}

typedef struct CrcPolicy {
    uint32_t expected;
} CrcPolicy;

static int validate_crc(const char *path, int positional,
                        const void *context) {
    const CrcPolicy *policy = (const CrcPolicy *)context;
    if (snesrecomp_rom_verify_crc32(path, policy->expected)) return 1;
    if (!positional) return 0;
    fprintf(stderr,
            "[Launcher] Warning: CRC mismatch for '%s' - continuing anyway\n",
            path);
    return 1;
}

typedef struct ShaPolicy {
    const uint8_t *expected;
} ShaPolicy;

static int validate_sha(const char *path, int positional,
                        const void *context) {
    const ShaPolicy *policy = (const ShaPolicy *)context;
    if (snesrecomp_rom_verify_sha256(path, policy->expected)) return 1;
    if (!positional) return 0;
    fprintf(stderr,
            "[Launcher] Warning: SHA-256 mismatch for '%s' - "
            "continuing anyway\n", path);
    return 1;
}

typedef struct MultiShaPolicy {
    const uint8_t (*hashes)[32];
    size_t count;
} MultiShaPolicy;

static int validate_sha_multi(const char *path, int positional,
                              const void *context) {
    const MultiShaPolicy *policy = (const MultiShaPolicy *)context;
    if (!positional && !snesrecomp_rom_is_readable(path)) {
        fprintf(stderr,
                "[Launcher] '%s' is not readable - pick again.\n", path);
        return 0;
    }
    if (policy->count &&
        snesrecomp_rom_match_sha256(path, policy->hashes,
                                    policy->count) < 0) {
        fprintf(stderr,
                "[Launcher] Warning: '%s' is not a recognized ROM for this "
                "build - loading anyway; the game may misbehave.\n", path);
    }
    return 1;
}

/* ---- Public ---- */

int snesrecomp_launcher_resolve_rom(int argc, char **argv,
                                    char *out_path, size_t max_len,
                                    uint32_t expected_crc) {
    const CrcPolicy policy = {expected_crc};
    return resolve_rom_common(argc, argv, out_path, max_len,
                              validate_crc, &policy);
}

int snesrecomp_launcher_resolve_rom_sha256(int argc, char **argv,
                                           char *out_path, size_t max_len,
                                           const uint8_t *expected_sha256) {
    const ShaPolicy policy = {expected_sha256};
    return resolve_rom_common(argc, argv, out_path, max_len,
                              validate_sha, &policy);
}

int snesrecomp_launcher_resolve_rom_sha256_multi(
        int argc, char **argv, char *out_path, size_t max_len,
        const uint8_t (*hashes)[32], size_t n_hashes) {
    const MultiShaPolicy policy = {hashes, n_hashes};
    return resolve_rom_common(argc, argv, out_path, max_len,
                              validate_sha_multi, &policy);
}
