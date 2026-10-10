#define main campaign_fixture_main
#include "test_campaign_saves.c"
#undef main
#include "../ISSDNative/issd_save.c"
int main(int argc, char **argv) {
    assert(argc == 2);
    const uint8_t rom[4] = {1,2,3,4};
    uint8_t expected[76] = {'I','S','S','D','C','T','X',1};
    uint8_t legacy[32], first[32], geometry[32] = {1};
    sha256_compute(rom, sizeof rom, expected+8);
    memcpy(expected+40, expected+8, 32);
    sha256_compute(expected, sizeof expected, legacy);
    issd_save_set_context(rom, sizeof rom, rom, sizeof rom, 0);
    assert(!memcmp(s_context, legacy, 32));
    issd_save_set_context_extra(geometry);
    assert(memcmp(s_context, legacy, 32));
    memcpy(first, s_context, 32);
    issd_save_set_context_extra(geometry);
    assert(!memcmp(s_context, first, 32));
    geometry[0] = 2;
    issd_save_set_context_extra(geometry);
    assert(memcmp(s_context, first, 32));
    issd_save_set_context(rom, sizeof rom, rom, sizeof rom, 0);
    assert(memcmp(s_context, legacy, 32));
    issd_save_set_context_extra(NULL);
    assert(!memcmp(s_context, legacy, 32));
    issd_save_set_context(rom, sizeof rom, rom, sizeof rom, 0);
    assert(!memcmp(s_context, legacy, 32));
    assert(issd_save_set_directory(argv[1]));
    issd_save_set_snapshot_backends(save_payload, load_payload);
    issd_save_set_snapshot_validator(validate_payload);
    issd_save_set_context_extra(geometry);
    assert(issd_save_init());
    g_ram[0] = 42;
    assert(issd_save_campaign("Custom geometry"));
    char paths[3][SAVE_PATH_MAX]; assert(CampaignPaths(paths, false));
    FILE *file = fopen(paths[0], "rb"); assert(file);
    uint8_t before[1024], after[1024];
    size_t size = fread(before, 1, sizeof before, file); fclose(file);
    geometry[0] = 3;
    issd_save_set_context_extra(geometry);
    g_ram[0] = 99;
    assert(!issd_save_continue());
    assert(g_ram[0] == 99);
    file = fopen(paths[0], "rb"); assert(file);
    assert(fread(after, 1, sizeof after, file) == size); fclose(file);
    assert(!memcmp(before, after, size));
    geometry[0] = 2;
    issd_save_set_context_extra(geometry);
    assert(issd_save_continue());
    assert(g_ram[0] == 42);
    return 0;
}
