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
#include "issd_bugfix_keeper.h"
#include "issd_bugfix_skills.h"
#include "issd_bugfix_goal.h"
#include "issd_bugfix_name.h"
#include <stdio.h>
#include <stdlib.h>
typedef struct { uint8_t *ram; uint16_t D, S, X, Y, A; uint8_t P, _flag_N, _flag_C; } CpuState;
IssdConfig g_issd_config;
static struct { void *ppu,*cart; } test_snes, *g_snes=&test_snes;
static uint8_t *g_rom_data;
static size_t g_rom_size;
static bool profiles,stadium_trace;
static unsigned scene_calls,transfer_calls,trace_calls;
static bool issd_stadium_has_profiles(void) { return profiles; }
static bool issd_stadium_trace_enabled(void) { return stadium_trace; }
static void issd_stadium_scene_transfer(void *ppu,uint8_t *memory,uint32_t pc) {
    (void)ppu;(void)memory;(void)pc;transfer_calls++;
}
static bool issd_stadium_scene_opcode(void *cart,uint8_t *memory,const uint8_t *rom,size_t size,uint32_t pc) {
    (void)cart;(void)memory;(void)rom;(void)size;(void)pc;scene_calls++;return true;
}
static void issd_stadium_trace_opcode(const uint8_t *memory,uint32_t pc) { (void)memory;(void)pc;trace_calls++; }
static void Die(const char *message) { (void)message;abort(); }
typedef void (*TestHook)(CpuState *, uint32_t);
static TestHook native_hook;
static uint32_t opcode_pcs[192];
static unsigned opcode_count;
static void cpu_set_native_block_hook(TestHook h) { native_hook = h; }
static void interp_bridge_set_pre_opcode_hook(uint32_t pc, TestHook h) {
    if (!h) { opcode_count = 0; return; }
    assert(opcode_count < 192); opcode_pcs[opcode_count++] = pc;
}
static int g_watchdog_tripped;
static uint64_t g_gameplay_gk_calls, g_gameplay_gk_changes;
static uint64_t g_gameplay_player_calls, g_gameplay_player_changes;
static uint64_t g_gameplay_native_calls, g_gameplay_lle_calls;
static uint64_t g_bugfix_keeper_changes, g_bugfix_skills_changes, g_bugfix_goal_changes, g_bugfix_score_changes, g_bugfix_restart_changes, g_bugfix_name_changes;
static uint32_t redirected;
static void interp_bridge_pre_opcode_redirect(uint32_t pc) { redirected = pc; }
static bool deadline;
bool interp_bridge_lle_master_deadline_reached(CpuState *c) { (void)c; return deadline; }
static uint16_t cpu_read16(CpuState *c, unsigned bank, uint16_t a) {
    (void)bank; return c->ram[a] | c->ram[(uint16_t)(a+1)] << 8;
}
static void cpu_write16(CpuState *c, unsigned bank, uint16_t a, uint16_t v) {
    (void)bank; c->ram[a] = (uint8_t)v; c->ram[(uint16_t)(a+1)] = (uint8_t)(v >> 8);
}
'''
    definitions = main[main.index("static unsigned IssdGameplayRamWord("):]
    for name in ("static unsigned IssdGameplayRamWord(", "static void IssdGameplayKeeper(", "static void IssdGameplayPlayer(", "static bool IssdBugFixDecision(", "static void IssdGameplayDecision(",
                 "static void IssdGameplayNativeBlock(", "static void IssdGameplayInterpreted(", "static void IssdConfigureGameplayHooks("):
        source += function(definitions, name)
    source += r'''
