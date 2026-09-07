#ifndef RETCOMM_RBENGINE_MONO_MS_H
#define RETCOMM_RBENGINE_MONO_MS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Portable monotonic milliseconds (QPC / CLOCK_MONOTONIC). Suitable for
 * RbeSchedGates.now_ms — wrap as `return rbe_mono_ms();`. */
uint32_t rbe_mono_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* RETCOMM_RBENGINE_MONO_MS_H */
