#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "issd_save.h"
#include "issd_bridge.h"
#include "snes/snes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ISSD_CAMPAIGN_FAULTS
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <time.h>
#endif
#endif

uint8_t g_ram[0x20000];
int snes_frame_counter;
Snes *g_snes;
Cpu *g_snes_cpu;
void issd_bridge_update_state(IssdMatchState *s) { memset(s, 0, sizeof(*s)); }
void issd_widescreen_reset(void) {}
static unsigned load_callbacks;
static void loaded(void) { ++load_callbacks; }
static bool save_payload(const char *path) {
#ifdef ISSD_CAMPAIGN_FAULTS
  if (getenv("ISSD_TEST_SLOW_CAPTURE")) {
#ifdef _WIN32
    Sleep(150);
#else
    struct timespec delay = {0, 150000000};
    nanosleep(&delay, NULL);
#endif
  }
#endif
  FILE *f = fopen(path, "wb");
  if (!f) return false;
  unsigned char bytes[12] = {'S','L','T','R',8,0,0,0,0,0,0,0};
  bytes[8] = g_ram[0];
  bool ok = fwrite(bytes, 1, sizeof(bytes), f) == sizeof(bytes);
  if (fclose(f)) ok = false;
  return ok;
}
static bool load_payload(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  unsigned char bytes[12];
  bool ok = fread(bytes, 1, sizeof(bytes), f) == sizeof(bytes);
  fclose(f);
  if (!ok || memcmp(bytes,"SLTR",4) || bytes[4] != 8 || bytes[8] == 254) return false;
  g_ram[0] = bytes[8];
  return true;
}
static bool validate_payload(const void *payload, size_t size) {
  const unsigned char *bytes = payload;
  return size == 12 && !memcmp(bytes, "SLTR", 4) && bytes[4] == 8 && bytes[8] != 254;
}
int main(int argc, char **argv) {
  assert(argc >= 4);
  if (strcmp(argv[1], "-")) assert(issd_save_set_directory(argv[1]));
  const unsigned char base[] = {1,2,3,4};
  unsigned char effective[] = {1,2,3,4};
  effective[3] = (unsigned char)atoi(argv[2]);
  const char *flags = getenv("ISSD_TEST_GAMEPLAY_FLAGS");
  issd_save_set_context(base, sizeof(base), effective, sizeof(effective), flags ? (uint32_t)atoi(flags) : 0);
  issd_save_set_snapshot_backends(save_payload, load_payload);
  if (getenv("ISSD_TEST_VALIDATE_PAYLOAD")) issd_save_set_snapshot_validator(validate_payload);
  issd_save_set_load_callback(loaded);
  assert(issd_save_init());
  g_ram[0] = 99;
  bool ok = false;
  char info[128] = {0};
  if (!strcmp(argv[3],"save")) { g_ram[0] = (uint8_t)atoi(argv[4]); ok = issd_save_campaign("Cup setup"); }
  else if (!strcmp(argv[3],"continue")) ok = issd_save_continue();
  else if (!strcmp(argv[3],"info")) ok = issd_save_continue_info(info, sizeof(info));
  else if (!strcmp(argv[3],"manual")) { g_ram[0] = (uint8_t)atoi(argv[4]); ok = issd_save_to_slot(0,"Manual"); }
  else if (!strcmp(argv[3],"load")) ok = issd_load_from_slot_confirmed(0, argc > 4 && atoi(argv[4]));
  else if (!strcmp(argv[3],"legacy")) ok = issd_save_is_legacy(0);
  printf("RESULT %d %u %s | %s | callbacks=%u\n", ok, g_ram[0], issd_save_error(), info, load_callbacks);
  return 0;
}

/* The fault executable compiles the real storage implementation in this
 * translation unit. Interpose only file primitives here, so production code
 * has no fault environment variables or testing API. */
#ifdef ISSD_CAMPAIGN_FAULTS
static bool fault(const char *name) {
  const char *selected = getenv("ISSD_TEST_SAVE_FAILURE");
  return selected && !strcmp(selected, name);
}
static size_t failed_fwrite(const void *p, size_t size, size_t count, FILE *f) {
  if (fault("write") && size * count >= 192) return fwrite(p, size, count - 1, f);
  return fwrite(p, size, count, f);
}
static int failed_fflush(FILE *f) {
  return fault("flush") ? EOF : fflush(f);
}
static int failed_fclose(FILE *f) {
  int result = fclose(f);
  return fault("close") ? EOF : result;
}
#ifdef _WIN32
static BOOL WINAPI failed_move(LPCSTR source, LPCSTR destination, DWORD flags) {
  if (fault("replace") && strstr(destination, "/campaign.sav")) return FALSE;
  if (fault("rotation") && strstr(destination, "/campaign.1.sav")) return FALSE;
  return MoveFileExA(source, destination, flags);
}
static BOOL WINAPI failed_durable_flush(HANDLE file) {
  return fault("durable") ? FALSE : FlushFileBuffers(file);
}
#define MoveFileExA failed_move
#define FlushFileBuffers failed_durable_flush
#else
static int failed_rename(const char *source, const char *destination) {
  if (fault("replace") && strstr(destination, "/campaign.sav")) return -1;
  if (fault("rotation") && strstr(destination, "/campaign.1.sav")) return -1;
  return rename(source, destination);
}
static int failed_durable_flush(int fd) {
  return fault("durable") ? -1 : fsync(fd);
}
#define rename failed_rename
#define fsync failed_durable_flush
#endif
#define fwrite failed_fwrite
#define fflush failed_fflush
#define fclose failed_fclose
#include "../ISSDNative/issd_save.c"
#endif
