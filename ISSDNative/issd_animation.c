#include "issd_animation.h"
#include <string.h>

#define FIRST 0x500u
#define END 0x1b00u
#define STEPS 64u

typedef struct {
  uint16_t descriptor, timer, duration, cursor, script, direction;
  uint16_t state, handler, flags, culled;
  uint16_t type;
} Snapshot;

typedef struct {
  bool seen;
  Snapshot live;
  uint16_t descriptor, timer, cursor;
} Slot;

static Slot slots[(END - FIRST) / 0x100];

static uint16_t word(const uint8_t *p, unsigned a) {
  return (uint16_t)(p[a] | (unsigned)p[a + 1] << 8);
}

static void state_word(uint8_t **p, uint16_t value) {
  *(*p)++ = (uint8_t)value;
  *(*p)++ = (uint8_t)(value >> 8);
}

/* Field order is the wire format, independent of struct layout or padding. */
#define LIVE_FIELDS(F) \
  F(descriptor) F(timer) F(duration) F(cursor) F(script) F(direction) \
  F(state) F(handler) F(flags) F(culled) F(type)

void issd_animation_save_state(uint8_t *state) {
  if (!state) return;
  memset(state, 0, ISSD_ANIMATION_STATE_SIZE);
  for (unsigned i = 0; i < sizeof(slots) / sizeof(slots[0]); i++) {
    const Slot *s = &slots[i];
    uint8_t *p = state + i * 32u;
    *p++ = s->seen ? 1 : 0;
    p += 3; /* reserved */
#define WRITE(field) state_word(&p, s->live.field);
    LIVE_FIELDS(WRITE)
#undef WRITE
    state_word(&p, s->descriptor);
    state_word(&p, s->timer);
    state_word(&p, s->cursor);
  }
}

static void state_slot(const uint8_t *p, Slot *s) {
  memset(s, 0, sizeof(*s));
  s->seen = p[0] != 0;
  p += 4;
#define READ(field) s->live.field = word(p, 0); p += 2;
  LIVE_FIELDS(READ)
#undef READ
  s->descriptor = word(p, 0); p += 2;
  s->timer = word(p, 0); p += 2;
  s->cursor = word(p, 0);
}
#undef LIVE_FIELDS

bool issd_animation_validate_state(const uint8_t *state, size_t size) {
  if (!state || size != ISSD_ANIMATION_STATE_SIZE) return false;
  for (unsigned i = 0; i < sizeof(slots) / sizeof(slots[0]); i++) {
    const uint8_t *p = state + i * 32u;
    Slot s;
    if (p[0] > 1 || p[1] || p[2] || p[3]) return false;
    state_slot(p, &s);
    if (s.live.direction >= 8 || s.live.type > 255 ||
        (s.live.cursor & 1) || s.live.cursor > STEPS * 2 ||
        (s.cursor & 1) || s.cursor > STEPS * 2) return false;
  }
  return true;
}

bool issd_animation_load_state(const uint8_t *state, size_t size) {
  if (!issd_animation_validate_state(state, size)) return false;
  for (unsigned i = 0; i < sizeof(slots) / sizeof(slots[0]); i++)
    state_slot(state + i * 32u, &slots[i]);
  return true;
}

static const uint8_t *span(const uint8_t *rom, size_t size,
                           unsigned bank, unsigned address, unsigned count) {
  if (!rom || address < 0x8000 || address > 0xffff ||
      count > 0x10000u - address) return NULL;
  size_t offset = (bank & 0x7f) * 0x8000u + (address & 0x7fff);
  if (offset > size || count > size - offset) return NULL;
  return rom + offset;
}

void issd_animation_reset(void) { memset(slots, 0, sizeof(slots)); }
void issd_animation_forget(unsigned object) {
  if (object >= FIRST && object < END && !(object & 255))
    memset(&slots[(object - FIRST) / 0x100], 0, sizeof(Slot));
}

