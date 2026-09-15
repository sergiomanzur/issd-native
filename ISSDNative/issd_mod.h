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
/* The select screen offers 42 teams once the seventh group is unlocked, but
 * only 36 of them are squads. The last six - ALL STAR, EUROSTAR A and B,
 * ASIAN STAR, AFRICAN STAR and ALL AMERICAN STAR - are assembled at kick-off
 * from the group they belong to: team 36+g takes twenty players out of the
 * six rosters of group g. That was measured, not guessed - a match as ALL
 * AMERICAN STAR reads twenty eight-byte names scattered across teams 30 to
 * 35 and none from a roster of its own, and the roster pointer table's last
 * seven entries all point at the same dead address.
 *
 * So a pack can unlock them, and can give them attributes and a kit, but it
 * cannot name their players: rename the group's teams and the all-stars
 * follow. Only 0-35 are real squads. */
#define ISSD_ROM_TEAMS            42
#define ISSD_ROM_STOCK_TEAMS      36
#define ISSD_MAX_TEAMS_PER_PACK   42
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
    /* One byte in the cartridge, and neither half does what its name
     * says - the names were a guess and the guess was wrong. The top
     * half picks the palette the sprite is drawn with: 0 dark hair,
     * 1 fair, and those are the only two the cartridge uses; 2 and 3
     * select palettes the strip does not fit and turn the whole player
     * orange or green. The bottom half changes nothing in a match,
     * though the cartridge's own squads vary it from 0 to 13. Both keep
     * their names so packs already written still load. */
    uint8_t  skin_tone;    /* really hair colour: 0 dark, 1 fair */
    uint8_t  hair_style;   /* kept so a squad round-trips; unread */
    IssdPlayerAttributes attributes;
} IssdModPlayer;

typedef struct {
    uint8_t  team_id;
    /* True when this is a team to be added rather than one to replace.
     * The six cells of the seventh group are real slots the cartridge
     * already has - they are only unusable because the squad loader
     * assembles their players from their group instead of reading a
     * roster. A pack that says so gets the next of them, with a roster
     * of its own written into free cartridge space.
     *
     * `team_id` is ignored for these; `assigned_slot` is where it landed,
     * and is what the plate, the photograph and the strip are keyed on. */
    bool     new_team;
    int      assigned_slot;          /* -1 until the pack is applied */
    char     name[32];
    char     short_name[4];
    char     country_code[4];
    /* A formation name from the library in issd_formation.h - "4-4-2",
     * "4-2-3-1", "3-4-2-1" and so on. Empty leaves the team's own shape
     * alone. `tactics` is "attacking", "balanced" or "defensive" and shifts
     * that shape without changing how many players are in each line. */
    char     formation[24];
    char     tactics[16];
    /* What the select screen's plate should read. The plate is a
     * pre-rendered graphic, so the host draws over it - the same way it
     * does for stadiums. Empty leaves the cartridge's own. */
    char     plate_name[20];
    /* The squad photograph the select screen shows. The cartridge's own is
     * a compressed graphic per team, so this is drawn by the host instead:
     * a 32-bit BMP beside the pack file, any size, scaled into the 96x72
     * the frame leaves. Empty keeps the cartridge's photograph. */
    char     photo[64];
    /* Kit colours. The cartridge keeps a seventeen-colour palette per kit:
     * three shirt shades, three shorts shades, two sock shades, and skin and
     * hair it shares with everyone. A pack gives the base colour of a part
     * and the shades are derived from it, the way the cartridge shades its
     * own. 0 leaves that part alone.
     *
     * Which palette a team wears is not written down anywhere a search can
     * find it. It was measured instead, one match per team, by recording
     * every cartridge offset a run touched; kKitRecord in issd_mod_rom.c is
     * what that measured. `kit_record` overrides it for a cartridge or a
     * team the table does not cover. */
    uint32_t shirt_rgb;
    uint32_t shorts_rgb;
    uint32_t socks_rgb;
    int      kit_record;      /* -1: use the measured table */
    int      player_count;
    IssdModPlayer players[ISSD_MAX_PLAYERS_PER_TEAM];
} IssdModTeam;

/* The cartridge has eight stadiums, and like the teams they are a fixed
 * table: one can be replaced but none added. A stadium is its name, which
 * the pre-match screen prints, and the size of its pitch, which the game
 * really does play differently on - 114 by 74 yards up to 138 by 90. */
/* The cartridge ships eight. Its tables move into free space and extend -
 * see issd_mod_rom.c - and the host draws the name plate itself, so the
 * ceiling is now just how much free space the two banks have: 1187 bytes
 * in $82 at six per stadium, 1336 in $87 at seven. Thirty-two is a round
 * number well inside both and more grounds than anyone has asked for. */
#define ISSD_MAX_STADIUMS 32
#define ISSD_STOCK_STADIUMS 8

typedef struct {
    int8_t  stadium_id;      /* 0-7, or -1 for an entry with none */
    char    name[16];        /* 7 characters reach the cartridge */
    /* What the select screen's plate shows. The cartridge picks that
     * plate from a list of pre-rendered graphics by slot number, so past
     * the ninth there is nothing to show; the host draws it instead, and
     * can therefore fit a few more characters than the cartridge can. */
    char    display_name[20];
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
    /* True when the pack wants the seventh group of teams offered. */
    bool        unlock_bonus_teams;
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

/* The plate text for a stadium slot, or NULL when no enabled pack names
 * it. Later packs in the stack win, as everywhere else. */
const char *issd_mod_stadium_plate_name(int slot);

/* The plate text for a team, or NULL when no enabled pack renames it. */
const char *issd_mod_team_plate_name(int team_id);

/* How many teams the enabled packs add, and where each landed. Six is the
 * ceiling: that is how many cells the seventh group has. */
#define ISSD_MAX_ADDED_TEAMS 6
int         issd_mod_added_team_count(void);

/* The full path of the squad photograph a pack gives a team, resolved
 * against the pack's own directory. False when no enabled pack has one. */
bool        issd_mod_team_photo_path(int team_id, char *out, size_t cap);

/* The select screen's grid is not in team order: its first cell is
 * England. These read the cartridge's own cell table, so anything drawn
 * over that screen lands on the right team. -1 when it is not known. */
/* What a slot is, in team order - "Italy", "All Star". NULL past the end. */
const char *issd_mod_rom_team_name(int team);

int         issd_mod_cell_team(int cell);
int         issd_mod_team_cell(int team);

/* True when any enabled pack asks for the seventh group. */
bool        issd_mod_wants_bonus_teams(void);

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
