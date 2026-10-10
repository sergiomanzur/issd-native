#ifndef ISSD_STADIUM_H
#define ISSD_STADIUM_H
#include <stdbool.h>
#include <stdint.h>

typedef struct IssdStadiumProfile {
    uint16_t version, base_layout;
    uint16_t length_units, width_units;
    uint16_t camera[5];
    uint16_t fields_present; /* Parsing presence, not gameplay identity. */
    char artwork[256];
} IssdStadiumProfile;

bool issd_stadium_rebuild_registry(void);
void issd_stadium_clear(void);
const IssdStadiumProfile *issd_stadium_profile(unsigned logical_id);
struct IssdStadiumAssets;
const struct IssdStadiumAssets *issd_stadium_assets(unsigned logical_id);
bool issd_stadium_has_profiles(void);
void issd_stadium_gameplay_digest(uint8_t out[32]);
unsigned issd_stadium_generation(void);
/* Opt-in read-only investigation; disabled without ISSD_STADIUM_TRACE. */
bool issd_stadium_trace_enabled(void);
void issd_stadium_trace_set_frame(unsigned frame);
void issd_stadium_trace_opcode(const uint8_t ram[0x20000], uint32_t pc);
void issd_stadium_trace_ppu(const uint8_t ram[0x20000]);

#endif