/* $84E68F indexes an eight-direction pointer table in bank $82. Its words
 * have bit 15 set for continuing steps; a cleared bit resets the cursor.
 * That bit is restored before the descriptor is consumed by $84E6C7.
 * Validate the complete bounded script before retaining any visual state. */
static bool script(const uint8_t *rom, size_t size, const Snapshot *s,
                    uint16_t frames[STEPS], unsigned *length) {
  if (s->direction >= 8 || (s->cursor & 1) || s->cursor > STEPS * 2 ||
      !s->type || !(s->descriptor & 0x8000)) return false;
  const uint8_t *directions = span(rom, size, 0x82, s->script, 16);
  if (!directions) return false;
  unsigned base = word(directions, s->direction * 2);
  for (unsigned i = 0; i < STEPS; i++) {
    const uint8_t *step = span(rom, size, 0x82, base + i * 2, 2);
    if (!step) return false;
    uint16_t raw = word(step, 0), descriptor = raw | 0x8000;
    /* The nine-byte descriptor is geometry(2), graphics(3), detail(1),
     * shadow geometry(2), shadow selector(1): DATA_82A3D7. Both graphics
     * rows are successive length-prefixed blocks at the one pointer. */
    const uint8_t *desc = span(rom, size, 0x82, descriptor, 9);
    if (!desc || !word(desc, 0) || word(desc, 2) < 0x8000 ||
        desc[4] < 0x80) return false;
    frames[i] = descriptor;
    if (!(raw & 0x8000)) {
      *length = i + 1;
      unsigned previous = s->cursor ? s->cursor / 2 - 1 : i;
      if (previous >= *length || frames[previous] != s->descriptor)
        return false;
      if (s->duration & 0x8000)
        return span(rom, size, 0x82, s->duration, *length) != NULL;
      return s->duration > 0 && s->duration <= 255;
    }
  }
  return false;
}

bool issd_animation_running_pair(unsigned object, const uint8_t *ram,
                                 const uint8_t *rom, size_t size,
                                 uint16_t *current, uint16_t *next) {
  if (!ram || !current || !next || object < FIRST || object >= END ||
      (object & 255) || object == 0x500 || object == 0x1000 ||
      !ram[object + 0x30] || word(ram, object + 0x32) != 0x38 ||
      word(ram, object + 0x92) != 0xbecf ||
      word(ram, object + 0x1c) != 0xcf9f) return false;
  Snapshot live = {0};
  live.descriptor = word(ram, object + 0x14);
  live.timer = word(ram, object + 0x16);
  live.duration = word(ram, object + 0x18);
  live.cursor = word(ram, object + 0x1a);
  live.script = word(ram, object + 0x1c);
  live.direction = word(ram, object + 0x2e);
  live.type = ram[object + 0x30];
  uint16_t frames[STEPS];
  unsigned length;
  if (!script(rom, size, &live, frames, &length) || length != 8) return false;
  unsigned descriptor = live.descriptor, remaining = live.timer;
  const Slot *slot = &slots[(object - FIRST) / 0x100];
  /* The widescreen preparation has already advanced its frozen presentation
   * copy; use that exact descriptor/timer without touching the copy again. */
  if (word(ram, object + 0x1e) && slot->seen &&
      slot->live.script == live.script && slot->live.direction == live.direction &&
      slot->live.descriptor == live.descriptor &&
      slot->live.state == 0x38 && slot->live.handler == 0xbecf) {
    descriptor = slot->descriptor;
    remaining = slot->timer;
  }
  unsigned index = 0;
  while (index < length && frames[index] != descriptor) index++;
  if (index == length) return false;
  unsigned duration = live.duration;
  if (duration & 0x8000) {
    const uint8_t *times = span(rom, size, 0x82, duration, length);
    if (!times) return false;
    duration = times[index];
  }
  if (duration < 2 || duration > 255 || !remaining || remaining > duration ||
      remaining * 2 > duration) return false;
  *current = (uint16_t)descriptor;
  *next = frames[(index + 1) % length];
  return true;
}

