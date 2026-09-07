/* Regression coverage for the host-side Super FX enhancement boundary.
 * No game ROM, generated data, or platform frontend is required. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "snes/superfx.h"

enum { kRomSize = 65536, kRamSize = 65536 };

static int failures;

static void check(int condition, const char *message) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s\n", message);
    failures++;
  }
}

static SuperFx *make_superfx(uint8_t **rom_out, uint8_t **ram_out) {
  uint8_t *rom = (uint8_t *)calloc(kRomSize, 1);
  uint8_t *ram = (uint8_t *)calloc(kRamSize, 1);
  if (!rom || !ram) {
    free(rom);
    free(ram);
    return NULL;
  }

  /* PLOT x=4,y=3; decrement x; ALT1; RPIX; STOP. This exercises the native
   * pixel cache in both instances without invoking an enhanced replay. */
  static const uint8_t program[] = {0x4c, 0xe1, 0x3d, 0x4c, 0x00};
  memcpy(rom, program, sizeof(program));

  SuperFx *fx = superfx_create(rom, kRomSize, ram, kRamSize);
  if (!fx) {
    free(rom);
    free(ram);
    return NULL;
  }
  fx->colr = 3;
  fx->r[1].data = 4;
  fx->r[2].data = 3;
  *rom_out = rom;
  *ram_out = ram;
  return fx;
}

static void start_at_zero(SuperFx *fx) {
  superfx_cpu_write_io(fx, 0x301e, 0);
  superfx_cpu_write_io(fx, 0x301f, 0);
  superfx_sync(fx, 10000);
}

static void destroy_fixture(SuperFx *fx, uint8_t *rom, uint8_t *ram) {
  superfx_destroy(fx);
  free(rom);
  free(ram);
}

int main(void) {
  uint8_t *native_rom = NULL, *native_ram = NULL;
  uint8_t *optin_rom = NULL, *optin_ram = NULL;
  SuperFx *native = make_superfx(&native_rom, &native_ram);
  SuperFx *optin = make_superfx(&optin_rom, &optin_ram);
  if (!native || !optin) {
    fputs("FAIL: could not allocate Super FX fixtures\n", stderr);
    destroy_fixture(native, native_rom, native_ram);
    destroy_fixture(optin, optin_rom, optin_ram);
    return 1;
  }

  check(superfx_get_enhancement_mode(native) == kSuperFxEnhancement_None,
        "new cores default to faithful Super FX");
  superfx_set_widescreen(native, 32, 0x00, 0x0000, 0x100, 0x102, 64);
  check(native->ws_extra == 0 && native->ws_pixels == NULL,
        "widescreen configuration is inert without explicit enhancement");

  superfx_set_enhancement_mode(
      optin, kSuperFxEnhancement_WidescreenLinearProjection);
  superfx_set_widescreen(optin, 32, 0x00, 0x0000, 0x100, 0x102, 64);
  check(optin->ws_extra == 32 && optin->ws_pixels != NULL &&
            optin->ws_task_state != NULL,
        "explicit enhancement allocates presentation-only replay state");

  /* Match the configured task and give it a tiny valid native viewport. The
   * enhanced clone will execute, but it must not touch authoritative state. */
  native_ram[0x100] = optin_ram[0x100] = 4;
  native_ram[0x102] = optin_ram[0x102] = 7;
  start_at_zero(native);
  start_at_zero(optin);
  check(optin->ws_pending_ready,
        "matching task produces an enhanced presentation replay");
  check(memcmp(native_ram, optin_ram, kRamSize) == 0,
        "enhanced replay does not alter native GSU RAM");
  check(memcmp((const uint8_t *)native + offsetof(SuperFx, r),
               (const uint8_t *)optin + offsetof(SuperFx, r),
               offsetof(SuperFx, enhancement_mode) - offsetof(SuperFx, r)) ==
            0,
        "enhanced replay does not alter architectural GSU execution");

  optin->ws_frame_ready = true;
  optin->ws_pending_ready = true;
  optin->ws_render_active = true;
  superfx_set_enhancement_mode(optin, kSuperFxEnhancement_None);
  check(optin->ws_extra == 0 && !optin->ws_frame_ready &&
            !optin->ws_pending_ready && !optin->ws_render_active,
        "returning to faithful mode discards all enhanced presentation state");
  check(!superfx_get_widescreen_frame(optin, NULL, NULL, NULL, NULL),
        "faithful mode never exposes an enhanced frame");

  destroy_fixture(native, native_rom, native_ram);
  destroy_fixture(optin, optin_rom, optin_ram);
  if (failures)
    return 1;
  puts("enhancement_opt_in_test: PASS");
  return 0;
}
