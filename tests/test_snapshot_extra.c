#include "issd_snapshot.h"

int g_interp_apu_driving;
uint64_t g_main_cpu_cycles_estimate;
uint64_t g_apu_pace_cycles_estimate;
uint64_t g_apu_last_sync_cycles;
uint64_t g_apu_last_sync_master;
uint8_t g_memsel;
uint8_t g_snesrecomp_last_hdmaen;
static uint64_t frame_start;
static uint8_t frame_valid;
static CpuTailcallContextSave tail_context;
static unsigned cache_resets;
void rtl_apu_snapshot_pacing(uint64_t *start, uint8_t *valid) {
    *start = frame_start; *valid = frame_valid;
}
void rtl_apu_restore_pacing(uint64_t start, uint8_t valid) {
    frame_start = start; frame_valid = valid;
}
void cpu_tailcall_context_save(CpuTailcallContextSave *out) { *out = tail_context; }
void cpu_tailcall_context_restore(const CpuTailcallContextSave *in) { tail_context = *in; }
void interp_bridge_reset_dynamic_cache(void) { ++cache_resets; }

int main(void) {
    /* Keep the legacy harness hooks linked while switching to ISSD's real
     * extension. All machine component serializers above are production code. */
    (void)game;
    machine.cpu = &cpu; machine.apu = &apu; machine.dma = &dma;
    machine.ppu = &ppu; machine.cart = &cart; machine.ram = ram;
    apu.dsp = &dsp; apu.spc = &spc;
    cart.ram = sram; cart.ramSize = sizeof(sram);
    const RtlGameInfo issd = {
        .state_save_extra = issd_snapshot_save_extra,
        .state_load_extra = issd_snapshot_load_extra,
        .state_validate_extra = issd_snapshot_validate_extra,
        .on_state_loaded = issd_snapshot_on_loaded,
    };
    g_rtl_game_info = &issd;
    g_cpu.A = 0xABCD; g_cpu.X = 0x3456; g_cpu.S = 0x1af;
    g_cpu.PB = 0x80; g_cpu.DB = 0x7e; g_cpu._flag_N = 1;
    g_cpu.host_return_valid = 3;
    g_cpu.ram = ram;
    g_cpu.cycles = 7654321; g_cpu.master_cycles = 61234568;
    g_cpu.coprocessor_master_cycles = 61234000;
    snes_frame_counter = 200;
    frame_start = 61230000; frame_valid = 1;
    g_apu_last_sync_master = 61234000;
    g_apu_pace_cycles_estimate = 888;
    tail_context.valid = 1; tail_context.entry_s = 0x1aa; tail_context.hrv = 2;
    unsigned char *snapshot = malloc(1024 * 1024);
    assert(snapshot);
    size_t size = RtlSaveSnapshotToMemory(snapshot, 1024 * 1024);
    assert(size && issd_snapshot_validate_extra(snapshot + size - 128, 128, 8));
    CpuState expected_cpu = g_cpu;
    memset(&g_cpu, 0, sizeof(g_cpu));
    g_cpu.ram = ram;
    frame_start = 42; frame_valid = 0;
    snes_frame_counter = 3;
    g_apu_last_sync_master = 0; g_apu_pace_cycles_estimate = 0;
    tail_context.valid = 0;
    assert(RtlLoadSnapshotFromMemory(snapshot, size));
    assert(!memcmp(&g_cpu, &expected_cpu, sizeof(g_cpu)));
    assert(frame_start == 61230000 && frame_valid && snes_frame_counter == 200);
    assert(g_apu_last_sync_master == 61234000 && g_apu_pace_cycles_estimate == 888);
    assert(tail_context.valid && tail_context.entry_s == 0x1aa && tail_context.hrv == 2);
    assert(cache_resets == 1);
    const unsigned corrupt_offsets[] = {0, 4, 8, 31, 104, 112};
    for (size_t i = 0; i < sizeof(corrupt_offsets) / sizeof(corrupt_offsets[0]); ++i) {
        unsigned char *byte = snapshot + size - 128 + corrupt_offsets[i];
        *byte ^= 0xff;
        assert(!RtlLoadSnapshotFromMemory(snapshot, size));
        assert(!memcmp(&g_cpu, &expected_cpu, sizeof(g_cpu)) && cache_resets == 1);
        *byte ^= 0xff;
    }
    /* Every supported raw guest layout remains readable without a chunk. */
    for (uint32_t version = 4; version <= 8; ++version) {
        uint32_t hdr[2] = {RTL_SAV_MAGIC, version};
        MemorySli legacy = {{&memory_sli_func}, snapshot, 1024 * 1024, 0, true, false};
        memory_sli_func(&legacy.base, hdr, sizeof(hdr));
        snes_saveload_set_version(version);
        snes_saveload(&machine, &legacy.base);
        assert(!legacy.error);
        machine.joypad1Index = 15; machine.joypadPair2Index = 7;
        assert(RtlLoadSnapshotFromMemory(snapshot, legacy.position));
        if (version < 7) assert(machine.joypad1Index == 0);
        if (version < 8) assert(machine.joypadPair2Index == 0);
    }
    free(snapshot);
    return 0;
}
