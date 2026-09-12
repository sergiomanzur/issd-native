#include "issd_mod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

/* Global SNES WRAM access */
extern uint8_t g_ram[0x20000];

static IssdModPack g_mod_packs[ISSD_MAX_MOD_PACKS];
static int g_mod_pack_count = 0;
static int g_active_pack_idx = 0;
static int g_next_apply_order = 1;
static IssdModResult g_result;

bool issd_mod_init(void) {
    memset(g_mod_packs, 0, sizeof(g_mod_packs));
    memset(&g_result, 0, sizeof(g_result));
    g_mod_pack_count = 0;
    g_active_pack_idx = -1;
    g_next_apply_order = 1;
    return true;
}

/* ------------------------------------------------------------ parsing -- */

/* A small recursive-descent JSON reader, enough for a mod pack: objects,
 * arrays, strings, numbers, true/false/null. Values whose keys we do not
 * recognise are skipped whole, so a pack may carry anything it likes for
 * an editor's benefit without the game caring.
 *
 * It replaced a line scanner that inferred structure from key order. That
 * mattered more than it sounds: a pack whose team object listed "name"
 * before "team_id" - ordinary JSON - silently ended up with the team's
 * name on the pack and the player's name on the team. */
typedef struct {
    const char *p;
    int line;
    const char *error;   /* first failure, or NULL */
    int error_line;
} JsonReader;

static void json_fail(JsonReader *r, const char *why) {
    if (!r->error) { r->error = why; r->error_line = r->line; }
}

static void json_skip_ws(JsonReader *r) {
    for (;;) {
        const char c = *r->p;
        if (c == '\n') { r->line++; r->p++; }
        else if (c == ' ' || c == '\t' || c == '\r') r->p++;
        /* Line and block comments are not JSON, but hand-written packs
         * have always had them and the old scanner skipped them. */
        else if (c == '/' && r->p[1] == '/') { while (*r->p && *r->p != '\n') r->p++; }
        else if (c == '/' && r->p[1] == '*') {
            r->p += 2;
            while (*r->p && !(*r->p == '*' && r->p[1] == '/')) {
                if (*r->p == '\n') r->line++;
                r->p++;
            }
            if (*r->p) r->p += 2;
        }
        else return;
    }
}

static bool json_eat(JsonReader *r, char c) {
    json_skip_ws(r);
    if (*r->p != c) return false;
    r->p++;
    return true;
}

/* Reads a string into `out` when given, or skips it. Escapes are resolved
 * for the handful JSON defines; \u is accepted and becomes '?', since the
 * cartridge has no characters outside A-Z anyway. */
static bool json_string(JsonReader *r, char *out, size_t cap) {
    if (!json_eat(r, '"')) { json_fail(r, "expected a string"); return false; }
    size_t n = 0;
    while (*r->p && *r->p != '"') {
        char c = *r->p++;
        if (c == '\n') r->line++;
        if (c == '\\' && *r->p) {
            const char esc = *r->p++;
            switch (esc) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'u':
                    for (int i = 0; i < 4 && *r->p; i++) r->p++;
                    c = '?';
                    break;
                default: c = esc; break;   /* \" \\ \/ and anything else */
            }
        }
        if (out && n + 1 < cap) out[n++] = c;
    }
    if (out && cap) out[n] = '\0';
    if (*r->p != '"') { json_fail(r, "unterminated string"); return false; }
    r->p++;
    return true;
}

static bool json_number(JsonReader *r, long *out) {
    json_skip_ws(r);
    char *end = NULL;
    const double v = strtod(r->p, &end);
    if (end == r->p) { json_fail(r, "expected a number"); return false; }
    r->p = end;
    if (out) *out = (long)v;
    return true;
}

static bool json_skip_value(JsonReader *r);

/* Runs `body` for each "key": value of an object. */
typedef bool (*JsonMember)(JsonReader *r, const char *key, void *ctx);

