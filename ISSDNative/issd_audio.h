#ifndef ISSD_AUDIO_H
#define ISSD_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include "cpu_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * High-Level Emulation for ISSD Audio Subsystem.
 * Fast-paths Konami SPC700 command protocols, eliminates port polling spin loops,
 * and tracks audio telemetry.
 */

/* Entry hook for CODE_80BE95 (Sound effect FIFO queue pump, called every VBlank) */
bool Issd_HlePumpAudio(CpuState *cpu);

/* Entry hook for CODE_80BFAE (Announcer voice line playback) */
bool Issd_HlePlayVoice(CpuState *cpu);

/* Entry hook for CODE_80BEEC (Sound mute / reset) */
bool Issd_HleStopSound(CpuState *cpu);

/* Human-readable name lookup for announcer commentary voice IDs */
const char *Issd_GetVoiceName(uint16_t voice_id);

/* Human-readable name lookup for sound effect IDs */
const char *Issd_GetSfxName(uint8_t sfx_id);

/* Telemetry query for debug HUD and logging */
void Issd_AudioGetTelemetry(uint32_t *out_sfx_count, uint32_t *out_voice_count, const char **out_last_voice);

#ifdef __cplusplus
}
#endif

#endif /* ISSD_AUDIO_H */
