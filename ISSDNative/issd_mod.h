#ifndef ISSD_MOD_H
#define ISSD_MOD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The cartridge stores 20 squad slots per team. This was 16, which quietly
 * left the last four players of every team unpatched. */
#define ISSD_MAX_PLAYERS_PER_TEAM 20
#define ISSD_MAX_TEAMS_PER_PACK   36
#define ISSD_MAX_MOD_PACKS        16

typedef struct {
    uint8_t acceleration;  /* 0 - 99, quantised to the cartridge 1-16 */
    uint8_t speed;         /* 0 - 99, quantised to the cartridge 1-16 */
    uint8_t shooting;      /* 0 - 99, quantised to the cartridge 1-16 */
    uint8_t technique;     /* 0 - 99, quantised to the cartridge 1-16 */
    uint8_t balance;       /* 0 - 99, quantised to the cartridge 1-16 */
    uint8_t intelligence;  /* 0 - 99, quantised to the cartridge 1-16 */
    uint8_t dribbling;     /* 0 - 99, quantised to the cartridge 1-16 */
    uint8_t jumping;       /* 0 - 99, quantised to the cartridge 1-16 */
    uint8_t stamina;       /* 0 - 99, quantised to the cartridge 1-16 */
    uint8_t goalkeeping;   /* 0 - 99, quantised to the cartridge 1-16 */
} IssdPlayerAttributes;

typedef struct {
    char     name[24];
    uint8_t  shirt_number;
    char     position[4];  /* "GK", "DF", "MF", "FW" */
    uint8_t  skin_tone;    /* 0=Light, 1=Medium, 2=Dark */
    uint8_t  hair_style;   /* 0 - 7 */
    IssdPlayerAttributes attributes;
} IssdModPlayer;

typedef struct {
    uint8_t  team_id;
    char     name[32];
    char     short_name[4];
    char     country_code[4];
    uint8_t  formation;
    uint8_t  strategy;
    uint32_t home_shirt_rgb;
    uint32_t home_shorts_rgb;
    uint32_t away_shirt_rgb;
    uint32_t away_shorts_rgb;
    int      player_count;
    IssdModPlayer players[ISSD_MAX_PLAYERS_PER_TEAM];
} IssdModTeam;

typedef struct {
    char        name[64];
    char        author[64];
    char        version[16];
    char        description[128];
    char        filepath[256];
    bool        is_active;
    int         team_count;
    IssdModTeam teams[ISSD_MAX_TEAMS_PER_PACK];
} IssdModPack;

/* Lifecycle & Registry */
bool        issd_mod_init(void);
int         issd_mod_load_pack(const char *json_filepath);
int         issd_mod_scan_and_load(const char *mods_directory);
int         issd_mod_get_pack_count(void);
IssdModPack* issd_mod_get_pack(int index);
int         issd_mod_get_active_pack_index(void);
void        issd_mod_set_active_pack(int index);

/* Lookup & Runtime Patching */
const IssdModTeam* issd_mod_get_active_team(uint8_t team_id);
bool        issd_mod_apply_to_game(uint8_t team_id);
void        issd_mod_apply_match_overrides(uint8_t p1_team, uint8_t p2_team);

/* Apply every active pack to the cartridge image. Rosters live in ROM, so
 * this must run after the ROM is read and before the engine starts.
 * Returns the number of players patched. */
int         issd_mod_apply_to_rom(uint8_t *rom, size_t rom_size);

#ifdef __cplusplus
}
#endif

#endif /* ISSD_MOD_H */