static bool json_object(JsonReader *r, JsonMember body, void *ctx) {
    if (!json_eat(r, '{')) { json_fail(r, "expected an object"); return false; }
    json_skip_ws(r);
    if (json_eat(r, '}')) return true;
    for (;;) {
        char key[64];
        if (!json_string(r, key, sizeof key)) return false;
        if (!json_eat(r, ':')) { json_fail(r, "expected ':'"); return false; }
        if (!body(r, key, ctx)) return false;
        json_skip_ws(r);
        if (json_eat(r, ',')) { json_skip_ws(r); continue; }
        if (json_eat(r, '}')) return true;
        json_fail(r, "expected ',' or '}'");
        return false;
    }
}

/* Runs `body` for each element of an array. */
typedef bool (*JsonElement)(JsonReader *r, void *ctx);

static bool json_array(JsonReader *r, JsonElement body, void *ctx) {
    if (!json_eat(r, '[')) { json_fail(r, "expected an array"); return false; }
    json_skip_ws(r);
    if (json_eat(r, ']')) return true;
    for (;;) {
        if (!body(r, ctx)) return false;
        json_skip_ws(r);
        if (json_eat(r, ',')) { json_skip_ws(r); continue; }
        if (json_eat(r, ']')) return true;
        json_fail(r, "expected ',' or ']'");
        return false;
    }
}

static bool json_skip_member(JsonReader *r, const char *key, void *ctx) {
    (void)key; (void)ctx;
    return json_skip_value(r);
}
static bool json_skip_element(JsonReader *r, void *ctx) {
    (void)ctx;
    return json_skip_value(r);
}

static bool json_skip_value(JsonReader *r) {
    json_skip_ws(r);
    switch (*r->p) {
        case '"': return json_string(r, NULL, 0);
        case '{': return json_object(r, json_skip_member, NULL);
        case '[': return json_array(r, json_skip_element, NULL);
        case 't': r->p += 4; return true;
        case 'f': r->p += 5; return true;
        case 'n': r->p += 4; return true;
        default: return json_number(r, NULL);
    }
}

/* Convenience: read a string member straight into a fixed field. */
#define JSON_STR_FIELD(r, dst) json_string((r), (dst), sizeof(dst))

static bool json_u8(JsonReader *r, uint8_t *dst) {
    long v = 0;
    if (!json_number(r, &v)) return false;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    *dst = (uint8_t)v;
    return true;
}

/* ------------------------------------------------------- the pack shape -- */

/* A colour, written the way everyone writes colours: "#C8102E". The high
 * byte is set on anything valid so that 0 can go on meaning "not given",
 * which matters because black is a perfectly ordinary kit colour. */
static bool json_colour(JsonReader *r, uint32_t *dst) {
    char text[16];
    /* A colour that is not written as text is skipped, not fatal: one
     * mistyped field should cost that field, not the whole pack. */
    json_skip_ws(r);
    if (*r->p != '"') return json_skip_value(r);
    if (!json_string(r, text, sizeof text)) return false;
    const char *p = text;
    if (*p == '#') p++;
    uint32_t v = 0;
    int digits = 0;
    for (; *p; p++, digits++) {
        int d;
        if (*p >= '0' && *p <= '9')      d = *p - '0';
        else if (*p >= 'a' && *p <= 'f') d = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F') d = *p - 'A' + 10;
        else return true;                /* not a colour; leave it unset */
        v = (v << 4) | (uint32_t)d;
    }
    if (digits != 6) return true;
    *dst = 0xFF000000u | v;
    return true;
}

static bool player_member(JsonReader *r, const char *key, void *ctx) {
    IssdModPlayer *p = (IssdModPlayer *)ctx;
    IssdPlayerAttributes *a = &p->attributes;
    if (strcmp(key, "name") == 0)              return JSON_STR_FIELD(r, p->name);
    if (strcmp(key, "position") == 0)          return JSON_STR_FIELD(r, p->position);
    if (strcmp(key, "shirt_number") == 0)      return json_u8(r, &p->shirt_number);
    if (strcmp(key, "skin_tone") == 0)         return json_u8(r, &p->skin_tone);
    if (strcmp(key, "hair_style") == 0)        return json_u8(r, &p->hair_style);
    if (strcmp(key, "acceleration") == 0)      return json_u8(r, &a->acceleration);
    if (strcmp(key, "speed") == 0)             return json_u8(r, &a->speed);
    if (strcmp(key, "shooting") == 0)          return json_u8(r, &a->shooting);
    if (strcmp(key, "technique") == 0)         return json_u8(r, &a->technique);
    if (strcmp(key, "balance") == 0)           return json_u8(r, &a->balance);
    if (strcmp(key, "intelligence") == 0)      return json_u8(r, &a->intelligence);
    if (strcmp(key, "dribbling") == 0)         return json_u8(r, &a->dribbling);
    if (strcmp(key, "jumping") == 0)           return json_u8(r, &a->jumping);
    if (strcmp(key, "stamina") == 0)           return json_u8(r, &a->stamina);
    if (strcmp(key, "goalkeeping") == 0)       return json_u8(r, &a->goalkeeping);
    return json_skip_value(r);
}

