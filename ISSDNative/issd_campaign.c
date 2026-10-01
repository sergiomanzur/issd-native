#include "issd_campaign.h"
#include <stddef.h>
#include <string.h>

enum { CUP = 1, WORLD_SERIES = 2, WORLD_COMPLETE = 3, CUP_COMPLETE = 4,
       CUP_IMPORT = 5, WORLD_IMPORT = 6,
       PROGRESS_SIZE = 0x300 + 0x58 + 0x26 + 4 };
static unsigned armed_kind, pending_kind;
static bool emitted;
static bool observed, have_result;
static bool password_import_pending;
static uint64_t last_result;
static unsigned last_result_event;
static uint8_t previous_progress[PROGRESS_SIZE];
static uint8_t setup_progress[PROGRESS_SIZE];
static bool have_setup;

static unsigned word(const uint8_t *ram, unsigned address) {
    return ram[address] | (unsigned)ram[address + 1] << 8;
}
static unsigned callback(const uint8_t *ram) {
    return word(ram, 0x1446) | (unsigned)ram[0x1448] << 16;
}
static unsigned campaign_kind(const uint8_t *ram) {
    unsigned flags = word(ram, 0x1648);
    if ((flags & 0x24) == 0x04) return CUP;
    if ((flags & 0x24) == 0x20) return WORLD_SERIES;
    return 0;
}
static void progress(const uint8_t *ram, uint8_t *out) {
    memcpy(out, ram + 0xdc00, 0x300); out += 0x300;
    memcpy(out, ram + 0x1640, 0x58); out += 0x58;
    memcpy(out, ram + 0x1e4a, 0x26); out += 0x26;
    memcpy(out, ram + 0x0da0, 2); out += 2;
    memcpy(out, ram + 0x0ea0, 2);
}

void issd_campaign_reset(void) {
    armed_kind = pending_kind = 0;
    emitted = false;
    observed = have_result = false;
    password_import_pending = false;
    last_result = 0;
    last_result_event = 0;
    memset(previous_progress, 0, sizeof previous_progress);
    have_setup = false;
}

void issd_campaign_note_password_import(void) {
    issd_campaign_reset();
    observed = true;
    password_import_pending = true;
}

