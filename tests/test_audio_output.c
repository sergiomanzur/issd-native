/* Exercise the production resampler with a real DSP sample ring. */
/* test_audio_output.py prepends the unmodified production resampler and
 * DSP FIFO functions, keeping this small regression independent of a ROM. */

/* Tracing is observational; the test supplies PCM directly, without a ROM. */
void audio_trace_on_consume(uint64_t base, uint32_t count, uint32_t available) {
  (void)base; (void)count; (void)available;
}
void audio_trace_on_output_underflow(uint32_t available) { (void)available; }

int main(int argc, char **argv) {
  (void)argv;
  Dsp dsp = {0};
  int16_t block[2048];
  /* A constant signal isolates amplitude discontinuities from wave phase.
   * With only 10 native pairs, starvation starts close to this block's end. */
  for (int i = 0; i < 10; ++i) {
    dsp.sampleBuffer[2 * i] = 16384;
    dsp.sampleBuffer[2 * i + 1] = -16384;
  }
  dsp.sampleWrite = 10;
  RtlSetAudioOutputRate(32040);
  rtl_render_native(&dsp, block, 16);
  int previous = block[30];
  if (previous < 8192) {
    fprintf(stderr, "fixture must finish partway through the fade: %d\n", previous);
    return 1;
  }
  if (argc > 1) {
    /* A producer can refill before the starvation fade reaches silence. */
    for (int i = 10; i < 200; ++i) {
      dsp.sampleBuffer[2 * i] = 16384;
      dsp.sampleBuffer[2 * i + 1] = -16384;
    }
    dsp.sampleWrite = 200;
  }
  rtl_render_native(&dsp, block, 16);
  if (abs(block[0] - previous) > 256) {
    fprintf(stderr, "starvation fade jumps across callbacks: %d -> %d\n",
            previous, block[0]);
    return 1;
  }
  if (argc > 1) {
    for (int n = 0; n < 4; ++n) rtl_render_native(&dsp, block, 16);
    if (block[30] != 16384 || block[31] != -16384) return 1;
    puts("PASS: recovery joins a partial fade and restores full stereo level");
    return 0;
  }
  /* The fade must progress to silence, rather than restart each callback. */
  for (int n = 0; n < 5; ++n) {
    rtl_render_native(&dsp, block, 16);
    if (block[0] > previous || block[1] != -block[0]) return 1;
    previous = block[30];
  }
  if (block[30] != 0 || block[31] != 0) return 1;
  puts("PASS: starvation fade spans callbacks and reaches stereo silence");
  return 0;
}
