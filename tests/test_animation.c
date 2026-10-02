#include "issd_animation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t rom[0x18000], ram[0x20000], original[0x20000];
static void put(uint8_t *p, unsigned a, unsigned v) {
  p[a] = (uint8_t)v; p[a + 1] = (uint8_t)(v >> 8);
}
static unsigned get(const uint8_t *p, unsigned a) { return p[a] | p[a+1] << 8; }
static void fixture(bool table) {
  memset(rom, 0, sizeof(rom)); memset(ram, 0, sizeof(ram));
  issd_animation_reset();
  for (unsigned d = 0; d < 8; d++) put(rom, 0x10000 + d * 2, 0x8100 + (d & 1) * 0x20);
  for (unsigned d = 0; d < 2; d++) {
    for (unsigned i = 0; i < 4; i++) {
      unsigned descriptor = 0x9000 + d * 0x40 + i * 9;
      put(rom, 0x10100 + d * 0x20 + i * 2, descriptor ^ (i == 3 ? 0x8000 : 0));
      put(rom, 0x11000 + d * 0x40 + i * 9, 0x9000);
      put(rom, 0x11000 + d * 0x40 + i * 9 + 2, 0x8000);
      rom[0x11000 + d * 0x40 + i * 9 + 4] = 0x96;
    }
  }
  rom[0x10200] = 2; rom[0x10201] = 3; rom[0x10202] = 4; rom[0x10203] = 0;
  put(ram, 0x814, 0x9000); put(ram, 0x816, 2);
  put(ram, 0x818, table ? 0x8200 : 2);
  put(ram, 0x81a, 2); put(ram, 0x81c, 0x8000);
  ram[0x830] = 1; put(ram, 0x832, 0x20);
  put(ram, 0x892, table ? 0xbecf : 0xbdd4); put(ram, 0x81e, 1);
}

static uint16_t pose(bool live) {
  uint16_t descriptor = (uint16_t)get(ram, 0x814);
  memcpy(original, ram, sizeof(ram));
  assert(issd_animation_pose(0x800, ram, rom, sizeof(rom), live, &descriptor));
  assert(!memcmp(original, ram, sizeof(ram)));
  return descriptor;
}

/* Independent transcription of $84E641/$84E683 -> $84E68F, on a separate
 * actor copy. In particular, the duration is indexed by the NEXT cursor,
 * and a sign-cleared final descriptor resets that cursor after emission. */
static void cartridge_step(uint8_t *actor, bool table) {
  unsigned timer = (get(actor, 0x16) - 1) & 0xffff;
  put(actor, 0x16, timer);
  if (timer) return;
  unsigned cursor = get(actor, 0x1a), duration = get(actor, 0x18);
  if (table) duration = rom[0x10000 + duration - 0x8000 + cursor / 2];
  put(actor, 0x16, duration);
  unsigned base = get(rom, 0x10000 + get(actor, 0x2e) * 2);
  unsigned raw = get(rom, 0x10000 + base - 0x8000 + cursor);
  put(actor, 0x1a, raw & 0x8000 ? cursor + 2 : 0);
  put(actor, 0x14, raw | 0x8000);
}

static void test_reference(bool table) {
  fixture(table);
  uint8_t actor[256]; memcpy(actor, ram + 0x800, sizeof(actor));
  assert(pose(false) == get(actor, 0x14));
  for (unsigned f = 0; f < 100; f++) {
    cartridge_step(actor, table);
    assert(pose(false) == get(actor, 0x14));
  }
  if (table) assert(pose(false) == 0x901b); /* authored one-shot hold */
}

static void test_authority(void) {
  fixture(false); pose(false); pose(false);
  assert(pose(false) == 0x9009);
  /* Different action identity with identical tokens reanchors its timer. */
  put(ram, 0x832, 0x28);
  assert(pose(false) == 0x9000);
  assert(pose(false) == 0x9000);
  assert(pose(false) == 0x9009);
  /* Live timers own the pose even if that descriptor has not changed. */
  put(ram, 0x816, 1);
  assert(pose(false) == 0x9000);
  assert(pose(true) == 0x9000);
  assert(pose(true) == 0x9000);
  /* A direction switch uses the newly authored directional sequence. */
  put(ram, 0x82e, 1); put(ram, 0x814, 0x9040); put(ram, 0x816, 2);
  assert(pose(false) == 0x9040);
  pose(false); assert(pose(false) == 0x9049);
  /* Negative timers are parked one-shot actions, never locomotion. */
  put(ram, 0x816, 0x8001);
  for (unsigned i = 0; i < 20; i++) assert(pose(false) == 0x9040);
  put(ram, 0x818, 0x0200); /* E5DD cleared the former $8200 ROM flag */
  for (unsigned i = 0; i < 20; i++) assert(pose(false) == 0x9040);
  put(ram, 0x818, 0); put(ram, 0x816, 0);
  assert(pose(false) == 0x9040);
  /* Unsupported static states and still-running native animation hold. */
  fixture(false); put(ram, 0x892, 0xc189);
  for (unsigned i = 0; i < 20; i++) assert(pose(false) == 0x9000);
  fixture(false); put(ram, 0x81e, 0);
  for (unsigned i = 0; i < 20; i++) assert(pose(false) == 0x9000);
  fixture(false); pose(false); pose(false); assert(pose(false) == 0x9009);
  issd_animation_forget(0x800);
  assert(pose(false) == 0x9000); /* same snapshot, freshly reused slot */
}

