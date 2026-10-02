#include "issd_bugfix_name.h"

bool issd_bugfix_name_skip_caret(const uint8_t *ram, uint16_t direct_page, bool enabled) {
    if (!enabled || !ram || direct_page != 0x1500) return false;
    /* DATA_81E041 contains three caret positions. A completed three-letter
     * name indexes DATA_81E047 instead, interpreting the next menu's script
     * pointer as a tilemap address and queuing an unrelated VRAM upload.
     * Skip only the absent caret; the user's complete name remains intact. */
    unsigned letters = ram[0x156c] | (unsigned)ram[0x156d] << 8;
    return letters >= 3;
}
