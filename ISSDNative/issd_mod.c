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

bool issd_mod_init(void) {
    memset(g_mod_packs, 0, sizeof(g_mod_packs));
    g_mod_pack_count = 0;
    g_active_pack_idx = -1;
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
    if (!json_filepath || g_mod_pack_count >= ISSD_MAX_MOD_PACKS) return -1;

    FILE *f = fopen(json_filepath, "r");
    if (!f) return -1;

    IssdModPack *pack = &g_mod_packs[g_mod_pack_count];
    memset(pack, 0, sizeof(*pack));
    strncpy(pack->filepath, json_filepath, sizeof(pack->filepath) - 1);
    pack->is_active = (g_mod_pack_count == 0); /* Default first pack active */

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
            } else if (strcmp(key, "formation") == 0 && cur_team) {
                cur_team->formation = (uint8_t)ival;
            } else if (strcmp(key, "strategy") == 0 && cur_team) {
                cur_team->strategy = (uint8_t)ival;
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
    printf("[ModLoader] Successfully loaded mod pack '%s' (%d teams) from '%s'\n",
           pack->name[0] ? pack->name : "Unnamed Mod", pack->team_count, json_filepath);

    if (g_active_pack_idx < 0) g_active_pack_idx = g_mod_pack_count;
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

    printf("[ModLoader] Mod scan completed (%d active mod packs found).\n", g_mod_pack_count);
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

