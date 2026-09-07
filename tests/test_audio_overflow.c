/* test_audio_output.py supplies the production output-enqueue block. */
void audio_trace_on_sample(int16_t left, int16_t right, int dropped,
                           uint32_t fill) {
  (void)left; (void)right; (void)dropped; (void)fill;
}

int main(void) {
  Dsp dsp = {0};
  /* Ordinary delivery must retain the original PCM, including hard edges
   * in the source; only a proven dropped segment warrants smoothing. */
  for (unsigned i = 0; i < DSP_SAMPLE_RING; ++i) {
    int sample = i == 0 ? -12345 : 16000;
    queue_sample(&dsp, sample, -sample);
    if (dsp.sampleBuffer[i * 2] != sample ||
        dsp.sampleBuffer[i * 2 + 1] != -sample) return 1;
  }
  /* The producer advances during a full FIFO, dropping the intervening
   * waveform. Its next accepted sample has the opposite polarity. */
  for (int i = 0; i < 500; ++i) queue_sample(&dsp, -16000, 16000);
  if (dsp.sampleWrite != DSP_SAMPLE_RING || dsp.sampleRead != 0) return 1;
  for (unsigned i = 1; i < DSP_SAMPLE_RING; ++i)
    if (dsp.sampleBuffer[i * 2] != 16000) return 1;

  /* Free room at the oldest end. Smoothing must be attached to the actual
   * gap at the write cursor, leaving all older queued samples unchanged. */
  dsp.sampleRead = 128;
  for (int i = 0; i < 128; ++i) queue_sample(&dsp, -16000, 16000);
  int previous = 16000;
  for (unsigned i = DSP_SAMPLE_RING - 4; i < DSP_SAMPLE_RING + 128; ++i) {
    unsigned index = i & (DSP_SAMPLE_RING - 1);
    int current = dsp.sampleBuffer[index * 2];
    if (abs(current - previous) > 500) {
      fprintf(stderr, "overflow PCM gap jumps at queued sample %u: %d -> %d\n",
              i, previous, current);
      return 1;
    }
    if (dsp.sampleBuffer[index * 2 + 1] != -current) return 1;
    previous = current;
  }
  if (previous != -16000 || dsp.sampleWrite - dsp.sampleRead != DSP_SAMPLE_RING)
    return 1;
  puts("PASS: overflow seam is smoothed at FIFO boundary; ordinary PCM unchanged");
  return 0;
}
