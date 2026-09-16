#include "issd_save.h"
#include "snes/snes.h"
#include "cpu_state.h"
#include "issd_bridge.h"
#include "common_cpu_infra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "issd_android.h"
#include "issd_widescreen.h"
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <errno.h>
#endif

extern uint8_t g_ram[0x20000];

#define SAVE_MAGIC 0x44535349 /* 'ISSD' */
#define SAVE_VERSION 2
#define SAVES_DIR "saves"

static const char *GetSavesDir(char *buf, size_t size) {
#ifdef ISSD_ANDROID
    const char *internal = issd_android_internal_dir();
    if (internal && internal[0]) {
        snprintf(buf, size, "%s/saves", internal);
        return buf;
    }
#endif
    snprintf(buf, size, "%s", SAVES_DIR);
    return buf;
}

static void EnsureSaveDir(void) {
    char dir[1024];
    GetSavesDir(dir, sizeof(dir));
#ifdef _WIN32
    _mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
}

static void GetSlotPath(int slot_index, char *out_path, size_t max_len) {
    char dir[1024];
    GetSavesDir(dir, sizeof(dir));
    if (slot_index < 0) {
        snprintf(out_path, max_len, "%s/quicksave.sav", dir);
    } else {
        snprintf(out_path, max_len, "%s/slot_%d.sav", dir, slot_index);
    }
}

/* Move the PPU snapshot region between the live PPU and a slot. Absent a PPU
 * (unit tests, pre-init) the simulation half of the slot still round-trips. */
static void CopyVideoState(IssdSaveSlot *slot, bool to_ppu) {
    Ppu *ppu = g_snes ? g_snes->ppu : NULL;
    if (!ppu) return;
    if (to_ppu) {
        memcpy(ppu->cgram, slot->cgram, sizeof(slot->cgram));
        memcpy(ppu->oam, slot->oam, sizeof(slot->oam));
        memcpy(ppu->highOam, slot->high_oam, sizeof(slot->high_oam));
        memcpy(ppu->vram, slot->vram, sizeof(slot->vram));
    } else {
        memcpy(slot->cgram, ppu->cgram, sizeof(slot->cgram));
        memcpy(slot->oam, ppu->oam, sizeof(slot->oam));
        memcpy(slot->high_oam, ppu->highOam, sizeof(slot->high_oam));
        memcpy(slot->vram, ppu->vram, sizeof(slot->vram));
    }
}

static IssdSnapshotSaveFn s_save_snapshot_backend = NULL;
static IssdSnapshotLoadFn s_load_snapshot_backend = NULL;

void issd_save_set_snapshot_backends(IssdSnapshotSaveFn save_fn, IssdSnapshotLoadFn load_fn) {
    s_save_snapshot_backend = save_fn;
    s_load_snapshot_backend = load_fn;
}

bool issd_save_init(void) {
    EnsureSaveDir();
    return true;
}

bool issd_save_to_slot(int slot_index, const char *label) {
    EnsureSaveDir();
    char path[1024];
    GetSlotPath(slot_index, path, sizeof(path));

    if (s_save_snapshot_backend) {
        bool ok = s_save_snapshot_backend(path);
        if (ok) {
            printf("[Save] Snapshot saved to '%s' (Slot %d)\n", path, slot_index < 0 ? 0 : slot_index + 1);
        } else {
            fprintf(stderr, "[Save] Snapshot save failed for '%s'\n", path);
        }
        return ok;
    }

    IssdSaveSlot slot;
    memset(&slot, 0, sizeof(slot));

    slot.magic = SAVE_MAGIC;
    slot.version = SAVE_VERSION;
    slot.timestamp = (uint64_t)time(NULL);
    slot.frame_counter = snes_frame_counter;

    IssdMatchState ms;
    issd_bridge_update_state(&ms);

    slot.game_mode1 = ms.game_mode1;
    slot.game_mode2 = ms.game_mode2;
    slot.p1_team = ms.p1_team;
    slot.p2_team = ms.p2_team;
    slot.p1_score = ms.p1_score;
    slot.p2_score = ms.p2_score;
    slot.match_seconds_remaining = (uint16_t)(ms.timer_minutes * 60 + ms.timer_seconds);

    if (label && label[0]) {
        strncpy(slot.slot_label, label, sizeof(slot.slot_label) - 1);
    } else {
        snprintf(slot.slot_label, sizeof(slot.slot_label), "Save %d", slot_index);
    }

    /* Copy WRAM */
    memcpy(slot.wram, g_ram, sizeof(slot.wram));
    CopyVideoState(&slot, false);

    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "[Save] Failed to open '%s' for writing.\n", path);
        return false;
    }

    size_t written = fwrite(&slot, 1, sizeof(slot), f);
    fclose(f);

    if (written != sizeof(slot)) {
        fprintf(stderr, "[Save] Incomplete write to '%s'.\n", path);
        return false;
    }

    printf("[Save] Saved state to '%s' (%s - P1: %u vs P2: %u, Time: %02u:%02u)\n",
           path, slot.slot_label, slot.p1_score, slot.p2_score,
           slot.match_seconds_remaining / 60, slot.match_seconds_remaining % 60);
    return true;
}

