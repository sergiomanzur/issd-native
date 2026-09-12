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
    /* A formation name from the library in issd_formation.h - "4-4-2",
     * "4-2-3-1", "3-4-2-1" and so on. Empty leaves the team's own shape
     * alone. `tactics` is "attacking", "balanced" or "defensive" and shifts
     * that shape without changing how many players are in each line. */
    char     formation[24];
    char     tactics[16];
    uint32_t home_shirt_rgb;
    uint32_t home_shorts_rgb;
    uint32_t away_shirt_rgb;
    uint32_t away_shorts_rgb;
    int      player_count;
    IssdModPlayer players[ISSD_MAX_PLAYERS_PER_TEAM];
} IssdModTeam;

/* The cartridge has eight stadiums, and like the teams they are a fixed
 * table: one can be replaced but none added. A stadium is its name, which
 * the pre-match screen prints, and the size of its pitch, which the game
 * really does play differently on - 114 by 74 yards up to 138 by 90. */
/* The cartridge ships eight, and the tables that hold them can be moved
 * into free space and extended - see issd_mod_rom.c. Sixteen is where the
 * useful part stops: the name plate on the select screen is picked by slot
 * number from a longer list the cartridge already has, and only the ninth
 * entry of that list (ALL STAR) is a finished graphic. */
#define ISSD_MAX_STADIUMS 16
#define ISSD_STOCK_STADIUMS 8

typedef struct {
    int8_t  stadium_id;      /* 0-7, or -1 for an entry with none */
    char    name[16];        /* 7 characters reach the screen */
    uint8_t pitch_length;    /* yards; 0 leaves the cartridge's own */
    uint8_t pitch_width;
} IssdModStadium;

typedef struct {
    char        name[64];
    char        author[64];
    char        version[16];
    char        description[128];
    char        filepath[256];
    bool        is_active;
    /* Packs stack. This is the order they are applied in, so when two
     * of them change the same team the higher number wins. Enabling a
     * pack puts it last, which is what 'this one on top' means. */
    int         apply_order;
    int         team_count;
    IssdModTeam teams[ISSD_MAX_TEAMS_PER_PACK];
    int         stadium_count;      /* entries in `stadiums` below */
    /* How many stadiums the game should offer at all. 0 leaves the
     * cartridge's eight; more than that relocates and extends its
     * tables. The highest value across the enabled packs wins. */
    int         stadium_slots;
    IssdModStadium stadiums[ISSD_MAX_STADIUMS];
} IssdModPack;

/* What the last application of the enabled packs actually did.
 *
 * A mod that silently does nothing is the worst outcome, so the result is
 * kept rather than only printed: the menu reports it, and a pack that
 * failed to parse or named a team that does not exist says so on screen
 * instead of only in a log nobody reads. */
typedef struct {
    int  packs_applied;
    int  teams_patched;
    int  players_patched;
    int  formations_patched;
    int  stadiums_patched;
    int  tiles_loaded;      /* filled in by the host, not the roster patcher */
    int  warnings;          /* applied, but something was ignored */
    int  errors;            /* a pack could not be used at all */
    char detail[96];        /* the first thing that went wrong */
} IssdModResult;

const IssdModResult *issd_mod_last_result(void);
void issd_mod_result_reset(void);
void issd_mod_result_note_tiles(int textures);

/* One line, for the console and the on-screen notification, which is as
 * wide as the game. */
void issd_mod_result_summary(char *out, size_t cap);

/* The same thing split to fit the menu box, which is 28 characters wide.
 * `headline` says whether it worked; `detail` says what changed. */
void issd_mod_result_lines(char *headline, size_t hcap,
                           char *detail, size_t dcap);

/* Lifecycle & Registry */
bool        issd_mod_init(void);
int         issd_mod_load_pack(const char *json_filepath);
int         issd_mod_scan_and_load(const char *mods_directory);
int         issd_mod_get_pack_count(void);
IssdModPack* issd_mod_get_pack(int index);
int         issd_mod_get_active_pack_index(void);

/* Stacking. Packs are independent switches rather than one choice. */
void        issd_mod_set_pack_enabled(int index, bool enabled);
bool        issd_mod_is_pack_enabled(int index);
int         issd_mod_enabled_count(void);

/* The enabled packs as a '|' separated list of names, in apply order, and
 * the reverse. This is what the config file stores, so the stack and its
 * order survive a restart - which is when mods actually take effect. */
void        issd_mod_enabled_list(char *out, size_t cap);
void        issd_mod_enable_from_list(const char *list);

/* Index of the pack applied `slot` places into the stack, -1 past the end. */
int         issd_mod_pack_at_order(int slot);

/* Used by the patcher to fold problems into the result the menu reports. */
void        issd_mod_result_note_error(const char *detail);
void        issd_mod_result_note_warning(const char *detail);
IssdModResult *issd_mod_result_mutable(void);
void        issd_mod_set_active_pack(int index);

/* Lookup & Runtime Patching */
const IssdModTeam* issd_mod_get_active_team(uint8_t team_id);
bool        issd_mod_apply_to_game(uint8_t team_id);
void        issd_mod_apply_match_overrides(uint8_t p1_team, uint8_t p2_team);

/* Apply every active pack to the cartridge image. Rosters live in ROM, so
 * this must run after the ROM is read and before the engine starts.
 * Returns the number of players patched. */
int         issd_mod_apply_to_rom(uint8_t *rom, size_t rom_size);

/* Remember the cartridge image and keep a pristine copy, so packs can be
 * switched at runtime. Call once, after the ROM is read. */
void        issd_mod_rom_set_image(uint8_t *rom, size_t rom_size);

/* Restore the pristine image and apply whichever pack is active. Rosters are
 * read when a match loads, so a change takes effect from the next match. */
int         issd_mod_reapply(void);

#ifdef __cplusplus
}
#endif

#endif /* ISSD_MOD_H */