static bool player_element(JsonReader *r, void *ctx) {
    IssdModTeam *team = (IssdModTeam *)ctx;
    if (team->player_count >= ISSD_MAX_PLAYERS_PER_TEAM) {
        /* Counted as a warning when the team is applied; here it is simply
         * read past so the rest of the file still parses. */
        return json_skip_value(r);
    }
    IssdModPlayer *p = &team->players[team->player_count];
    memset(p, 0, sizeof *p);
    if (!json_object(r, player_member, p)) return false;
    team->player_count++;
    return true;
}

static bool team_member(JsonReader *r, const char *key, void *ctx) {
    IssdModTeam *t = (IssdModTeam *)ctx;
    if (strcmp(key, "team_id") == 0)      return json_u8(r, &t->team_id);
    if (strcmp(key, "name") == 0)         return JSON_STR_FIELD(r, t->name);
    if (strcmp(key, "short_name") == 0)   return JSON_STR_FIELD(r, t->short_name);
    if (strcmp(key, "country_code") == 0) return JSON_STR_FIELD(r, t->country_code);
    if (strcmp(key, "formation") == 0)    return JSON_STR_FIELD(r, t->formation);
    if (strcmp(key, "tactics") == 0 ||
        strcmp(key, "strategy") == 0)     return JSON_STR_FIELD(r, t->tactics);
    if (strcmp(key, "plate_name") == 0)   return JSON_STR_FIELD(r, t->plate_name);
    if (strcmp(key, "photo") == 0)        return JSON_STR_FIELD(r, t->photo);
    if (strcmp(key, "new_team") == 0) {
        json_skip_ws(r);
        if (*r->p == 't' || *r->p == 'f') {
            t->new_team = (*r->p == 't');
            return json_skip_value(r);
        }
        long v = 0;
        if (!json_number(r, &v)) return false;
        t->new_team = (v != 0);
        return true;
    }
    if (strcmp(key, "shirt") == 0)        return json_colour(r, &t->shirt_rgb);
    if (strcmp(key, "shorts") == 0)       return json_colour(r, &t->shorts_rgb);
    if (strcmp(key, "socks") == 0)        return json_colour(r, &t->socks_rgb);
    if (strcmp(key, "kit_record") == 0) {
        long v = 0;
        if (!json_number(r, &v)) return false;
        t->kit_record = (int)v;
        return true;
    }
    if (strcmp(key, "players") == 0)      return json_array(r, player_element, t);
    return json_skip_value(r);
}

static bool team_element(JsonReader *r, void *ctx) {
    IssdModPack *pack = (IssdModPack *)ctx;
    if (pack->team_count >= ISSD_MAX_TEAMS_PER_PACK) return json_skip_value(r);
    IssdModTeam *t = &pack->teams[pack->team_count];
    memset(t, 0, sizeof *t);
    t->team_id = 0xFF;            /* so a missing team_id is detectable */
    t->kit_record = -1;           /* -1: whatever the measured table says */
    t->assigned_slot = -1;        /* set when the pack is applied */
    if (!json_object(r, team_member, t)) return false;
    pack->team_count++;
    return true;
}