static void test_invalid(void) {
  uint16_t descriptor = 0x9000;
  fixture(false);
  assert(!issd_animation_pose(0x800, ram, NULL, sizeof(rom), false, &descriptor));
  assert(!issd_animation_pose(0x801, ram, rom, sizeof(rom), false, &descriptor));
  assert(!issd_animation_pose(0x800, ram, rom, 0x10001, false, &descriptor));
  put(ram, 0x82e, 8);
  assert(!issd_animation_pose(0x800, ram, rom, sizeof(rom), false, &descriptor));
  fixture(false); put(ram, 0x81a, 3);
  assert(!issd_animation_pose(0x800, ram, rom, sizeof(rom), false, &descriptor));
  fixture(false); put(rom, 0x10000, 0xffff);
  assert(!issd_animation_pose(0x800, ram, rom, sizeof(rom), false, &descriptor));
  fixture(false); put(rom, 0x10106, 0x901b); /* unterminated sequence */
  assert(!issd_animation_pose(0x800, ram, rom, sizeof(rom), false, &descriptor));
  fixture(false); ram[0x830] = 0;
  assert(!issd_animation_pose(0x800, ram, rom, sizeof(rom), false, &descriptor));
}

static void reject_state(const uint8_t *bad, size_t size) {
  uint8_t before[ISSD_ANIMATION_STATE_SIZE], after[ISSD_ANIMATION_STATE_SIZE];
  issd_animation_save_state(before);
  assert(!issd_animation_validate_state(bad, size));
  assert(!issd_animation_load_state(bad, size));
  issd_animation_save_state(after);
  assert(!memcmp(before, after, sizeof(before)));
}

static void test_state(bool table) {
  uint8_t saved[ISSD_ANIMATION_STATE_SIZE], after[ISSD_ANIMATION_STATE_SIZE];
  uint8_t bad[ISSD_ANIMATION_STATE_SIZE];
  uint16_t expected[48];
  fixture(table);
  for (unsigned i = 0; i < 4; i++) pose(false);
  issd_animation_save_state(saved);
  assert(issd_animation_validate_state(saved, sizeof(saved)));
  /* Descriptor is explicitly little endian in the object's wire record. */
  assert(saved[3u * 32u + 4u] == 0 && saved[3u * 32u + 5u] == 0x90);
  for (unsigned i = 0; i < 48; i++) expected[i] = pose(false);
  issd_animation_reset();
  assert(issd_animation_load_state(saved, sizeof(saved)));
  issd_animation_save_state(after);
  assert(!memcmp(saved, after, sizeof(saved)));
  for (unsigned i = 0; i < 48; i++) assert(pose(false) == expected[i]);
  reject_state(NULL, sizeof(saved));
  for (size_t n = 0; n < sizeof(saved); n++) reject_state(saved, n);
  reject_state(saved, sizeof(saved) + 1);
  const unsigned offsets[] = { 0, 1, 4 + 6, 4 + 10, 4 + 20, 30,
                                ISSD_ANIMATION_STATE_SIZE - 32 };
  const uint8_t values[] = { 2, 1, 3, 8, 0, 3, 2 };
  for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
    memcpy(bad, saved, sizeof(bad));
    bad[offsets[i]] = values[i];
    if (i == 4) bad[offsets[i] + 1] = 1; /* type exceeds a byte */
    reject_state(bad, sizeof(bad));
  }
}

int main(void) {
  test_reference(false); test_reference(true); test_authority(); test_invalid();
  test_state(false); test_state(true);
  puts("ROM animation sequencer: original routine comparison passed");
  return 0;
}
