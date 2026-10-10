"""Exercise the production reset with native SPC and host-player backends."""
import subprocess
from test_config_persistence import compile_c
from test_snapshot_transactional import ROOT, RUNTIME, function


def test_native_apu_reset_retains_sram_and_resets_pacing(tmp_path):
    rtl = (RUNTIME / "common_rtl.c").read_text()
    harness = r"""
#include <assert.h>
#include "common_rtl.h"
#include "common_cpu_infra.h"
#include "cpu_state.h"
#include "snes/snes.h"
#include "spc_player.h"
static Snes machine;
static Ppu ppu;
static uint8 sram[32];
Snes *g_snes = &machine;
Ppu *g_ppu = &ppu;
CpuState g_cpu;
uint8 *g_sram = sram;
int g_sram_size = sizeof sram;
SpcPlayer *g_spc_player;
int snes_frame_counter;
uint64_t g_main_cpu_cycles_estimate, g_apu_pace_cycles_estimate;
uint64_t g_apu_last_sync_cycles, g_apu_last_sync_master;
static bool g_apu_frame_time_valid, g_audio_fast_forward;
static uint64_t g_apu_frame_start_master;
static uint32_t g_audio_recovery_frames, g_audio_recovery_remaining;
static int16 g_audio_recovery_anchor_l, g_audio_recovery_anchor_r;
static int16 g_audio_last_output_l, g_audio_last_output_r;
static unsigned reset_calls, native_calls, player_calls, locks;
void snes_reset(Snes *snes, bool hard) {
    assert(snes == &machine && hard); reset_calls++;
}
void SnesEnterNativeMode(void) { native_calls++; }
void ppu_reset(Ppu *p) { assert(p == &ppu); }
void RtlApuLock(void) { locks++; }
void RtlApuUnlock(void) { assert(locks); locks--; }
static void initialize_player(SpcPlayer *p) { assert(p == g_spc_player); player_calls++; }
"""
    checks = r"""
int main(void) {
    memset(sram, 0x5a, sizeof sram);
    g_cpu.master_cycles = 1234567;
    snes_frame_counter = 123;
    g_apu_frame_time_valid = true;
    g_main_cpu_cycles_estimate = g_apu_pace_cycles_estimate = 42;
    g_apu_last_sync_cycles = g_apu_last_sync_master = 99;
    RtlReset(1); /* Native SPC has no host player. */
    assert(!snes_frame_counter && !g_apu_frame_time_valid && !locks);
    assert(g_apu_frame_start_master == 1234567 && machine.beamMasterLast == 1234567);
    assert(g_apu_last_sync_master == 1234567 && !g_apu_last_sync_cycles);
    assert(!g_main_cpu_cycles_estimate && !g_apu_pace_cycles_estimate);
    for (unsigned i=0; i<sizeof sram; i++) assert(sram[i] == 0x5a);
    SpcPlayer player = {0}; player.initialize = initialize_player;
    g_spc_player = &player;
    RtlReset(1); /* Existing host-player initialization remains intact. */
    assert(player_calls == 1 && reset_calls == 2 && native_calls == 2 && !locks);
    for (unsigned i=0; i<sizeof sram; i++) assert(sram[i] == 0x5a);
    RtlReset(0); /* The explicit non-preserving reset still clears SRAM. */
    for (unsigned i=0; i<sizeof sram; i++) assert(sram[i] == 0);
    return 0;
}
"""
    source = tmp_path / "reset.c"
    source.write_text(harness + function(rtl, "void RtlReset(") + checks)
    exe = tmp_path / "reset.exe"
    compile_c(exe, ROOT, [source], [RUNTIME, RUNTIME / "snes"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