const char *issd_campaign_tick(const uint8_t *ram, bool healthy) {
    if (!ram || !healthy) {
        pending_kind = 0;
        return NULL;
    }
    bool first = !observed;
    observed = true;
    unsigned kind = campaign_kind(ram);
    if (!kind) {
        armed_kind = pending_kind = 0;
        emitted = false;
        have_setup = false;
        return NULL;
    }
    if (word(ram, 0x32) != 6 || word(ram, 0x70) != 12) {
        pending_kind = 0;
        return NULL;
    }
    unsigned cb = callback(ram);
    unsigned flags = word(ram, 0x1648);
    /* These cartridge input callbacks follow committed standings updates.
     * World Series has a separate final table after the 35th result. */
    unsigned event = 0;
    if ((kind == CUP || !(flags & 1)) && word(ram, 0x1652) &&
        !word(ram, 0x1460) && !word(ram, 0x1462)) {
        if (kind == CUP && cb == 0x85c37c) event = CUP;
        if (kind == WORLD_SERIES && cb == 0x8b9496) event = WORLD_SERIES;
        if (kind == WORLD_SERIES && cb == 0x8b94ee && word(ram, 0x1652) == 35)
            event = WORLD_COMPLETE;
    }
    if (!word(ram, 0x1460) && !word(ram, 0x1462)) {
        unsigned stage = word(ram, 0x1640), winner = ram[0xddce];
        if (kind == CUP && (flags & 8) && cb == 0x85d32e) {
            if (stage >= 5 && stage <= 8) event = CUP;
            if (stage == 9 && !word(ram, 0x1652) && winner <= 0x46 && !(winner & 1))
                event = CUP_COMPLETE;
        }
        if (password_import_pending) {
            if (kind == CUP && cb == 0x8b953c) event = CUP_IMPORT;
            if (kind == WORLD_SERIES && cb == 0x8b9414) event = WORLD_IMPORT;
        }
    }
    if (event) {
        uint64_t token = (uint64_t)word(ram, 0x1640) << 48 | (uint64_t)word(ram, 0x1652) << 32 |
            (uint64_t)word(ram, 0x165e) << 16 | word(ram, 0x1660);
        if (first) {
            have_result = true;
            last_result = token;
            last_result_event = event;
        }
        if (have_result && last_result == token &&
            (last_result_event == event ||
             (last_result_event == WORLD_COMPLETE && event == WORLD_SERIES))) {
            pending_kind = 0;
            return NULL;
        }
        uint8_t current[PROGRESS_SIZE];
        progress(ram, current);
        if (pending_kind != event || memcmp(current, previous_progress, sizeof current)) {
            memcpy(previous_progress, current, sizeof current);
            pending_kind = event;
            return NULL;
        }
        have_result = true;
        last_result = token;
        last_result_event = event;
        if (event == CUP_IMPORT || event == WORLD_IMPORT) {
            last_result_event = kind;
            emitted = true;
            have_setup = true;
            memcpy(setup_progress, current, sizeof current);
            armed_kind = 0;
        }
        pending_kind = 0;
        password_import_pending = false;
        if (event == WORLD_COMPLETE) return "World Series complete";
        if (event == CUP_COMPLETE) return "International Cup complete";
        if (event == CUP_IMPORT) return "International Cup password";
        if (event == WORLD_IMPORT) return "World Series password";
        return kind == CUP ? "International Cup result" : "World Series result";
    }
    /* Initial setup needs a witnessed preparation-to-settled transition. */
    if (!(flags & 1) || word(ram, 0x1652) || word(ram, 0x165e) || word(ram, 0x1660)) {
        pending_kind = armed_kind = 0;
        return NULL;
    }
    if ((kind == CUP && cb == 0x85c37c) ||
        (kind == WORLD_SERIES && cb == 0x8b9431)) {
        uint8_t current[PROGRESS_SIZE];
        progress(ram, current);
        if (emitted && have_setup && memcmp(current, setup_progress, sizeof current))
            emitted = false;
        if (!emitted) {
            armed_kind = kind;
            have_result = false;
        }
        pending_kind = 0;
        return NULL;
    }
    bool settled = !word(ram, 0x1460) && !word(ram, 0x1462) &&
        ((kind == CUP && cb == 0xa4beb6) ||
         (kind == WORLD_SERIES && cb == 0x86c3eb));
    if (settled && password_import_pending) armed_kind = kind;
    if (settled && first && !password_import_pending) {
        emitted = have_setup = true;
        progress(ram, setup_progress);
        pending_kind = armed_kind = 0;
        return NULL;
    }
    if (!settled || emitted || armed_kind != kind) {
        pending_kind = 0;
        return NULL;
    }
    uint8_t current[PROGRESS_SIZE];
    progress(ram, current);
    if (have_setup && !password_import_pending &&
        !memcmp(current, setup_progress, sizeof current)) {
        emitted = true;
        pending_kind = armed_kind = 0;
        return NULL;
    }
    if (pending_kind != kind || memcmp(current, previous_progress, sizeof current)) {
        memcpy(previous_progress, current, sizeof current);
        pending_kind = kind;
        return NULL;
    }
    emitted = true;
    have_setup = true;
    memcpy(setup_progress, current, sizeof current);
    password_import_pending = false;
    pending_kind = armed_kind = 0;
    return kind == CUP ? "International Cup setup" : "World Series setup";
}

bool issd_campaign_can_export_password(const uint8_t *ram) {
    if (!ram || word(ram, 0x32) != 6 || word(ram, 0x70) != 12 ||
        word(ram, 0x1460) || word(ram, 0x1462)) return false;
    unsigned kind = campaign_kind(ram), cb = callback(ram);
    unsigned flags = word(ram, 0x1648);
    if ((kind == CUP && cb == 0x8b953c) || (kind == WORLD_SERIES && cb == 0x8b9414))
        return true;
    if (kind == CUP && (flags & 8) && cb == 0x85d32e) {
        unsigned stage = word(ram, 0x1640);
        return (stage >= 5 && stage <= 8) ||
            (stage == 9 && !word(ram, 0x1652) && ram[0xddce] <= 0x46 && !(ram[0xddce] & 1));
    }
    if ((flags & 1) && !word(ram, 0x1652)) {
        return !word(ram, 0x1652) && !word(ram, 0x165e) && !word(ram, 0x1660) &&
            ((kind == CUP && cb == 0xa4beb6) || (kind == WORLD_SERIES && cb == 0x86c3eb));
    }
    return word(ram, 0x1652) &&
        ((kind == CUP && cb == 0x85c37c) ||
         (kind == WORLD_SERIES && !(flags & 1) && cb == 0x8b9496) ||
         (kind == WORLD_SERIES && !(flags & 1) && cb == 0x8b94ee && word(ram, 0x1652) == 35));
}
