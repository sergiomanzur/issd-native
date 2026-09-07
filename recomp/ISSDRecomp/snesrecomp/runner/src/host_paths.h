#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Resolve `path` against the current working directory into an absolute path.
 * This also works for paths that do not exist yet. Use it for relative
 * command-line paths before calling snesrecomp_anchor_to_exe_dir().
 * Returns 1 on success.
 */
int snesrecomp_abspath(const char *path, char *out, size_t max_len);

/*
 * Build an absolute path for `leaf` inside the executable's directory,
 * independent of the current working directory. Returns 0 when the executable
 * directory cannot be determined.
 */
int snesrecomp_exe_dir_path(const char *leaf, char *out, size_t max_len);

/*
 * Change the working directory to the executable's writable directory so
 * relative config and save paths remain stable regardless of launch context.
 * Call this before opening files. If the directory cannot be determined or is
 * not writable, the current working directory remains unchanged. Returns 1
 * when the directory was changed and 0 when it was left alone.
 */
int snesrecomp_anchor_to_exe_dir(void);

#ifdef __cplusplus
}
#endif
