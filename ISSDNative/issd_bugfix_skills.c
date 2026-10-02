#include "issd_bugfix_skills.h"

static unsigned word(const uint8_t *r, unsigned a) {
    return r[a] | (unsigned)r[a + 1] << 8;
}

bool issd_bugfix_skills(uint8_t *r, uint16_t d, bool enabled) {
    if (!enabled || !r || d != 0x1500 || word(r, d + 0xec) != 1 ||
        word(r, d + 0x1c) != 0 || word(r, d + 0x1e) != 1) return false;
    unsigned side = word(r, d + 0x22);
    if (side > 1 || word(r, d + 0x46) != 0) return false;
    /* $86B139 wraps the last row to the footer. $86B0D7 publishes $FFFF
     * and $869F8F skips refreshing skills there. On the next Down, $86B12C
     * clears the footer and selects row zero, then returns without $869F8F.
     * Thus entering edit copies the last row's skills into the goalkeeper.
     * Decreasing those phantom excess skills refunds points via $86B1C6.
     * Perform just the missing player-cache refresh, as $86A6F5/$86A71B
     * normally do. The original code still owns navigation and accounting. */
    unsigned slot = 0; /* Only the starting roster has the footer. */
    unsigned player = r[0x3f90 + side * 20 + slot] & 0x1f;
    if (player >= 20) return false;
    unsigned record = 0x3e00 + side * 200 + player * 10;
    for (unsigned i = 0; i < 9; i++)
        if (((r[record + i / 2] >> (i % 2 * 4)) & 15) > 9) return false;
    for (unsigned i = 0; i < 9; i++) {
        r[d + 0xaa + i] = (uint8_t)((r[record + i / 2] >> (i % 2 * 4)) & 15);
        /* $86A754 also refreshes the display comparison cache. The menu
         * already unpacked the roster's originals at $86B490 into C100. */
        r[0xcd00 + i] = r[0xc100 + slot * 9 + i];
    }
    r[d + 0x9e] = (uint8_t)slot;
    r[d + 0x9f] = 0;
    return true;
}