static bool stadium_member(JsonReader *r, const char *key, void *ctx) {
    IssdModStadium *st = (IssdModStadium *)ctx;
    if (strcmp(key, "stadium_id") == 0) {
        long v = 0;
        if (!json_number(r, &v)) return false;
        st->stadium_id = (int8_t)((v < 0 || v > 127) ? -1 : v);
        return true;
    }
    if (strcmp(key, "name") == 0)         return JSON_STR_FIELD(r, st->name);
    if (strcmp(key, "display_name") == 0) return JSON_STR_FIELD(r, st->display_name);
    if (strcmp(key, "pitch_length") == 0) return json_u8(r, &st->pitch_length);
    if (strcmp(key, "pitch_width") == 0)  return json_u8(r, &st->pitch_width);
    return json_skip_value(r);
}

static bool stadium_element(JsonReader *r, void *ctx) {
    IssdModPack *pack = (IssdModPack *)ctx;
    if (pack->stadium_count >= ISSD_MAX_STADIUMS) return json_skip_value(r);
    IssdModStadium *st = &pack->stadiums[pack->stadium_count];
    memset(st, 0, sizeof *st);
    st->stadium_id = -1;            /* so a missing id is detectable */
    if (!json_object(r, stadium_member, st)) return false;
    pack->stadium_count++;
    return true;
}

static bool pack_member(JsonReader *r, const char *key, void *ctx) {
    IssdModPack *pack = (IssdModPack *)ctx;
    if (strcmp(key, "name") == 0)        return JSON_STR_FIELD(r, pack->name);
    if (strcmp(key, "author") == 0)      return JSON_STR_FIELD(r, pack->author);
    if (strcmp(key, "version") == 0)     return JSON_STR_FIELD(r, pack->version);
    if (strcmp(key, "description") == 0) return JSON_STR_FIELD(r, pack->description);
    if (strcmp(key, "teams") == 0)       return json_array(r, team_element, pack);
    if (strcmp(key, "stadiums") == 0)    return json_array(r, stadium_element, pack);
    if (strcmp(key, "unlock_bonus_teams") == 0) {
        json_skip_ws(r);
        if (*r->p == 't' || *r->p == 'f') {
            pack->unlock_bonus_teams = (*r->p == 't');
            return json_skip_value(r);
        }
        long v = 0;
        if (!json_number(r, &v)) return false;
        pack->unlock_bonus_teams = (v != 0);
        return true;
    }
    if (strcmp(key, "stadium_count") == 0) {
        long v = 0;
        if (!json_number(r, &v)) return false;
        pack->stadium_slots = (int)v;
        return true;
    }
    return json_skip_value(r);
}

/* The menu shows `detail` in a box 28 characters wide, and a full path
 * eats all of it - leaving the part that says what is wrong on the floor.
 * The path still goes to the console. */
static const char *base_name(const char *path) {
    const char *base = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;
    return base;
}

