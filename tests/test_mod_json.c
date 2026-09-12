/* A mod pack is JSON, and has to behave like it.
 *
 * The loader used to match one "key": value per line and infer structure from
 * which key it saw last. Ordinary JSON broke it in ways that never announced
 * themselves: a team object listing "name" before "team_id" put the team's
 * name on the pack and the player's name on the team, and a file printed
 * without indentation produced nothing at all. Both loaded "successfully".
 *
 * These are the shapes a generator emits without thinking about it.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "issd_mod.h"

uint8_t g_ram[0x20000];

static const IssdModPack *load(const char *dir, const char *name,
                               const char *json) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    assert(f);
    fputs(json, f);
    fclose(f);

    issd_mod_init();
    const int i = issd_mod_load_pack(path);
    return i < 0 ? NULL : issd_mod_get_pack(i);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";

    /* --- key order is not structure ----------------------------------- */
    const IssdModPack *p = load(dir, "order.json",
        "{\n"
        "  \"name\": \"Ordered\",\n"
        "  \"teams\": [\n"
        "    {\n"
        "      \"name\": \"Brazil\",\n"
        "      \"players\": [\n"
        "        { \"name\": \"PELE\", \"position\": \"FW\", \"shirt_number\": 10 }\n"
        "      ],\n"
        "      \"formation\": \"4-2-3-1\",\n"
        "      \"team_id\": 30\n"
        "    }\n"
        "  ]\n"
        "}\n");
    assert(p);
    assert(strcmp(p->name, "Ordered") == 0);
    assert(p->team_count == 1);
    assert(p->teams[0].team_id == 30);
    assert(strcmp(p->teams[0].name, "Brazil") == 0);
    assert(strcmp(p->teams[0].formation, "4-2-3-1") == 0);
    assert(p->teams[0].player_count == 1);
    assert(strcmp(p->teams[0].players[0].name, "PELE") == 0);
    assert(p->teams[0].players[0].shirt_number == 10);

    /* --- whitespace is not structure ---------------------------------- */
    p = load(dir, "compact.json",
        "{\"name\":\"Compact\",\"teams\":[{\"team_id\":31,\"name\":\"Argentina\","
        "\"players\":[{\"shirt_number\":9,\"name\":\"BATI\",\"speed\":90}]}]}");
    assert(p);
    assert(p->team_count == 1 && p->teams[0].team_id == 31);
    assert(p->teams[0].player_count == 1);
    assert(p->teams[0].players[0].attributes.speed == 90);

    /* --- unknown keys are for editors, and must not derail parsing ----- */
    p = load(dir, "extra.json",
        "{\n"
        "  \"name\": \"Extra\",\n"
        "  \"editor\": { \"grid\": true, \"palette\": [1, 2, 3] },\n"
        "  \"teams\": [\n"
        "    { \"team_id\": 32, \"kits\": { \"home\": \"#FF0000\" },\n"
        "      \"players\": [ { \"shirt_number\": 1, \"height\": 182,\n"
        "                      \"notes\": null, \"name\": \"KEEPER\" } ] }\n"
        "  ]\n"
        "}\n");
    assert(p);
    assert(p->team_count == 1 && p->teams[0].team_id == 32);
    assert(strcmp(p->teams[0].players[0].name, "KEEPER") == 0);

    /* --- a team with no team_id is dropped, not applied to team 0 ------ */
    p = load(dir, "noid.json",
        "{ \"name\": \"NoId\", \"teams\": [ { \"name\": \"Nowhere\" } ] }");
    assert(p);
    assert(p->team_count == 0 && "a team without an id must not become team 0");
    assert(issd_mod_last_result()->errors > 0);

    /* --- malformed JSON is refused, with a line number ----------------- */
    assert(load(dir, "broken.json",
                "{\n  \"name\": \"Broken\",\n  \"teams\": [ {  \n") == NULL);
    assert(issd_mod_last_result()->errors > 0);
    assert(strstr(issd_mod_last_result()->detail, "line") != NULL);

    /* --- escapes, and comments packs have always been written with ----- */
    p = load(dir, "esc.json",
        "{\n"
        "  // which cup this is\n"
        "  \"name\": \"Say \\\"hi\\\"\",\n"
        "  /* the only team */\n"
        "  \"teams\": [ { \"team_id\": 33 } ]\n"
        "}\n");
    assert(p);
    assert(strcmp(p->name, "Say \"hi\"") == 0);
    assert(p->team_count == 1 && p->teams[0].team_id == 33);

    /* --- a squad longer than the cartridge holds is truncated, not a
     *     buffer overrun ----------------------------------------------- */
    {
        char json[4096];
        int n = snprintf(json, sizeof json,
                         "{\"name\":\"Big\",\"teams\":[{\"team_id\":0,\"players\":[");
        for (int i = 0; i < 40; i++)
            n += snprintf(json + n, sizeof json - n, "%s{\"shirt_number\":%d}",
                          i ? "," : "", i + 1);
        snprintf(json + n, sizeof json - n, "]}]}");
        p = load(dir, "big.json", json);
        assert(p);
        assert(p->teams[0].player_count == ISSD_MAX_PLAYERS_PER_TEAM);
    }

    /* --- an empty teams array is honest, not a crash ------------------- */
    p = load(dir, "empty.json", "{ \"name\": \"Empty\", \"teams\": [] }");
    assert(p && p->team_count == 0);

    puts("mod json tests passed");
    return 0;
}
