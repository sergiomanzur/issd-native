#define main campaign_harness_main
#include "test_campaign_saves.c"
#undef main

static size_t favorite_fwrite(const void *p, size_t size, size_t count, FILE *f) {
  if (getenv("ISSD_FAVORITE_WRITE_FAILURE")) return fwrite(p, size, count ? count - 1 : 0, f);
  return fwrite(p, size, count, f);
}
static int favorite_fflush(FILE *f) {
  return getenv("ISSD_FAVORITE_FLUSH_FAILURE") ? EOF : fflush(f);
}
static int favorite_fclose(FILE *f) {
  int result = fclose(f);
  return getenv("ISSD_FAVORITE_CLOSE_FAILURE") ? EOF : result;
}
#ifdef _WIN32
#include <windows.h>
static BOOL WINAPI favorite_move(LPCSTR source, LPCSTR destination, DWORD flags) {
  if (getenv("ISSD_FAVORITE_REPLACE_FAILURE")) return FALSE;
  return MoveFileExA(source, destination, flags);
}
static BOOL WINAPI favorite_durable(HANDLE file) {
  return getenv("ISSD_FAVORITE_DURABLE_FAILURE") ? FALSE : FlushFileBuffers(file);
}
#define MoveFileExA favorite_move
#define FlushFileBuffers favorite_durable
#else
static int favorite_rename(const char *source, const char *destination) {
  if (getenv("ISSD_FAVORITE_REPLACE_FAILURE")) return -1;
  return rename(source, destination);
}
static int favorite_durable(int file) {
  return getenv("ISSD_FAVORITE_DURABLE_FAILURE") ? -1 : fsync(file);
}
#define rename favorite_rename
#define fsync favorite_durable
#endif
#define fwrite favorite_fwrite
#define fflush favorite_fflush
#define fclose favorite_fclose
#include "../ISSDNative/issd_save.c"

int main(int argc, char **argv) {
  assert(argc >= 4);
  assert(issd_save_set_directory(argv[1]));
  unsigned char base[] = {1,2,3,4}, effective[] = {1,2,3,4};
  effective[3] = (unsigned char)atoi(argv[2]);
  const char *flags = getenv("ISSD_TEST_GAMEPLAY_FLAGS");
  issd_save_set_context(base, sizeof(base), effective, sizeof(effective), flags ? (uint32_t)atoi(flags) : 0);
  if (getenv("ISSD_TEST_NO_CONTEXT")) issd_save_set_context(NULL, 0, NULL, 0, 0);
  issd_save_set_snapshot_backends(save_payload, load_payload);
  issd_save_set_snapshot_validator(getenv("ISSD_TEST_NO_VALIDATOR") ? NULL : validate_payload);
  issd_save_set_load_callback(loaded);
  g_ram[0] = 99;
  unsigned char payload[12] = {'S','L','T','R',8,0,0,0,11,0,0,0};
  if (argc > 4) payload[8] = (unsigned char)atoi(argv[4]);
  bool ok = false;
  char info[128] = {0};
  void *data = (void *)payload;
  size_t size = 123;
  unsigned value = 0;
  if (!strcmp(argv[3], "save")) ok = issd_save_match_favorite(payload, sizeof(payload), "Training setup");
  else if (!strcmp(argv[3], "null")) ok = issd_save_match_favorite(NULL, sizeof(payload), NULL);
  else if (!strcmp(argv[3], "empty")) ok = issd_save_match_favorite(payload, 0, NULL);
  else if (!strcmp(argv[3], "oversized")) ok = issd_save_match_favorite(payload, 16u * 1024u * 1024u + 1, NULL);
  else if (!strcmp(argv[3], "nulloutputs")) {
    assert(!issd_save_read_match_favorite(NULL, &size) && size == 0);
    assert(!issd_save_read_match_favorite(&data, NULL) && data == NULL);
    assert(!issd_save_read_match_favorite(NULL, NULL));
    assert(!issd_save_match_favorite_info(NULL, 12));
    assert(!issd_save_match_favorite_info(info, 0));
  }
  else if (!strcmp(argv[3], "read")) {
    ok = issd_save_read_match_favorite(&data, &size);
    if (ok) { assert(size == sizeof(payload)); value = ((unsigned char *)data)[8]; free(data); }
    else assert(!data && size == 0);
  } else if (!strcmp(argv[3], "info")) ok = issd_save_match_favorite_info(info, sizeof(info));
  else if (!strcmp(argv[3], "campaign")) ok = issd_save_campaign("Cup");
  else if (!strcmp(argv[3], "manual")) ok = issd_save_to_slot(0, "Manual");
  printf("RESULT %d %u %s | %s | ram=%u callbacks=%u\n", ok, value, issd_save_error(), info, g_ram[0], load_callbacks);
  return 0;
}