int issd_mod_load_pack(const char *json_filepath) {
    if (!json_filepath) return -1;
    if (g_mod_pack_count >= ISSD_MAX_MOD_PACKS) {
        issd_mod_result_note_error("too many packs in mods/");
        fprintf(stderr, "[ModLoader] More than %d packs in mods/; '%s' ignored.\n",
                ISSD_MAX_MOD_PACKS, json_filepath);
        return -1;
    }

    FILE *f = fopen(json_filepath, "rb");
    if (!f) {
        char why[96];
        snprintf(why, sizeof why, "cannot open %s", base_name(json_filepath));
        issd_mod_result_note_error(why);
        fprintf(stderr, "[ModLoader] %s\n", why);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) size = 0;
    char *text = (char *)malloc((size_t)size + 1);
    if (!text) { fclose(f); return -1; }
    const size_t got = fread(text, 1, (size_t)size, f);
    text[got] = '\0';
    fclose(f);

    IssdModPack *pack = &g_mod_packs[g_mod_pack_count];
    memset(pack, 0, sizeof(*pack));
    strncpy(pack->filepath, json_filepath, sizeof(pack->filepath) - 1);
    /* Off until asked for. Defaulting the first pack on meant a clean
     * install quietly ran whichever mod sorted first. */
    pack->is_active = false;

    JsonReader r = { text, 1, NULL, 0 };
    const bool ok = json_object(&r, pack_member, pack);
    free(text);

    if (!ok || r.error) {
        char why[96];
        snprintf(why, sizeof why, "%s line %d: %s", base_name(json_filepath),
                 r.error_line, r.error ? r.error : "malformed");
        issd_mod_result_note_error(why);
        fprintf(stderr, "[ModLoader] %s\n", why);
        return -1;
    }

    /* A team with no team_id names nothing, and would silently rewrite team
     * 0 if it defaulted to zero. Drop it and say so. An added team is the
     * exception: it is given a slot when the stack is applied, so naming
     * one here would be naming a slot it may not get. */
    int kept = 0;
    for (int i = 0; i < pack->team_count; i++) {
        if (pack->teams[i].team_id == 0xFF && !pack->teams[i].new_team) {
            char why[96];
            snprintf(why, sizeof why, "%s: a team has no team_id",
                     pack->name[0] ? pack->name : base_name(json_filepath));
            issd_mod_result_note_error(why);
            fprintf(stderr, "[ModLoader] %s\n", why);
            continue;
        }
        if (kept != i) pack->teams[kept] = pack->teams[i];
        kept++;
    }
    pack->team_count = kept;

    if (pack->team_count == 0 && pack->stadium_count == 0) {
        char why[96];
        snprintf(why, sizeof why, "%s changes nothing",
                 pack->name[0] ? pack->name : base_name(json_filepath));
        issd_mod_result_note_error(why);
        fprintf(stderr, "[ModLoader] %s - is the \"teams\" or \"stadiums\""
                        " array there?\n", why);
    }
    printf("[ModLoader] Loaded pack '%s' (%d teams, %d stadiums) from '%s'\n",
           pack->name[0] ? pack->name : "Unnamed Mod", pack->team_count,
           pack->stadium_count, json_filepath);

    g_mod_pack_count++;
    return g_mod_pack_count - 1;
}

int issd_mod_scan_and_load(const char *mods_directory) {
    const char *dir = (mods_directory && mods_directory[0]) ? mods_directory : "mods";
    printf("[ModLoader] Scanning for mod packages in '%s'...\n", dir);

#ifdef _WIN32
    char search_pattern[300];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*.json", dir);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char full_path[350];
                snprintf(full_path, sizeof(full_path), "%s\\%s", dir, fd.cFileName);
                issd_mod_load_pack(full_path);
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
#else
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *dir_entry;
        while ((dir_entry = readdir(d)) != NULL) {
            char *dot = strrchr(dir_entry->d_name, '.');
            if (dot && strcmp(dot, ".json") == 0) {
                char full_path[350];
                snprintf(full_path, sizeof(full_path), "%s/%s", dir, dir_entry->d_name);
                issd_mod_load_pack(full_path);
            }
        }
        closedir(d);
    }
#endif

    printf("[ModLoader] %d pack(s) available.\n", g_mod_pack_count);
    return g_mod_pack_count;
}

int issd_mod_get_pack_count(void) {
    return g_mod_pack_count;
}

IssdModPack* issd_mod_get_pack(int index) {
    if (index >= 0 && index < g_mod_pack_count) {
        return &g_mod_packs[index];
    }
    return NULL;
}

int issd_mod_get_active_pack_index(void) {
    return g_active_pack_idx;
}

/* ------------------------------------------------------------- stacking -- */

void issd_mod_set_pack_enabled(int index, bool enabled) {
    if (index < 0 || index >= g_mod_pack_count) return;
    IssdModPack *pack = &g_mod_packs[index];
    if (pack->is_active == enabled) return;
    pack->is_active = enabled;
    /* Enabling puts a pack on top of the stack. Turning one off and on again
     * is therefore how a modder says "let this one win". */
    pack->apply_order = enabled ? g_next_apply_order++ : 0;

    g_active_pack_idx = -1;
    for (int i = 0; i < g_mod_pack_count; i++)
        if (g_mod_packs[i].is_active) { g_active_pack_idx = i; break; }
}

bool issd_mod_is_pack_enabled(int index) {
    return (index >= 0 && index < g_mod_pack_count) && g_mod_packs[index].is_active;
}

int issd_mod_enabled_count(void) {
    int n = 0;
    for (int i = 0; i < g_mod_pack_count; i++) if (g_mod_packs[i].is_active) n++;
    return n;
}

