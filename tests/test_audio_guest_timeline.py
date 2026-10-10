"""Guest frame time bounds CPU execution credit during bulk sound uploads."""
import subprocess
from test_config_persistence import compile_c
from test_snapshot_transactional import function, ROOT, RUNTIME


def test_long_upload_does_not_push_following_frame_backwards(tmp_path):
    source = (RUNTIME / "common_rtl.c").read_text()
    harness = r'''
#include <assert.h>
#include <stdint.h>
#include "cpu_state.h"
CpuState g_cpu;
static int snes_frame_counter;
static uint64_t g_apu_frame_start_master;
#define RTL_APU_CYCLES_PER_FRAME 17088ull
#define RTL_MASTER_CYCLES_PER_FRAME 357368ull
'''
    checks = r'''
int main(void) {
  snes_frame_counter = 10;
  g_apu_frame_start_master = 1000;
  g_cpu.master_cycles = 179684; /* Half a frame of CPU execution. */
  assert(rtl_apu_guest_cycle() == 179424);
  g_cpu.master_cycles = 5350520; /* Bulk upload executes beyond one frame. */
  uint64_t upload_end = rtl_apu_guest_cycle();
  assert(upload_end == 187968 && "CPU credit must stop at the completed frame boundary");
  snes_frame_counter = 11;
  g_apu_frame_start_master = g_cpu.master_cycles;
  assert(rtl_apu_guest_cycle() >= upload_end && "next frame must not rewind guest audio time");
  g_cpu.master_cycles += 178684;
  assert(rtl_apu_guest_cycle() == 196512);
  return 0;
}
'''
    path = tmp_path / "timeline.c"
    path.write_text(harness + function(source, "static uint64_t rtl_apu_guest_cycle(") + checks)
    exe = tmp_path / "timeline.exe"
    compile_c(exe, ROOT, [path], [RUNTIME])
    subprocess.run([str(exe)], check=True)
