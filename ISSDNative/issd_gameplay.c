#include "issd_gameplay.h"

static unsigned word(const uint8_t *r, unsigned a) {
    return r[a] | (unsigned)r[a + 1] << 8;
}
static void put(uint8_t *r, unsigned a, int v) {
    r[a] = (uint8_t)v; r[a + 1] = (uint8_t)((unsigned)v >> 8);
}
static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static bool human_controlled(const uint8_t *r, unsigned actor) {
    /* The selected bit also belongs to CPU players. The cartridge's five
     * controller records carry the actual actor and human/CPU ownership. */
    for (unsigned controller = 0x1aa0; controller <= 0x1b60; controller += 0x30)
        if (word(r, controller + 0x2c) == actor && !(word(r, controller + 0x2e) & 0x8000)) return true;
    return false;
}
static bool live_ai(const uint8_t *r, unsigned a) {
    return r && word(r, 0x32) == 6 && word(r, 0x70) == 8 &&
           !word(r, 0xbc) && word(r, a + 0x30) &&
           !(word(r, a + 0x6e) & 0x0200) && !human_controlled(r, a) && word(r, 0xd8) != a;
}
static bool pitch(const uint8_t *r, int *length, int *width) {
    *length = (int)word(r, 0x12a2); *width = (int)word(r, 0x12a4);
    return *length >= 1024 && *length <= 4096 && *width >= 256 && *width <= 1024;
}

bool issd_gameplay_goalkeeper(uint8_t *r, uint16_t a, bool enabled) {
    if (!enabled || (a != 0x500 && a != 0x1000) || !live_ai(r, a) ||
        !(word(r, a + 0x60) & 0x2000) || (word(r, a + 0x9e) & 0x8000) ||
        word(r, a + 0x4c) || (word(r, 0x11fa) & 0x8000)) return false;
    int length, width;
    if (!pitch(r, &length, &width)) return false;
    int x = (int)word(r, a + 0x50), y = (int)word(r, a + 0x52);
    if ((x != 16 && x != length - 16) || y > width) return false;
    /* Ball velocities are signed 16.16 in isometric coordinates. Convert to
     * longitudinal/lateral pitch axes; using the full fraction also handles
     * slow shots deterministically. Original dive/catch decisions stand. */
    int64_t dy = (int16_t)word(r, 0x428) * INT64_C(65536) + word(r, 0x426);
    int64_t dx = (int16_t)word(r, 0x424) * INT64_C(65536) + word(r, 0x422) - dy;
    int64_t distance = x - (int)word(r, 0x42a);
    if (!dx || (distance < 0) != (dx < 0) || !distance) return false;
    int64_t time = distance * 65536 / dx;
    if (time < 1 || time > 40) return false;
    int predicted = (int)word(r, 0x42c) + (int)(dy * distance / dx);
    /* Ignore shots outside the goal corridor; the cartridge handles those. */
    if (predicted < width / 2 - 80 || predicted > width / 2 + 80) return false;
    int target = clamp(y + clamp(predicted - y, -12, 12), 0, width);
    if (target == y) return false;
    put(r, a + 0x52, target);
    return true;
}

bool issd_gameplay_player(uint8_t *r, uint16_t a, bool enabled) {
    if (!enabled || a < 0x600 || a > 0x1a00 || (a & 0xff) || a == 0x1000 ||
        !live_ai(r, a)) return false;
    unsigned team = word(r, a + 0x9a), slot = word(r, a + 0x68);
    if ((team != 0xd00 && team != 0xe00) || slot < 1 || slot > 10 ||
        (a < 0x1000 ? team != 0xd00 : team != 0xe00)) return false;
    unsigned formation = r[team + 0xa6];
    if (formation >= 16) return false;
    int length, width;
    if (!pitch(r, &length, &width)) return false;
    unsigned lineup = team == 0xd00 ? 0x3f90 : 0x3fa4;
    unsigned selected = r[lineup + slot], roster = selected & 0x1f;
    if ((selected & 0x20) || roster >= 20) return false;
    unsigned stats = (team == 0xd00 ? 0x3e00 : 0x3ec8) + roster * 10;
    if ((r[stats + 7] & 0x88) || r[stats + 8] > 4 ||
        (r[stats] & 15) > 9 || (r[stats] >> 4) > 9) return false;
    unsigned pos = (team == 0xd00 ? 0xd000 : 0xd140) + formation * 20 + (slot - 1) * 2;
    unsigned roles = (team == 0xd00 ? 0xd280 : 0xd320) + formation * 10;
    unsigned role = r[roles + slot - 1];
    if (role != 1 && role != 2 && role != 3 && role != 5 && role != 6) return false;
    int x = (int)word(r, a + 0x50), y = (int)word(r, a + 0x52);
    if (x < 0 || x > length || y < 0 || y > width) return false;
    bool mirrored = (word(r, a + 0x6e) & 1) != 0;
    if (mirrored) { x = length - x; y = width - y; }
    /* Match the cartridge's roster/condition refresh, not an artificial speed
     * bonus. A substitution changes the response cap at the next AI decision. */
    int condition = (int)r[stats + 8] * 2 - 4;
    int speed = r[stats] & 15, inverse = 9 - (r[stats] >> 4);
    if (condition < 0) { speed = clamp(speed + condition, 0, 9); inverse = clamp(inverse - condition, 0, 9); }
    int cap = 8 + speed * 2;
    int depth = (int8_t)r[pos] * 8, lateral = (int8_t)r[pos + 1] * 4;
    /* Defenders hold their lanes more firmly; attacking roles retain more of
     * the original ball-driven target. Small bounded offsets preserve native
     * pressing/interception while making each active formation visible. */
    int divisor = role == 1 ? 2 : (role == 3 || role == 6 ? 4 : 3);
    int new_x = clamp(x + clamp(depth / divisor, -cap, cap), 0, length);
    int home_y = clamp(width / 2 + lateral, 16, width - 16);
    int new_y = clamp(y + clamp((home_y - y) / divisor, -cap, cap), 0, width);
    if (mirrored) { new_x = length - new_x; new_y = width - new_y; }
    bool changed = word(r, a + 0x50) != (unsigned)new_x || word(r, a + 0x52) != (unsigned)new_y;
    put(r, a + 0x50, new_x); put(r, a + 0x52, new_y);
    r[a + 0x62] = (uint8_t)speed; r[a + 0x66] = (uint8_t)inverse; r[a + 0x67] = r[stats + 2] >> 4;
    return changed;
}