/* Index of the pack that should be applied `slot` places into the stack, or
 * -1 when the stack is shorter than that. Selection sort over at most sixteen
 * packs: simpler to read than maintaining a parallel ordered array, and this
 * runs once per restart. */
int issd_mod_pack_at_order(int slot) {
    int best = -1, seen = 0;
    for (;;) {
        int next = -1;
        for (int i = 0; i < g_mod_pack_count; i++) {
            if (!g_mod_packs[i].is_active) continue;
            if (g_mod_packs[i].apply_order <= best) continue;
            if (next < 0 || g_mod_packs[i].apply_order < g_mod_packs[next].apply_order)
                next = i;
        }
        if (next < 0) return -1;
        if (seen == slot) return next;
        best = g_mod_packs[next].apply_order;
        seen++;
    }
}

void issd_mod_enabled_list(char *out, size_t cap) {
    if (!out || !cap) return;
    out[0] = '\0';
    size_t used = 0;
    for (int slot = 0; ; slot++) {
        const int idx = issd_mod_pack_at_order(slot);
        if (idx < 0) break;
        const char *name = g_mod_packs[idx].name;
        if (!name[0]) continue;
        const size_t need = strlen(name) + (used ? 1u : 0u);
        if (used + need >= cap) break;
        if (used) out[used++] = '|';
        memcpy(out + used, name, strlen(name));
        used += strlen(name);
        out[used] = '\0';
    }
}

void issd_mod_enable_from_list(const char *list) {
    for (int i = 0; i < g_mod_pack_count; i++) {
        g_mod_packs[i].is_active = false;
        g_mod_packs[i].apply_order = 0;
    }
    g_next_apply_order = 1;
    g_active_pack_idx = -1;
    if (!list || !list[0]) return;

    /* The list is in apply order, so walking it left to right rebuilds the
     * same stack the player saw before the restart. */
    const char *p = list;
    while (*p) {
        const char *end = strchr(p, '|');
        const size_t len = end ? (size_t)(end - p) : strlen(p);
        for (int i = 0; i < g_mod_pack_count; i++) {
            if (strlen(g_mod_packs[i].name) == len &&
                strncmp(g_mod_packs[i].name, p, len) == 0) {
                issd_mod_set_pack_enabled(i, true);
                break;
            }
        }
        if (!end) break;
        p = end + 1;
    }
}

/* --------------------------------------------------------------- result -- */

static IssdModResult g_result_public;

bool issd_mod_team_photo_path(int team_id, char *out, size_t cap) {
    bool found = false;
    for (int i = 0; ; i++) {
        const int pi = issd_mod_pack_at_order(i);
        if (pi < 0) break;
        const IssdModPack *pack = issd_mod_get_pack(pi);
        if (!pack) continue;
        for (int k = 0; k < pack->team_count; k++) {
            const IssdModTeam *t = &pack->teams[k];
            const int slot = t->new_team ? t->assigned_slot : (int)t->team_id;
            if (slot != team_id) continue;
            if (!t->photo[0]) continue;
            /* The photograph sits beside the pack that names it, so a
             * pack is one folder a player can move around. */
            char dir[256];
            snprintf(dir, sizeof dir, "%s", pack->filepath);
            char *cut = strrchr(dir, '/');
            char *alt = strrchr(dir, '\\');
            if (alt && (!cut || alt > cut)) cut = alt;
            if (cut) *cut = '\0'; else dir[0] = '\0';
            if (dir[0]) snprintf(out, cap, "%s/%s", dir, t->photo);
            else        snprintf(out, cap, "%s", t->photo);
            found = true;
        }
    }
    return found;
}

const char *issd_mod_team_plate_name(int team_id) {
    const char *found = NULL;
    for (int i = 0; ; i++) {
        const int pi = issd_mod_pack_at_order(i);
        if (pi < 0) break;
        const IssdModPack *pack = issd_mod_get_pack(pi);
        if (!pack) continue;
        for (int k = 0; k < pack->team_count; k++) {
            const IssdModTeam *t = &pack->teams[k];
            const int slot = t->new_team ? t->assigned_slot : (int)t->team_id;
            if (slot == team_id && t->plate_name[0]) found = t->plate_name;
        }
    }
    return found;
}