int main(void) {
    IssdConfigureGameplayHooks(); assert(!native_hook && !opcode_count);
    g_issd_config.gameplay_bug_fixes = true;
    IssdConfigureGameplayHooks(); assert(native_hook && opcode_count >= 4);
    g_issd_config.gameplay_bug_fixes = false;
    IssdConfigureGameplayHooks(); assert(!native_hook && !opcode_count);
    g_issd_config.gameplay_goalkeeper_ai = true;
    IssdConfigureGameplayHooks(); assert(native_hook && opcode_count == 1 && opcode_pcs[0] == 0x84dede);
    g_issd_config.gameplay_player_ai = true;
    IssdConfigureGameplayHooks(); assert(native_hook && opcode_count == 3);
    g_issd_config.gameplay_goalkeeper_ai = false;
    IssdConfigureGameplayHooks(); assert(native_hook && opcode_count == 2 && opcode_pcs[0] == 0x84c638 && opcode_pcs[1] == 0x84c63d);
    g_issd_config.gameplay_player_ai = false;
    IssdConfigureGameplayHooks(); assert(!native_hook && !opcode_count);
    profiles=true;IssdConfigureGameplayHooks();assert(native_hook&&opcode_count>=20);
    fixture();CpuState profile_cpu={0};profile_cpu.ram=ram;
    native_hook(&profile_cpu,0x8b8000);
    assert(scene_calls==1&&transfer_calls==1&&trace_calls==1);
    IssdGameplayInterpreted(&profile_cpu,0x8b8000);
    assert(scene_calls==2&&transfer_calls==2&&trace_calls==2);
    profiles=false;IssdConfigureGameplayHooks();assert(!native_hook&&!opcode_count);
    g_issd_config.gameplay_goalkeeper_ai = true;
    g_issd_config.gameplay_player_ai = true;
    fixture(); CpuState cpu = {ram, 0x500, 0x1ac, 123, 456, 0xa55a, 0x81, 1, 1};
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
    /* Production hooks preserve original branch flags except the rejection
     * bit, and defer all writes at native yield deadlines. */
    g_issd_config.gameplay_goalkeeper_ai = g_issd_config.gameplay_player_ai = false;
    g_issd_config.gameplay_bug_fixes = true;
    fixture(); put(0x19e0, (unsigned)-8); put(0x19e4, 0); put(0x98, 0); put(0x9a, 0); put(0x42a, (unsigned)-5);
    cpu.D = 0; cpu.P = 0xa4; cpu._flag_C = 0; saved = cpu;
    deadline = true; IssdGameplayDecision(&cpu, 0x038dab, true);
    assert(!memcmp(&cpu, &saved, sizeof cpu) && !g_bugfix_goal_changes);
    deadline = false; IssdGameplayDecision(&cpu, 0x038dab, true);
    assert(cpu.P == 0xa5 && cpu._flag_C == 1 && g_bugfix_goal_changes == 1);
    cpu = saved; IssdGameplayDecision(&cpu, 0x838dab, false);
    assert(cpu.P == 0xa5 && cpu._flag_C == 1 && g_bugfix_goal_changes == 2);
    cpu = saved; g_issd_config.gameplay_bug_fixes = false;
    IssdGameplayDecision(&cpu, 0x038dab, true); assert(!memcmp(&cpu, &saved, sizeof cpu));
    g_issd_config.gameplay_bug_fixes = true;
    put(0x14d2,0xd00); put(0xda2,100);
    IssdGameplayDecision(&cpu,0x06dbbe,true); assert(get(0xda2)==99 && g_bugfix_score_changes==1);
    assert(!memcmp(&cpu,&saved,sizeof cpu));
    /* The LLE-only name entry skips only the missing fourth caret. */
    cpu.D = 0x1500; put(0x156c,3); saved=cpu;
    IssdGameplayDecision(&cpu,0x8ab3fe,false);
    assert(redirected==0x8ab3fd && g_bugfix_name_changes==1 && !memcmp(&cpu,&saved,sizeof cpu));
    redirected=0; put(0x156c,2); IssdGameplayDecision(&cpu,0x8ab3fe,false); assert(!redirected);
    put(0x156c,3); g_issd_config.gameplay_bug_fixes=false;
    IssdGameplayDecision(&cpu,0x8ab3fe,false); assert(!redirected);
    return 0;
}
'''
    path = tmp_path / "hooks.c"
    path.write_text(source)
    exe = tmp_path / "hooks.exe"
    compile_c(exe, repo, [path, repo / "ISSDNative/issd_gameplay.c", repo / "ISSDNative/issd_bugfix_keeper.c",
              repo / "ISSDNative/issd_bugfix_skills.c", repo / "ISSDNative/issd_bugfix_goal.c", repo / "ISSDNative/issd_bugfix_name.c"], [repo / "ISSDNative"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
