"""Check production hook timing and CPU register contracts without a ROM."""
from pathlib import Path
import subprocess
from test_config_persistence import compile_c
from test_snapshot_transactional import function


def test_production_gameplay_hook_contract(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    main = (repo / "ISSDNative/main.c").read_text()
    fixture = (repo / "tests/test_gameplay_tweaks.c").read_text().split("int main(void)")[0]
    source = fixture + r'''
#include "issd_config.h"
#include <stdio.h>
#include <stdlib.h>
typedef struct { uint8_t *ram; uint16_t D, S, X, Y, A; uint8_t P; } CpuState;
IssdConfig g_issd_config;
typedef void (*TestHook)(CpuState *, uint32_t);
static TestHook native_hook;
static uint32_t opcode_pcs[3];
static unsigned opcode_count;
static void cpu_set_native_block_hook(TestHook h) { native_hook = h; }
static void interp_bridge_set_pre_opcode_hook(uint32_t pc, TestHook h) {
    if (!h) { opcode_count = 0; return; }
    assert(opcode_count < 3); opcode_pcs[opcode_count++] = pc;
}
static int g_watchdog_tripped;
static uint64_t g_gameplay_gk_calls, g_gameplay_gk_changes;
static uint64_t g_gameplay_player_calls, g_gameplay_player_changes;
static uint64_t g_gameplay_native_calls, g_gameplay_lle_calls;
static bool deadline;
bool interp_bridge_lle_master_deadline_reached(CpuState *c) { (void)c; return deadline; }
static uint16_t cpu_read16(CpuState *c, unsigned bank, uint16_t a) {
    (void)bank; return c->ram[a] | c->ram[(uint16_t)(a+1)] << 8;
}
static void cpu_write16(CpuState *c, unsigned bank, uint16_t a, uint16_t v) {
    (void)bank; c->ram[a] = (uint8_t)v; c->ram[(uint16_t)(a+1)] = (uint8_t)(v >> 8);
}
'''
    definitions = main[main.index("static void IssdGameplayKeeper("):]
    for name in ("static void IssdGameplayKeeper(", "static void IssdGameplayPlayer(", "static void IssdGameplayDecision(",
                 "static void IssdGameplayNativeBlock(", "static void IssdGameplayInterpreted(", "static void IssdConfigureGameplayHooks("):
        source += function(definitions, name)
    source += r'''
int main(void) {
    IssdConfigureGameplayHooks(); assert(!native_hook && !opcode_count);
    g_issd_config.gameplay_goalkeeper_ai = true;
    IssdConfigureGameplayHooks(); assert(native_hook && opcode_count == 1 && opcode_pcs[0] == 0x84dede);
    g_issd_config.gameplay_player_ai = true;
    IssdConfigureGameplayHooks(); assert(native_hook && opcode_count == 3);
    g_issd_config.gameplay_goalkeeper_ai = false;
    IssdConfigureGameplayHooks(); assert(native_hook && opcode_count == 2 && opcode_pcs[0] == 0x84c638 && opcode_pcs[1] == 0x84c63d);
    g_issd_config.gameplay_player_ai = false;
    IssdConfigureGameplayHooks(); assert(!native_hook && !opcode_count);
    g_issd_config.gameplay_goalkeeper_ai = true;
    g_issd_config.gameplay_player_ai = true;
    fixture(); CpuState cpu = {ram, 0x500, 0x1ac, 123, 456, 0xa55a, 0x81};
    CpuState saved = cpu; memcpy(original, ram, sizeof ram);
    deadline = true;
    IssdGameplayDecision(&cpu, 0x04dede, true);
    assert(!memcmp(original, ram, sizeof ram) && !memcmp(&saved, &cpu, sizeof cpu));
    assert(!g_gameplay_gk_calls);
    deadline = false; IssdGameplayDecision(&cpu, 0x04dede, true);
    assert(get(0x552) == 308 && g_gameplay_gk_changes == 1);
    assert(!memcmp(&saved, &cpu, sizeof cpu));

    fixture(); cpu.D = 0x600; cpu.X = 600; cpu.Y = 320; put(cpu.S+1, 0xbc40);
    saved = cpu; memcpy(original, ram, sizeof ram); deadline = true;
    IssdGameplayDecision(&cpu, 0x04c638, true);
    assert(!memcmp(original, ram, sizeof ram) && !memcmp(&saved, &cpu, sizeof cpu));
    assert(!g_gameplay_player_calls);
    deadline = false; IssdGameplayDecision(&cpu, 0x04c638, true);
    assert(cpu.X > 600 && cpu.Y > 320 && g_gameplay_player_changes == 1);
    assert(cpu.A == saved.A && cpu.P == saved.P && cpu.D == saved.D && cpu.S == saved.S);
    assert(get(0x650) == 600 && get(0x652) == 320); /* Original stores publish X/Y. */
    CpuState native = cpu; memcpy(original, ram, sizeof ram);
    fixture(); cpu = saved; put(cpu.S+1, 0xbc40);
    IssdGameplayDecision(&cpu, 0x04c638, false);
    assert(!memcmp(&native, &cpu, sizeof cpu) && !memcmp(original, ram, sizeof ram));

    fixture(); cpu = saved; put(cpu.S+1, 0xabcd); memcpy(original, ram, sizeof ram);
    IssdGameplayDecision(&cpu, 0x04c638, true);
    assert(!memcmp(&saved, &cpu, sizeof cpu) && !memcmp(original, ram, sizeof ram));
    g_issd_config.gameplay_player_ai = false; put(cpu.S+1, 0xbc40); memcpy(original, ram, sizeof ram);
    IssdGameplayDecision(&cpu, 0x04c638, true);
    assert(!memcmp(&saved, &cpu, sizeof cpu) && !memcmp(original, ram, sizeof ram));
    return 0;
}
'''
    path = tmp_path / "hooks.c"
    path.write_text(source)
    exe = tmp_path / "hooks.exe"
    compile_c(exe, repo, [path, repo / "ISSDNative/issd_gameplay.c"], [repo / "ISSDNative"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