int issd_mod_added_team_count(void) {
    int n = 0;
    for (int i = 0; ; i++) {
        const int pi = issd_mod_pack_at_order(i);
        if (pi < 0) break;
        const IssdModPack *pack = issd_mod_get_pack(pi);
        if (!pack) continue;
        for (int k = 0; k < pack->team_count; k++)
            if (pack->teams[k].new_team) n++;
    }
    return n;
}

bool issd_mod_wants_bonus_teams(void) {
    for (int i = 0; ; i++) {
        const int pi = issd_mod_pack_at_order(i);
        if (pi < 0) break;
        const IssdModPack *pack = issd_mod_get_pack(pi);
        if (!pack) continue;
        /* A pack that edits one of the six implies it wants them offered. */
        if (pack->unlock_bonus_teams) return true;
        for (int k = 0; k < pack->team_count; k++)
            if (pack->teams[k].new_team) return true;
        for (int k = 0; k < pack->team_count; k++)
            if (pack->teams[k].team_id >= ISSD_ROM_STOCK_TEAMS) return true;
    }
    return false;
}

const char *issd_mod_stadium_plate_name(int slot) {
    const char *found = NULL;
    for (int i = 0; ; i++) {
        const int pi = issd_mod_pack_at_order(i);
        if (pi < 0) break;
        const IssdModPack *pack = issd_mod_get_pack(pi);
        if (!pack) continue;
        for (int k = 0; k < pack->stadium_count; k++) {
            const IssdModStadium *st = &pack->stadiums[k];
            if (st->stadium_id != slot) continue;
            if (st->display_name[0]) found = st->display_name;
            else if (st->name[0]) found = st->name;
        }
    }
    return found;
}

const IssdModResult *issd_mod_last_result(void) {
    g_result_public = g_result;
    return &g_result_public;
}

/* Re-applying recounts what the current stack does, but a pack that failed
 * to parse stays broken until the file is fixed and the game restarted, so
 * errors survive. Clearing them here made a broken pack look fine the
 * moment anything else was toggled. */
void issd_mod_result_reset(void) {
    const int errors = g_result.errors;
    char detail[sizeof g_result.detail];
    memcpy(detail, g_result.detail, sizeof detail);
    memset(&g_result, 0, sizeof(g_result));
    g_result.errors = errors;
    memcpy(g_result.detail, detail, sizeof detail);
}

void issd_mod_result_note_tiles(int textures) { g_result.tiles_loaded = textures; }

void issd_mod_result_note_error(const char *detail) {
    g_result.errors++;
    if (!g_result.detail[0] && detail)
        snprintf(g_result.detail, sizeof(g_result.detail), "%s", detail);
}

void issd_mod_result_note_warning(const char *detail) {
    g_result.warnings++;
    if (!g_result.detail[0] && detail)
        snprintf(g_result.detail, sizeof(g_result.detail), "%s", detail);
}

IssdModResult *issd_mod_result_mutable(void) { return &g_result; }

/* The menu box fits 28 characters, and a summary that runs off the edge is
 * worse than no summary: it is the half that got cut which usually says
 * what went wrong. Two short lines instead of one long one. */
void issd_mod_result_lines(char *headline, size_t hcap,
                           char *detail, size_t dcap) {
    const IssdModResult *r = &g_result;
    if (headline && hcap) headline[0] = 0;
    if (detail && dcap) detail[0] = 0;

    if (r->errors) {
        snprintf(headline, hcap, "%d PACK FAILED", r->errors);
        snprintf(detail, dcap, "%.27s", r->detail);
        return;
    }
    if (!r->packs_applied && !r->tiles_loaded) {
        snprintf(headline, hcap, "Vanilla - nothing enabled");
        return;
    }
    if (r->warnings)
        snprintf(headline, hcap, "Applied, %d warning%s", r->warnings,
                 r->warnings == 1 ? "" : "s");
    else
        snprintf(headline, hcap, "Applied cleanly");

    char counts[64];
    int n = snprintf(counts, sizeof counts, "%dp", r->packs_applied);
    if (r->players_patched)
        n += snprintf(counts + n, sizeof counts - n, " %dplr", r->players_patched);
    if (r->formations_patched)
        n += snprintf(counts + n, sizeof counts - n, " %dfrm", r->formations_patched);
    if (r->stadiums_patched)
        n += snprintf(counts + n, sizeof counts - n, " %dstad", r->stadiums_patched);
    if (r->tiles_loaded)
        snprintf(counts + n, sizeof counts - n, " %dtile", r->tiles_loaded);
    snprintf(detail, dcap, "%.27s", counts);
}

