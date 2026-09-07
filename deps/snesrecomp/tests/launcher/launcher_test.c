/* Focused regression test for launcher resolution and SMC header stripping. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crc32.h"
#include "launcher.h"
#include "launcher_cache.h"
#include "sha256.h"

static void fail(const char *message) {
    fprintf(stderr, "launcher_test: FAIL: %s\n", message);
    exit(1);
}

static void write_headered_rom(const char *path,
                               const uint8_t *payload, size_t payload_size) {
    FILE *f = fopen(path, "wb");
    if (!f) fail("could not create test ROM");
    uint8_t header[512] = {0};
    if (fwrite(header, 1, sizeof(header), f) != sizeof(header) ||
        fwrite(payload, 1, payload_size, f) != payload_size) {
        fclose(f);
        fail("could not write test ROM");
    }
    fclose(f);
}

int main(void) {
    uint8_t payload[1024];
    for (size_t i = 0; i < sizeof(payload); i++)
        payload[i] = (uint8_t)(i * 37u + 11u);

    char rom_path[1024];
    char cfg_path[1024];
    if (!snesrecomp_exe_dir_path("launcher_test_rom.bin",
                                 rom_path, sizeof(rom_path)) ||
        !snesrecomp_exe_dir_path("rom.cfg", cfg_path, sizeof(cfg_path)))
        fail("could not resolve executable directory");
    write_headered_rom(rom_path, payload, sizeof(payload));

    char *argv[] = {"launcher_test", rom_path, NULL};
    char resolved[1024];
    uint32_t crc = crc32_compute(payload, sizeof(payload));
    if (!snesrecomp_launcher_resolve_rom(
            2, argv, resolved, sizeof(resolved), crc))
        fail("CRC32 resolver rejected a headered ROM");
    if (strcmp(resolved, rom_path) != 0)
        fail("CRC32 resolver changed the absolute ROM path");

    char cached[1024];
    if (!snesrecomp_rom_cache_read(cached, sizeof(cached)) ||
        strcmp(cached, rom_path) != 0)
        fail("resolver did not persist the selected ROM");

    if (!snesrecomp_rom_cache_write("cache-roundtrip.sfc") ||
        !snesrecomp_rom_cache_read(cached, sizeof(cached)) ||
        strcmp(cached, "cache-roundtrip.sfc") != 0)
        fail("ROM cache API did not round-trip");

    uint8_t sha[32];
    sha256_compute(payload, sizeof(payload), sha);
    if (!snesrecomp_launcher_resolve_rom_sha256(
            2, argv, resolved, sizeof(resolved), sha))
        fail("SHA-256 resolver rejected a headered ROM");

    uint8_t hashes[1][32];
    memcpy(hashes[0], sha, sizeof(sha));
    if (!snesrecomp_launcher_resolve_rom_sha256_multi(
            2, argv, resolved, sizeof(resolved), hashes, 1))
        fail("multi-hash resolver rejected a known headered ROM");

    if (snesrecomp_launcher_resolve_rom(
            2, argv, resolved, 0, crc) != 0)
        fail("zero-size output buffer was accepted");
    if (snesrecomp_rom_cache_read(cached, 0) != 0)
        fail("zero-size cache output buffer was accepted");
    if (snesrecomp_rom_cache_write("") != 0)
        fail("empty ROM cache path was accepted");

    remove(rom_path);
    remove(cfg_path);
    puts("launcher_test: PASS");
    return 0;
}
