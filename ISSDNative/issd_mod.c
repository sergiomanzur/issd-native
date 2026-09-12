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

static char* TrimWhitespace(char *str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

int issd_mod_load_pack(const char *json_filepath) {
    if (!json_filepath) return -1;
    if (g_mod_pack_count >= ISSD_MAX_MOD_PACKS) {
        issd_mod_result_note_error("too many packs in mods/");
        fprintf(stderr, "[ModLoader] More than %d packs in mods/; '%s' ignored.\n",
                ISSD_MAX_MOD_PACKS, json_filepath);
        return -1;
    }

    FILE *f = fopen(json_filepath, "r");
    if (!f) {
        char why[96];
        snprintf(why, sizeof why, "cannot open %s", json_filepath);
        issd_mod_result_note_error(why);
        fprintf(stderr, "[ModLoader] %s\n", why);
        return -1;
    }

    IssdModPack *pack = &g_mod_packs[g_mod_pack_count];
    memset(pack, 0, sizeof(*pack));
    strncpy(pack->filepath, json_filepath, sizeof(pack->filepath) - 1);
    /* Off until asked for. Defaulting the first pack on meant a clean
     * install quietly ran whichever mod sorted first. */
    pack->is_active = false;

    char line[512];
    IssdModTeam *cur_team = NULL;
    IssdModPlayer *cur_player = NULL;

    while (fgets(line, sizeof(line), f)) {
        char *p = TrimWhitespace(line);
        if (!p || !*p || *p == '/' || *p == '#') continue;

        char key[64], sval[128];
        int ival = 0;

        if (sscanf(p, "\"%63[^\"]\" : \"%127[^\"]\"", key, sval) == 2 ||
            sscanf(p, "\"%63[^\"]\": \"%127[^\"]\"", key, sval) == 2 ||
            sscanf(p, "\"%63[^\"]\":\"%127[^\"]\"", key, sval) == 2) {
            
            if (strcmp(key, "name") == 0) {
                if (!cur_team) strncpy(pack->name, sval, sizeof(pack->name) - 1);
                else if (!cur_player) strncpy(cur_team->name, sval, sizeof(cur_team->name) - 1);
                else strncpy(cur_player->name, sval, sizeof(cur_player->name) - 1);
            } else if (strcmp(key, "author") == 0) {
                strncpy(pack->author, sval, sizeof(pack->author) - 1);
            } else if (strcmp(key, "version") == 0) {
                strncpy(pack->version, sval, sizeof(pack->version) - 1);
            } else if (strcmp(key, "description") == 0) {
                strncpy(pack->description, sval, sizeof(pack->description) - 1);
            } else if (strcmp(key, "short_name") == 0 && cur_team) {
                strncpy(cur_team->short_name, sval, sizeof(cur_team->short_name) - 1);
            } else if (strcmp(key, "country_code") == 0 && cur_team) {
                strncpy(cur_team->country_code, sval, sizeof(cur_team->country_code) - 1);
            } else if (strcmp(key, "position") == 0 && cur_player) {
                strncpy(cur_player->position, sval, sizeof(cur_player->position) - 1);
            } else if (strcmp(key, "formation") == 0 && cur_team && !cur_player) {
                strncpy(cur_team->formation, sval, sizeof(cur_team->formation) - 1);
            } else if ((strcmp(key, "tactics") == 0 ||
                        strcmp(key, "strategy") == 0) && cur_team && !cur_player) {
                strncpy(cur_team->tactics, sval, sizeof(cur_team->tactics) - 1);
            }
        } else if (sscanf(p, "\"%63[^\"]\" : %d", key, &ival) == 2 ||
                   sscanf(p, "\"%63[^\"]\": %d", key, &ival) == 2 ||
                   sscanf(p, "\"%63[^\"]\":%d", key, &ival) == 2) {
            
            if (strcmp(key, "team_id") == 0) {
                if (pack->team_count < ISSD_MAX_TEAMS_PER_PACK) {
                    cur_team = &pack->teams[pack->team_count++];
                    cur_team->team_id = (uint8_t)ival;
                    cur_player = NULL;
                }
            } else if (strcmp(key, "shirt_number") == 0) {
                if (cur_team && cur_team->player_count < ISSD_MAX_PLAYERS_PER_TEAM) {
                    cur_player = &cur_team->players[cur_team->player_count++];
                    cur_player->shirt_number = (uint8_t)ival;
                }
            } else if (cur_player) {
                if (strcmp(key, "acceleration") == 0) cur_player->attributes.acceleration = (uint8_t)ival;
                else if (strcmp(key, "speed") == 0) cur_player->attributes.speed = (uint8_t)ival;
                else if (strcmp(key, "shooting") == 0) cur_player->attributes.shooting = (uint8_t)ival;
                else if (strcmp(key, "technique") == 0) cur_player->attributes.technique = (uint8_t)ival;
                else if (strcmp(key, "balance") == 0) cur_player->attributes.balance = (uint8_t)ival;
                else if (strcmp(key, "intelligence") == 0) cur_player->attributes.intelligence = (uint8_t)ival;
                else if (strcmp(key, "dribbling") == 0) cur_player->attributes.dribbling = (uint8_t)ival;
                else if (strcmp(key, "jumping") == 0) cur_player->attributes.jumping = (uint8_t)ival;
                else if (strcmp(key, "stamina") == 0) cur_player->attributes.stamina = (uint8_t)ival;
                else if (strcmp(key, "goalkeeping") == 0) cur_player->attributes.goalkeeping = (uint8_t)ival;
                else if (strcmp(key, "skin_tone") == 0) cur_player->skin_tone = (uint8_t)ival;
                else if (strcmp(key, "hair_style") == 0) cur_player->hair_style = (uint8_t)ival;
            }
        }
    }

    fclose(f);
    if (pack->team_count == 0) {
        /* Every key is optional, so a file with a typo in "team_id" parses
         * happily and changes nothing. Saying so beats a silent no-op. */
        char why[96];
        snprintf(why, sizeof why, "%s has no teams",
                 pack->name[0] ? pack->name : json_filepath);
        issd_mod_result_note_error(why);
        fprintf(stderr, "[ModLoader] %s - check the team_id keys.\n", why);
    }
    printf("[ModLoader] Loaded pack '%s' (%d teams) from '%s'\n",
           pack->name[0] ? pack->name : "Unnamed Mod", pack->team_count, json_filepath);

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

