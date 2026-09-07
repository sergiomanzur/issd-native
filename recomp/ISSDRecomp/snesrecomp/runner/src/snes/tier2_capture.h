#ifndef SNESRECOMP_TIER2_CAPTURE_H
#define SNESRECOMP_TIER2_CAPTURE_H

#include <stdint.h>

const char *tier2_capture_manifest_path(const char *rom_title);
const char *tier2_capture_journal_path(const char *rom_title);

/* Append one distinct recompiler-feedback tuple as a complete JSON object and
 * flush it immediately. This is a set journal, not a hot-path event trace. */
int tier2_capture_append_discovery(const char *rom_title,
                                   uint32_t site_pc24,
                                   uint32_t target_pc24,
                                   const char *entry_mx,
                                   const char *site_kind,
                                   int outcome,
                                   int32_t frame);

void tier2_capture_close(void);

#endif /* SNESRECOMP_TIER2_CAPTURE_H */
