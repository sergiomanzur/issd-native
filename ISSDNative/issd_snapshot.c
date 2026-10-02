#include "issd_snapshot.h"
#include "issd_animation.h"
#include "issd_pose_history.h"
#include "issd_widescreen.h"
#include "snes/snes.h"
#include "common_rtl.h"
#include "common_cpu_infra.h"
#include "cpu_state.h"
#include "snes/interp_bridge.h"
#include <limits.h>

extern CpuState g_cpu;
extern int g_interp_apu_driving;

/* Explicit little-endian fields; never persist CpuState's RAM pointer or ABI
 * padding. This extension is independent of the runner's v4-v8 guest schema. */
enum { CORE_SIZE = 128, EXTRA_SIZE = ISSD_SNAPSHOT_EXTRA_SIZE,
       EXTRA_MAGIC = 0x58445349, EXTRA_VERSION = 2 };
static bool presentation_loaded;
extern Snes *g_snes;
static void put(uint8_t *p, uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i) p[i] = (uint8_t)(value >> (i * 8));
}
static uint64_t get(const uint8_t *p, unsigned count) {
    uint64_t value = 0;
    for (unsigned i = 0; i < count; ++i) value |= (uint64_t)p[i] << (i * 8);
    return value;
}

void issd_snapshot_save_extra(SaveLoadInfo *sli) {
    uint8_t data[EXTRA_SIZE] = {0};
    uint64_t frame_start;
    uint8_t frame_valid;
    CpuTailcallContextSave tail;
    rtl_apu_snapshot_pacing(&frame_start, &frame_valid);
    cpu_tailcall_context_save(&tail);
    put(data, EXTRA_MAGIC, 4);
    put(data + 4, EXTRA_VERSION, 4);
    put(data + 8, EXTRA_SIZE, 4);
    put(data + 12, (uint32_t)snes_frame_counter, 4);
    put(data + 16, g_cpu.A, 2);
    put(data + 18, g_cpu.X, 2);
    put(data + 20, g_cpu.Y, 2);
    put(data + 22, g_cpu.S, 2);
    put(data + 24, g_cpu.D, 2);
    data[26] = g_cpu.DB;
    data[27] = g_cpu.PB;
    data[28] = g_cpu.host_return_valid;
    data[29] = g_cpu.P;
    data[30] = g_cpu.m_flag;
    data[31] = g_cpu.x_flag;
    data[32] = g_cpu.emulation;
    data[33] = g_cpu._flag_N;
    data[34] = g_cpu._flag_V;
    data[35] = g_cpu._flag_Z;
    data[36] = g_cpu._flag_C;
    data[37] = g_cpu._flag_I;
    data[38] = g_cpu._flag_D;
    data[39] = g_cpu.open_bus;
    put(data + 40, g_cpu.cycles, 8);
    put(data + 48, g_cpu.master_cycles, 8);
    put(data + 56, g_cpu.coprocessor_master_cycles, 8);
    put(data + 64, g_main_cpu_cycles_estimate, 8);
    put(data + 72, g_apu_pace_cycles_estimate, 8);
    put(data + 80, g_apu_last_sync_cycles, 8);
    put(data + 88, g_apu_last_sync_master, 8);
    put(data + 96, frame_start, 8);
    data[104] = frame_valid;
    data[105] = g_memsel;
    data[106] = g_snesrecomp_last_hdmaen;
    data[108] = tail.valid;
    data[109] = tail.hrv;
    put(data + 110, tail.entry_s, 2);
    issd_animation_save_state(data + CORE_SIZE);
    issd_pose_history_save_state(data + CORE_SIZE + ISSD_ANIMATION_STATE_SIZE);
    sli->func(sli, data, sizeof(data));
}