bool issd_load_from_slot(int slot_index) {
    char path[1024];
    GetSlotPath(slot_index, path, sizeof(path));

    if (s_load_snapshot_backend) {
        bool ok = s_load_snapshot_backend(path);
        if (ok) {
            issd_widescreen_reset();
            printf("[Save] Snapshot loaded from '%s' (Slot %d)\n", path, slot_index < 0 ? 0 : slot_index + 1);
        } else {
            printf("[Save] Snapshot load failed for '%s'\n", path);
        }
        return ok;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("[Save] Slot file '%s' not found.\n", path);
        return false;
    }

    IssdSaveSlot slot;
    size_t read_bytes = fread(&slot, 1, sizeof(slot), f);
    fclose(f);

    if (read_bytes != sizeof(slot) || slot.magic != SAVE_MAGIC || slot.version != SAVE_VERSION) {
        fprintf(stderr, "[Save] Invalid save file header in '%s'.\n", path);
        return false;
    }

    /* Restore WRAM */
    memcpy(g_ram, slot.wram, sizeof(slot.wram));
    CopyVideoState(&slot, true);
    snes_frame_counter = slot.frame_counter;
    issd_widescreen_reset();

    printf("[Save] Loaded state from '%s' (%s - P1 Score: %u, P2 Score: %u)\n",
           path, slot.slot_label, slot.p1_score, slot.p2_score);
    return true;
}

bool issd_save_quick(void) {
    return issd_save_to_slot(-1, "QuickSave");
}

bool issd_load_quick(void) {
    return issd_load_from_slot(-1);
}

bool issd_save_get_info(int slot_index, char *out_info, size_t max_len) {
    if (!out_info || max_len == 0) return false;
    char path[1024];
    GetSlotPath(slot_index, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(out_info, max_len, "Empty Slot");
        return false;
    }

    uint32_t magic = 0;
    if (fread(&magic, 1, sizeof(magic), f) != sizeof(magic)) {
        fclose(f);
        snprintf(out_info, max_len, "Corrupted Slot");
        return false;
    }
    fclose(f);

    if (magic != 0x52544c53u && magic != SAVE_MAGIC) {
        snprintf(out_info, max_len, "Corrupted Slot");
        return false;
    }

    char time_str[32] = {0};
#ifdef _WIN32
    struct _stat st;
    if (_stat(path, &st) == 0) {
#else
    struct stat st;
    if (stat(path, &st) == 0) {
#endif
        time_t t = st.st_mtime;
        struct tm *tm_info = localtime(&t);
        if (tm_info) {
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);
        }
    }

    if (slot_index < 0) {
        snprintf(out_info, max_len, "[QuickSave] (%s)", time_str[0] ? time_str : "Saved");
    } else {
        snprintf(out_info, max_len, "[Slot %d] (%s)", slot_index + 1, time_str[0] ? time_str : "Saved");
    }
    return true;
}
