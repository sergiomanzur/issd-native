#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Android storage glue. Every function is a no-op stub off Android, so callers
 * need no conditionals of their own. */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ISSD_ANDROID

/* App-private external storage: where config lives and where a user drops
 * their ROM over USB. Empty until ISSDActivity has handed it over. */
const char *issd_android_external_dir(void);

/* App-private internal storage, used for saves. */
const char *issd_android_internal_dir(void);

/* App-private external storage /mods directory. */
const char *issd_android_mods_dir(void);

/* Find a .sfc/.smc in the external directory. Android offers no file picker
 * reachable from C, so discovery replaces the desktop chooser. */
bool issd_android_find_rom(char *out, size_t out_size);

/* Open Android's document picker from the native overlay. The Java side
 * imports the result into app storage and restarts SDL when needed. */
void issd_android_pick_rom(void);
void issd_android_pick_mods_folder(void);

#else

static inline const char *issd_android_external_dir(void) { return ""; }
static inline const char *issd_android_internal_dir(void) { return ""; }
static inline const char *issd_android_mods_dir(void) { return "mods"; }
static inline bool issd_android_find_rom(char *out, size_t out_size) {
    (void)out; (void)out_size; return false;
}
static inline void issd_android_pick_rom(void) {}
static inline void issd_android_pick_mods_folder(void) {}

#endif

#ifdef __cplusplus
}
#endif
