"""Exercise diagnostic stop decisions against ordinary final-menu input."""
from pathlib import Path
import shutil
import subprocess

import pytest


def test_normal_ceremony_advances_and_only_decisive_shootout_stops(tmp_path):
    root = Path(__file__).resolve().parents[1]
    clang = shutil.which("clang")
    if not clang:
        pytest.skip("clang required for diagnostic C controller harness")
    driver = (root / "tools/ghidra/campaign_probe_input.c").as_posix()
    harness = tmp_path / "terminal.c"
    harness.write_text(f'#include "{driver}"\n' + r'''
#include <assert.h>
#include <string.h>
uint8_t g_ram[0x20000];
static unsigned finished;
void issd_probe_finish(unsigned frame) { finished=frame; }
static void put(unsigned a, unsigned v) { g_ram[a]=v; g_ram[a+1]=v>>8; }
static void setup(unsigned period) {
  memset(g_ram,0,sizeof g_ram); terminal_frame=0; finished=0;
  last_mode=~0u; last_period=~0u; last_cb=~0u;
  seen_live=true; seen_elimination=false; world=0;
  put(0x70,12); put(0xa8,period); put(0x32,6);
  put(0x1446,0xc8c0); g_ram[0x1448]=0x8b;
  put(0x1640,9); put(0x1648,8); put(0xda0,60); put(0xea0,40);
  put(0xddce,60); put(0x1700,0);
}
static void advances(void) {
  assert(issd_script_mask(4320)==ISSD_BTN_A);
  assert(issd_script_mask(4480)==ISSD_BTN_A);
  assert(!finished);
}
int main(void) {
  trace=tmpfile(); controller_trace=tmpfile(); assert(trace && controller_trace);
  setup(1); advances();
  setup(3); advances(); /* Extra time alone does not prove penalties. */
  setup(3); g_ram[0xd442]=5; g_ram[0xd443]=4; put(0x1700,2); advances();
  setup(3); g_ram[0xd442]=5; g_ram[0xd443]=4; put(0xddce,40); advances();
  setup(3); g_ram[0xd442]=5; g_ram[0xd443]=4; put(0x1648,0); advances();
  setup(3); g_ram[0xd442]=5; g_ram[0xd443]=4;
  assert(issd_script_mask(4320)==0); assert(issd_script_mask(4480)==0);
  assert(finished==4480);
  setup(1); put(0x1446,0xd32e); g_ram[0x1448]=0x85;
  assert(issd_script_mask(4320)==0); issd_script_mask(4480); assert(finished==4480);
  return 0;
}
''')
    executable = tmp_path / "terminal.exe"
    subprocess.run([clang, "-I", str(root / "ISSDNative"), str(harness), "-o", str(executable)], check=True, capture_output=True)
    subprocess.run([str(executable)], check=True, capture_output=True, cwd=tmp_path)
