#ifndef ISSD_ASSET_PATH_H
#define ISSD_ASSET_PATH_H
#include <stdbool.h>
#include <stddef.h>
/* Resolve an existing regular resource within the physical pack directory.
 * Symlink/reparse targets outside that directory are rejected. */
bool issd_asset_resolve(const char *directory, const char *relative,
                        char *output, size_t output_size);
#endif
