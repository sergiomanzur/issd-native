/* Cross-chain gate (Task 6): the IPS that tools/convert_rom_hack.py writes
 * must apply through the loader's own reader, issd_cartridge_apply_patch, and
 * reproduce the hack byte-for-byte. The two implementations were built in
 * separate worktrees, so this is the only test that proves the writer and the
 * reader agree on records, RLE runs, offsets and EOF.
 *
 * The base is the user's real retail cartridge: the loader refuses an IPS
 * whose base does not hash to the retail SHA-256, and that check is part of
 * what this test exercises. The pytest wrapper builds the synthetic hack and
 * the IPS with the converter, then passes both paths here (argv[1] scratch is
 * kept for the loader's own scratch use; argv[2] is the patch, argv[3] the
 * hack). Nothing is written inside the repository.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "issd_cartridge.h"

#define RETAIL_PATH "International Superstar Soccer Deluxe (USA).sfc"

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long end = ftell(f);
    if (end < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    uint8_t *data = malloc((size_t)end);
    if (!data) {
        fclose(f);
        return NULL;
    }
    const size_t got = fread(data, 1, (size_t)end, f);
    fclose(f);
    if (got != (size_t)end) {
        free(data);
        return NULL;
    }
    *size = got;
    return data;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s SCRATCH PATCH HACK\n", argv[0]);
        return 2;
    }

    size_t retail_size = 0;
    uint8_t *retail = read_file(RETAIL_PATH, &retail_size);
    if (!retail || retail_size != ISSD_RETAIL_SIZE) {
        fprintf(stderr, "cannot read the retail cartridge as %u bytes\n",
                (unsigned)ISSD_RETAIL_SIZE);
        free(retail);
        return 1;
    }
    if (!issd_cartridge_sha256_matches(retail, retail_size,
                                       ISSD_RETAIL_SHA256)) {
        fprintf(stderr, "the file at %s is not the supported retail image\n",
                RETAIL_PATH);
        free(retail);
        return 1;
    }
    char why[256] = "";
    if (!issd_cartridge_stash_retail(retail, retail_size, why, sizeof why)) {
        fprintf(stderr, "cannot stash the retail image: %s\n", why);
        free(retail);
        return 1;
    }

    size_t patch_size = 0, hack_size = 0;
    uint8_t *patch = read_file(argv[2], &patch_size);
    uint8_t *hack = read_file(argv[3], &hack_size);
    if (!patch || patch_size == 0 || !hack ||
        hack_size != ISSD_RETAIL_SIZE) {
        fprintf(stderr, "cannot read the generated patch or the hack\n");
        free(patch);
        free(hack);
        free(retail);
        return 1;
    }
    if (memcmp(hack, retail, retail_size) == 0) {
        fprintf(stderr, "the fixture hack is identical to retail; nothing "
                        "would be proven\n");
        free(patch);
        free(hack);
        free(retail);
        return 1;
    }

    uint8_t *out = malloc(retail_size);
    if (!out) {
        free(patch);
        free(hack);
        free(retail);
        return 1;
    }
    why[0] = 0;
    if (!issd_cartridge_apply_patch(patch, patch_size, out, retail_size,
                                    why, sizeof why)) {
        fprintf(stderr, "the loader refused the converter's IPS: %s\n", why);
        free(out);
        free(patch);
        free(hack);
        free(retail);
        return 1;
    }

    size_t at = retail_size;
    for (size_t i = 0; i < retail_size; i++) {
        if (out[i] != hack[i]) {
            at = i;
            break;
        }
    }
    if (at != retail_size) {
        fprintf(stderr,
                "loader output differs from the hack at file 0x%zX "
                "(loader 0x%02X, hack 0x%02X)\n",
                at, out[at], hack[at]);
        free(out);
        free(patch);
        free(hack);
        free(retail);
        return 1;
    }

    free(out);
    free(patch);
    free(hack);
    free(retail);
    puts("cartridge round-trip test passed");
    return 0;
}
