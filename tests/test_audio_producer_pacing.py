"""Realtime producers wait outside the consumer mutex and tolerate stalls."""
import subprocess
from test_config_persistence import compile_c
from test_snapshot_transactional import function, ROOT, RUNTIME


def test_device_backpressure_releases_mutex_and_is_bounded(tmp_path):
    main = (ROOT / "ISSDNative/main.c").read_text()
    dsp = (RUNTIME / "snes/dsp.c").read_text()
    prelude = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "snes/snes.h"
static Snes machine;
static Apu apu;
static Dsp dsp;
Snes *g_snes = &machine;
static bool g_audio_producer_wait_enabled;
static bool g_audio_producer_stalled;
static uint32_t g_audio_producer_stalled_read;
static uint32_t ticks, delays;
static unsigned locked;
static bool consume;
static void RtlApuLock(void) { assert(!locked); locked = 1; }
static void RtlApuUnlock(void) { assert(locked); locked = 0; }
static uint32_t SDL_GetTicks(void) { return ticks; }
static void SDL_Delay(uint32_t ms) {
  assert(!locked && "consumer must be able to lock while producer waits");
  ticks += ms; delays++;
  if (consume) dsp.sampleRead += 32;
}
'''
    checks = r'''
int main(void) {
  assert(!g_audio_producer_stalled && !g_audio_producer_stalled_read);
  machine.apu = &apu; apu.dsp = &dsp;
  dsp.sampleWrite = 7000;
  IssdWaitForAudioProducer(); /* Headless/paused has no active consumer. */
  assert(!delays && !locked);
  g_audio_producer_wait_enabled = true; consume = true;
  IssdWaitForAudioProducer();
  assert(delays == 91 && dsp.sampleRead == 2912 && !locked);
  assert(dsp.sampleWrite == 7000 && "pacing must never discard queued PCM");
  delays = 0; consume = false; dsp.sampleRead = 0;
  ticks = UINT32_MAX - 100; /* SDL ticks wrap while a device is stalled. */
  IssdWaitForAudioProducer();
  assert(delays == 250 && !locked && dsp.sampleRead == 0);
  IssdWaitForAudioProducer();
  assert(delays == 250 && "stalled transfers must not repeat the wait for each word");
  consume = true; dsp.sampleRead = 32;
  IssdWaitForAudioProducer();
  assert(delays == 340 && dsp.sampleRead == 2912 && !locked);
  return 0;
}
'''
    source = tmp_path / "producer.c"
    source.write_text(prelude + function(dsp, "uint32_t dsp_available(") +
                      function(main, "static void IssdWaitForAudioProducer(") + checks)
    exe = tmp_path / "producer.exe"
    compile_c(exe, ROOT, [source], [RUNTIME, RUNTIME / "snes"])
    subprocess.run([str(exe)], check=True)


def test_producer_hook_skips_turbo_and_unregistered_consumers(tmp_path):
    rtl = (RUNTIME / "common_rtl.c").read_text()
    prelude = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "cpu_state.h"
CpuState g_cpu;
static bool g_audio_fast_forward;
static void (*g_audio_producer_wait)(void);
static unsigned waits;
static void wait_consumer(void) { waits++; }
'''
    checks = r'''
int main(void) {
  RtlAudioSetProducerWait(wait_consumer);
  rtl_audio_wait_for_consumer(); assert(waits == 1);
  g_audio_fast_forward = true;
  rtl_audio_wait_for_consumer(); assert(waits == 1);
  RtlAudioSetProducerWait(0); g_audio_fast_forward = false;
  rtl_audio_wait_for_consumer(); assert(waits == 1);
  return 0;
}
'''
    source = tmp_path / "hook.c"
    source.write_text(prelude + function(rtl, "void RtlAudioSetProducerWait(") +
                      function(rtl, "static void rtl_audio_wait_for_consumer(") + checks)
    exe = tmp_path / "hook.exe"
    compile_c(exe, ROOT, [source], [RUNTIME])
    subprocess.run([str(exe)], check=True)