bool issd_animation_pose(unsigned object, const uint8_t *ram,
                         const uint8_t *rom, size_t rom_size,
                         bool native_window, uint16_t *descriptor) {
  if (!ram || !descriptor || object < FIRST || object >= END ||
      (object & 255)) return false;
  Slot *slot = &slots[(object - FIRST) / 0x100];
  Snapshot live = {0};
  live.descriptor = word(ram, object + 0x14);
  live.timer = word(ram, object + 0x16);
  live.duration = word(ram, object + 0x18);
  live.cursor = word(ram, object + 0x1a);
  live.script = word(ram, object + 0x1c);
  live.direction = word(ram, object + 0x2e);
  live.state = word(ram, object + 0x32);
  live.handler = word(ram, object + 0x92);
  live.flags = word(ram, object + 0x54);
  live.culled = word(ram, object + 0x1e);
  live.type = ram[object + 0x30];
  /* E5DD clears $18's high bit on one-shot completion, so that field is
   * no longer a readable ROM address. Respect the parked actor before
   * trying to parse its former script or invoking any learned fallback. */
  if (live.type && (live.descriptor & 0x8000) &&
      (!live.duration || (live.timer & 0x8000) ||
       (!(live.duration & 0x8000) && live.duration > 255))) {
    memset(slot, 0, sizeof(*slot));
    *descriptor = live.descriptor;
    return true;
  }
  uint16_t frames[STEPS];
  unsigned length = 0;
  if (!script(rom, rom_size, &live, frames, &length)) {
    memset(slot, 0, sizeof(*slot));
    return false;
  }
  *descriptor = live.descriptor;
  /* Timer changes count as real progress even between pose changes. This
   * avoids advancing a running guest interpreter a second time. Every
   * action/direction change and return to the native view resynchronizes. */
  if (native_window || !live.culled || !slot->seen ||
      memcmp(&slot->live, &live, sizeof(live))) {
    slot->seen = true;
    slot->live = live;
    slot->descriptor = live.descriptor;
    slot->timer = live.timer;
    slot->cursor = live.cursor;
    return true;
  }
  /* These exact secondary handlers skip their final shared interpreter
   * solely because $1E says culled. Other states may deliberately hold,
   * so an unchanged token alone never grants permission to advance them.
   * BD27/BECF/C007 -> E641; BDD4/BE70/BF3F/C0E7 -> E683. BDD4
   * instead initializes a new action through BEA5 when $54 is negative. */
  bool table = (live.duration & 0x8000) != 0, allowed = false;
  switch (live.handler) {
    case 0xbd27: case 0xbecf: case 0xc007: allowed = table; break;
    case 0xbdd4: allowed = !table && !(live.flags & 0x8000); break;
    case 0xbe70: allowed = !table; break;
    case 0xbf3f: allowed = !table; break;
    case 0xc0e7: allowed = !table && (live.flags & 0x0200); break;
    default: break;
  }
  if (!allowed) return true; /* Guest deliberately owns unsupported holds. */
  /* $84E5D1/$84E5DD use a negative timer to park completed one-shot
   * actions. A zero duration similarly holds the terminal pose. Neither
   * is a reason to manufacture another animation cycle. */
  if (!slot->timer || (slot->timer & 0x8000)) {
    *descriptor = slot->descriptor;
    return true;
  }
  if (--slot->timer == 0) {
    unsigned index = slot->cursor / 2;
    if (index >= length) { memset(slot, 0, sizeof(*slot)); return false; }
    unsigned duration = live.duration;
    if (duration & 0x8000) {
      const uint8_t *time = span(rom, rom_size, 0x82, duration + index, 1);
      if (!time) { memset(slot, 0, sizeof(*slot)); return false; }
      duration = *time;
    }
    slot->timer = (uint16_t)duration;
    slot->descriptor = frames[index];
    slot->cursor = index + 1 == length ? 0 : (uint16_t)(slot->cursor + 2);
  }
  *descriptor = slot->descriptor;
  return true;
}