void issd_mod_result_summary(char *out, size_t cap) {
    if (!out || !cap) return;
    const IssdModResult *r = &g_result;
    if (r->errors) {
        snprintf(out, cap, "Mods: %d FAILED - %s", r->errors,
                 r->detail[0] ? r->detail : "see the console");
        return;
    }
    if (!r->packs_applied && !r->tiles_loaded) {
        snprintf(out, cap, "Mods: none enabled (vanilla)");
        return;
    }
    /* Say what changed, not merely that something did: "applied" with no
     * numbers is indistinguishable from a mod that quietly did nothing. */
    char body[80];
    int n = snprintf(body, sizeof(body), "%d pack%s", r->packs_applied,
                     r->packs_applied == 1 ? "" : "s");
    if (r->players_patched)
        n += snprintf(body + n, sizeof(body) - n, ", %d players", r->players_patched);
    if (r->formations_patched)
        n += snprintf(body + n, sizeof(body) - n, ", %d shape%s",
                      r->formations_patched,
                      r->formations_patched == 1 ? "" : "s");
    if (r->stadiums_patched)
        n += snprintf(body + n, sizeof(body) - n, ", %d stadium%s",
                      r->stadiums_patched,
                      r->stadiums_patched == 1 ? "" : "s");
    if (r->tiles_loaded)
        snprintf(body + n, sizeof(body) - n, ", %d tiles", r->tiles_loaded);

    if (r->warnings)
        snprintf(out, cap, "Mods: %s (%d warning%s)", body, r->warnings,
                 r->warnings == 1 ? "" : "s");
    else
        snprintf(out, cap, "Mods applied: %s", body);
}

void issd_mod_set_active_pack(int index) {
    if (index >= 0 && index < g_mod_pack_count) {
        g_active_pack_idx = index;
        for (int i = 0; i < g_mod_pack_count; i++) {
            g_mod_packs[i].is_active = (i == index);
        }
        printf("[ModLoader] Active mod pack set to: '%s'\n", g_mod_packs[index].name);
    } else if (index == -1) {
        g_active_pack_idx = -1;
        for (int i = 0; i < g_mod_pack_count; i++) {
            g_mod_packs[i].is_active = false;
        }
        printf("[ModLoader] Active mod pack set to: NONE (Vanilla SNES)\n");
    }
}

const IssdModTeam* issd_mod_get_active_team(uint8_t team_id) {
    if (g_active_pack_idx < 0 || g_active_pack_idx >= g_mod_pack_count) return NULL;
    const IssdModPack *pack = &g_mod_packs[g_active_pack_idx];
    for (int i = 0; i < pack->team_count; i++) {
        if (pack->teams[i].team_id == team_id) {
            return &pack->teams[i];
        }
    }
    return NULL;
}

bool issd_mod_apply_to_game(uint8_t team_id) {
    const IssdModTeam *team = issd_mod_get_active_team(team_id);
    if (!team) return false;

    printf("[ModLoader] Injected custom team '%s' (ID %u, %d players)\n",
           team->name, team_id, team->player_count);
    return true;
}

void issd_mod_apply_match_overrides(uint8_t p1_team, uint8_t p2_team) {
    if (g_active_pack_idx < 0) return;

    const IssdModTeam *t1 = issd_mod_get_active_team(p1_team);
    if (t1) {
        printf("[ModLoader] Injecting P1 Roster: %s (%s)\n", t1->name, t1->country_code);
    }

    const IssdModTeam *t2 = issd_mod_get_active_team(p2_team);
    if (t2) {
        printf("[ModLoader] Injecting P2 Roster: %s (%s)\n", t2->name, t2->country_code);
    }
}

