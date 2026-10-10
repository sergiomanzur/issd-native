/* Append to a build-local copy of audio_trace.c, then relink using the other
 * unchanged production objects. Export after main returns, when producers have
 * stopped and the SDL mutex has been destroyed. Do not call locking query APIs.
 * The static rings referenced here belong to the preceding audio_trace.c. */
static void issd_export_audio_investigation(void) {
  const char *prefix = getenv("ISSD_AUDIO_EXPORT");
  if (!prefix || !*prefix) return;
  char path[2048];
  if (snprintf(path, sizeof path, "%s-stats.json", prefix) >= (int)sizeof path) return;
  FILE *out = fopen(path, "w");
  if (!out) return;
  fprintf(out, "{\"produced\":%llu,\"dropped\":%llu,\"dropped_audible\":%llu,"
          "\"consumed\":%llu,\"underflows\":%llu,\"event_count\":%llu,"
          "\"guest_frame_sync_cycles\":%llu,\"guest_read_sync_cycles\":%llu,"
          "\"cpu_port_overwrites\":[%llu,%llu,%llu,%llu]}\n",
          (unsigned long long)s_stats.produced, (unsigned long long)s_stats.dropped,
          (unsigned long long)s_stats.dropped_audible, (unsigned long long)s_stats.consumed,
          (unsigned long long)s_stats.output_underflows, (unsigned long long)s_stats.event_count,
          (unsigned long long)s_stats.guest_frame_sync_cycles,
          (unsigned long long)s_stats.guest_read_sync_cycles,
          (unsigned long long)s_stats.cpu_port_overwrites[0],
          (unsigned long long)s_stats.cpu_port_overwrites[1],
          (unsigned long long)s_stats.cpu_port_overwrites[2],
          (unsigned long long)s_stats.cpu_port_overwrites[3]);
  fclose(out);
  if (snprintf(path, sizeof path, "%s-events.jsonl", prefix) >= (int)sizeof path) return;
  out = fopen(path, "w");
  if (!out) return;
  uint64_t first = s_stats.event_count > AUDIO_TRACE_EVENT_RING
      ? s_stats.event_count - AUDIO_TRACE_EVENT_RING : 0;
  for (uint64_t index = first; index < s_stats.event_count; ++index) {
    const AudioTraceEvent *event = &s_events[index & (AUDIO_TRACE_EVENT_RING - 1)];
    fprintf(out, "{\"index\":%llu,\"sample\":%llu,\"type\":%u,\"port\":%u,"
            "\"value\":%u,\"aux\":%u,\"producer\":%u}\n",
            (unsigned long long)index, (unsigned long long)event->sample_idx,
            event->type, event->addr, event->val, event->aux, event->producer);
  }
  fclose(out);
}