bool issd_snapshot_validate_extra(const void *blob, size_t size, uint32_t version) {
    const uint8_t *data = (const uint8_t *)blob;
    if (!data || version < 5 || size < CORE_SIZE ||
        get(data, 4) != EXTRA_MAGIC ||
        !((get(data + 4, 4) == 1 && size == CORE_SIZE) ||
          (get(data + 4, 4) == EXTRA_VERSION && size == EXTRA_SIZE)) ||
        get(data + 8, 4) != size || get(data + 12, 4) > INT_MAX)
        return false;
    if (data[28] != 0 && data[28] != 2 && data[28] != 3) return false;
    for (unsigned i = 30; i <= 38; ++i)
        if (data[i] > 1) return false;
    if (data[104] > 1 || data[105] > 1 || data[107] || data[108] > 1 ||
        (data[109] != 0 && data[109] != 2 && data[109] != 3))
        return false;
    if (data[104] && get(data + 96, 8) > get(data + 48, 8)) return false;
    for (unsigned i = 112; i < CORE_SIZE; ++i)
        if (data[i]) return false;
    if (size == EXTRA_SIZE &&
        (!issd_animation_validate_state(data + CORE_SIZE, ISSD_ANIMATION_STATE_SIZE) ||
         !issd_pose_history_validate_state(data + CORE_SIZE + ISSD_ANIMATION_STATE_SIZE, ISSD_POSE_HISTORY_STATE_SIZE)))
        return false;
    return true;
}

void issd_snapshot_load_extra(SaveLoadInfo *sli, uint32_t version) {
    uint8_t data[EXTRA_SIZE] = {0};
    sli->func(sli, data, CORE_SIZE);
    size_t size = get(data + 8, 4);
    if (size != CORE_SIZE && size != EXTRA_SIZE) return;
    if (size == EXTRA_SIZE) sli->func(sli, data + CORE_SIZE, EXTRA_SIZE - CORE_SIZE);
    /* The runner already accepted the whole chunk through pure preflight. */
    if (!issd_snapshot_validate_extra(data, size, version)) return;
    presentation_loaded = size == EXTRA_SIZE;
    if (presentation_loaded) {
        issd_animation_load_state(data + CORE_SIZE, ISSD_ANIMATION_STATE_SIZE);
        issd_pose_history_load_state(data + CORE_SIZE + ISSD_ANIMATION_STATE_SIZE, ISSD_POSE_HISTORY_STATE_SIZE);
    }
    g_cpu.A = (uint16_t)get(data + 16, 2);
    g_cpu.X = (uint16_t)get(data + 18, 2);
    g_cpu.Y = (uint16_t)get(data + 20, 2);
    g_cpu.S = (uint16_t)get(data + 22, 2);
    g_cpu.D = (uint16_t)get(data + 24, 2);
    g_cpu.DB = data[26];
    g_cpu.PB = data[27];
    g_cpu.host_return_valid = data[28];
    g_cpu.P = data[29];
    g_cpu.m_flag = data[30];
    g_cpu.x_flag = data[31];
    g_cpu.emulation = data[32];
    g_cpu._flag_N = data[33];
    g_cpu._flag_V = data[34];
    g_cpu._flag_Z = data[35];
    g_cpu._flag_C = data[36];
    g_cpu._flag_I = data[37];
    g_cpu._flag_D = data[38];
    g_cpu.open_bus = data[39];
    g_cpu.cycles = get(data + 40, 8);
    g_cpu.master_cycles = get(data + 48, 8);
    g_cpu.coprocessor_master_cycles = get(data + 56, 8);
    g_main_cpu_cycles_estimate = get(data + 64, 8);
    g_apu_pace_cycles_estimate = get(data + 72, 8);
    g_apu_last_sync_cycles = get(data + 80, 8);
    g_apu_last_sync_master = get(data + 88, 8);
    snes_frame_counter = (int)get(data + 12, 4);
    rtl_apu_restore_pacing(get(data + 96, 8), data[104]);
    g_memsel = data[105];
    g_snesrecomp_last_hdmaen = data[106];
    CpuTailcallContextSave tail = {data[108], (uint16_t)get(data + 110, 2), data[109]};
    cpu_tailcall_context_restore(&tail);
}

void issd_snapshot_on_loaded(uint32_t version) {
    (void)version;
    /* Interpreter invocations have automatic local CPU contexts; their
     * dynamic-read cache is a host optimization and must be rebuilt. Older
     * guest-only files retain their historical CPU/clock compatibility. */
    if (!presentation_loaded) {
        issd_animation_reset();
        issd_pose_history_reset();
    }
    presentation_loaded = false;
    if (g_snes) issd_widescreen_rebase(g_snes->ppu, g_snes->ram);
    interp_bridge_reset_dynamic_cache();
    g_interp_apu_driving = 0;
}
