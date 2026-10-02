"""Existing opt-in AI diagnostic reports the decision's actual lineup and output."""
import os
import re
import struct
import subprocess
import pytest
from pathlib import Path
from test_config_persistence import compile_c


@pytest.mark.parametrize("kind", ["player", "keeper"])
def test_live_player_trace_is_read_only_and_reports_consumed_data(tmp_path, kind):
    root = Path(__file__).resolve().parents[1]
    source = (root / "ISSDNative/main.c").read_text(encoding="utf-8")
    start = source.index("static void IssdGameplayKeeper(CpuState *cpu)")
    end = source.index("static uint64_t g_bugfix_keeper_changes", start)
    peek_start = source.index("static unsigned IssdGameplayRamWord")
    peek_end = source.index("static void IssdGameplayKeeper", peek_start)
    actual_code = source[peek_start:peek_end] + source[start:end]
    harness = tmp_path / "trace.c"
    harness.write_text(f'#define TRACE_PLAYER {int(kind == "player")}\n' + r'''#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "issd_gameplay.h"
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
typedef struct { uint8_t *ram; uint16_t D; uint8_t open_bus; unsigned bus_notes; } CpuState;
static struct { int gameplay_player_ai, gameplay_goalkeeper_ai; } g_issd_config = {1,1};
static bool g_watchdog_tripped;
static uint64_t g_gameplay_player_calls, g_gameplay_player_changes;
static uint64_t g_gameplay_gk_calls, g_gameplay_gk_changes;
unsigned cpu_read16(CpuState *c, unsigned bank, unsigned a) {
  (void)bank; c->open_bus=c->ram[a+1];c->bus_notes++;
  return c->ram[a] | (unsigned)c->ram[a+1] << 8;
}
''' + actual_code + r'''
static uint8_t ram[0x20000];
static void put(unsigned a,unsigned v) { ram[a]=v;ram[a+1]=v>>8; }
int main(void) {
#ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  put(0x32,6);put(0x70,8);put(0x630,1);put(0x69a,0xd00);
  put(0x668,1);put(0x650,1000);put(0x652,300);
  put(0x12a2,2048);put(0x12a4,576);
  ram[0xda6]=2;ram[0x3f91]=12;
  ram[0x3e78]=0x37;ram[0x3e7a]=0xa0;ram[0x3e80]=1;
  ram[0xd028]=1;ram[0xd029]=(uint8_t)-3;ram[0xd294]=1;
  put(0x530,1);put(0x560,0x2000);put(0x550,16);put(0x552,288);
  put(0x42a,128);put(0x42c,288);put(0x424,(unsigned)-8);put(0x428,2);
  CpuState cpu={ram,0x1500,0x55,0};
  if (TRACE_PLAYER) {
    for (unsigned i=0;i<12;i++) IssdGameplayPlayer(&cpu);
    cpu.D=0x600;IssdGameplayPlayer(&cpu);
  } else { cpu.D=0x500;IssdGameplayKeeper(&cpu); }
  fwrite(ram,1,sizeof(ram),stdout);
  fwrite(&cpu.open_bus,1,1,stdout);fwrite(&cpu.bus_notes,sizeof(cpu.bus_notes),1,stdout);return 0;
}
''', encoding="utf-8")
    exe = tmp_path / "trace.exe"
    compile_c(exe, root, [harness, root / "ISSDNative/issd_gameplay.c"], [root / "ISSDNative"])
    quiet = subprocess.run([str(exe)], capture_output=True,
        env={k:v for k,v in os.environ.items() if k != "ISSD_GAMEPLAY_TRACE"})
    traced = subprocess.run([str(exe)], capture_output=True,
        env=dict(os.environ, ISSD_GAMEPLAY_TRACE="1"))
    assert quiet.returncode == traced.returncode == 0
    assert quiet.stdout == traced.stdout and len(traced.stdout) == 0x20005
    assert quiet.stderr == b""
    if kind == "keeper":
        assert b"[GameplayKeeper] D=0500" in traced.stderr
        assert struct.unpack_from("<H", traced.stdout, 0x552)[0] != 288
        return
    lines = traced.stderr.decode("ascii").splitlines()
    selected = [line for line in lines if "D=0600" in line]
    assert len(selected) == 1, lines
    line = selected[0]
    assert "D=0600" in line and "team=0D00 slot=1" in line
    assert "roster=12 formation=2 condition=1 role=1" in line
    assert "speed=5 inverse=8 skill=10" in line
    assert "applied=1" in line
    after = re.search(r"updated=(\d+)/(\d+)", line)
    assert after, line
    assert tuple(map(int, after.groups())) == struct.unpack_from("<HH", traced.stdout, 0x650)
