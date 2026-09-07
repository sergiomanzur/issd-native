#include "recomp_net/lan_lobby.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void trim_line(char *text)
{
    size_t n;
    if (text == NULL)
    {
        return;
    }
    n = strlen(text);
    while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r'))
    {
        text[--n] = '\0';
    }
}

static void write_field(FILE *file, const char *text)
{
    const unsigned char *p = (const unsigned char *)(text != NULL ? text : "");
    while (*p != 0)
    {
        fputc((*p == '\n' || *p == '\r') ? ' ' : *p, file);
        ++p;
    }
    fputc('\n', file);
}

static int read_field(FILE *file, char *out, size_t out_size)
{
    int ch;
    if (file == NULL || out == NULL || out_size == 0 ||
        fgets(out, (int)out_size, file) == NULL)
    {
        return 0;
    }
    if (strchr(out, '\n') == NULL && !feof(file))
    {
        do
        {
            ch = fgetc(file);
        } while (ch != '\n' && ch != EOF);
    }
    trim_line(out);
    return 1;
}

static int clamp_lan_input_delay(int delay)
{
    if (delay < 2)
        return 2;
    if (delay > 20)
        return 20;
    return delay;
}

static int clamp_lan_prediction(int p)
{
    if (p < 2)
        return 2;
    if (p > 16)
        return 16;
    return p;
}

static int write_lobby(const char *path, const RNetLanLobby *lobby)
{
    FILE *file;
    int ok;
    if (path == NULL || path[0] == '\0' || lobby == NULL)
    {
        return RNET_LAN_LOBBY_ERR_IO;
    }
    file = fopen(path, "wb");
    if (file == NULL)
    {
        return RNET_LAN_LOBBY_ERR_IO;
    }
    write_field(file, "RNET_LAN_LOBBY_3");
    write_field(file, lobby->name);
    write_field(file, lobby->game);
    write_field(file, lobby->game_version);
    write_field(file, lobby->endpoint);
    write_field(file, lobby->host_name);
    write_field(file, lobby->joiner_name);
    write_field(file, lobby->password);
    ok = fprintf(file, "%d\n%d\n%d\n%d\n%d\n", lobby->started ? 1 : 0,
                 lobby->host_slot == 1 ? 1 : 0,
                 clamp_lan_input_delay(lobby->input_delay >= 2
                                           ? lobby->input_delay
                                           : 2),
                 lobby->rollback ? 1 : 0,
                 clamp_lan_prediction(lobby->input_prediction >= 2
                                          ? lobby->input_prediction
                                          : 4)) > 0;
    if (fclose(file) != 0)
    {
        ok = 0;
    }
    return ok ? RNET_LAN_LOBBY_OK : RNET_LAN_LOBBY_ERR_IO;
}

int rnet_lan_lobby_publish(const char *path, const RNetLanLobby *lobby)
{
    RNetLanLobby clean;
    if (lobby == NULL || lobby->game[0] == '\0' ||
        lobby->game_version[0] == '\0' || lobby->endpoint[0] == '\0')
    {
        return RNET_LAN_LOBBY_ERR_IDENTITY;
    }
    clean = *lobby;
    clean.started = clean.started ? 1 : 0;
    clean.host_slot = clean.host_slot == 1 ? 1 : 0;
    if (clean.input_delay < 2)
        clean.input_delay = 2;
    if (clean.input_delay > 20)
        clean.input_delay = 20;
    clean.rollback = clean.rollback ? 1 : 0;
    clean.input_prediction =
        clamp_lan_prediction(clean.input_prediction >= 2 ? clean.input_prediction
                                                         : 4);
    return write_lobby(path, &clean);
}

int rnet_lan_lobby_read(const char *path, const char *expected_game,
                        const char *expected_version, RNetLanLobby *out)
{
    FILE *file;
    char magic[32];
    char started[16];
    char host_slot[16];
    char input_delay_line[16];
    char rollback_line[16];
    char prediction_line[16];
    RNetLanLobby lobby;
    if (path == NULL || out == NULL)
    {
        return RNET_LAN_LOBBY_ERR_IO;
    }
    file = fopen(path, "rb");
    if (file == NULL)
    {
        return RNET_LAN_LOBBY_ERR_IO;
    }
    memset(&lobby, 0, sizeof(lobby));
#define READ_FIELD(field) \
    do { if (!read_field(file, (field), sizeof(field))) { \
        fclose(file); return RNET_LAN_LOBBY_ERR_IO; } } while (0)
    READ_FIELD(magic);
    READ_FIELD(lobby.name);
    READ_FIELD(lobby.game);
    READ_FIELD(lobby.game_version);
    READ_FIELD(lobby.endpoint);
    READ_FIELD(lobby.host_name);
    READ_FIELD(lobby.joiner_name);
    READ_FIELD(lobby.password);
    READ_FIELD(started);
    READ_FIELD(host_slot);
#undef READ_FIELD
    lobby.input_delay = 2;
    lobby.rollback = 0;
    lobby.input_prediction = 4;
    if (strcmp(magic, "RNET_LAN_LOBBY_2") == 0 ||
        strcmp(magic, "RNET_LAN_LOBBY_3") == 0)
    {
        if (!read_field(file, input_delay_line, sizeof(input_delay_line)))
        {
            fclose(file);
            return RNET_LAN_LOBBY_ERR_IO;
        }
        lobby.input_delay =
            clamp_lan_input_delay((int)strtol(input_delay_line, NULL, 10));
    }
    if (strcmp(magic, "RNET_LAN_LOBBY_3") == 0)
    {
        if (!read_field(file, rollback_line, sizeof(rollback_line)) ||
            !read_field(file, prediction_line, sizeof(prediction_line)))
        {
            fclose(file);
            return RNET_LAN_LOBBY_ERR_IO;
        }
        lobby.rollback = (strtol(rollback_line, NULL, 10) != 0) ? 1 : 0;
        lobby.input_prediction =
            clamp_lan_prediction((int)strtol(prediction_line, NULL, 10));
    }
    fclose(file);
    if (strcmp(magic, "RNET_LAN_LOBBY_1") != 0 &&
        strcmp(magic, "RNET_LAN_LOBBY_2") != 0 &&
        strcmp(magic, "RNET_LAN_LOBBY_3") != 0)
    {
        return RNET_LAN_LOBBY_ERR_IDENTITY;
    }
    if (lobby.endpoint[0] == '\0')
    {
        return RNET_LAN_LOBBY_ERR_IDENTITY;
    }
    if ((expected_game != NULL && expected_game[0] != '\0' &&
         strcmp(lobby.game, expected_game) != 0) ||
        (expected_version != NULL && expected_version[0] != '\0' &&
         strcmp(lobby.game_version, expected_version) != 0))
    {
        return RNET_LAN_LOBBY_ERR_IDENTITY;
    }
    lobby.started = strcmp(started, "1") == 0;
    lobby.host_slot = strcmp(host_slot, "1") == 0 ? 1 : 0;
    *out = lobby;
    return RNET_LAN_LOBBY_OK;
}

int rnet_lan_lobby_join(const char *path, const char *expected_game,
                        const char *expected_version, const char *password,
                        const char *player_name, RNetLanLobby *out)
{
    RNetLanLobby lobby;
    int rc = rnet_lan_lobby_read(path, expected_game, expected_version, &lobby);
    if (rc != RNET_LAN_LOBBY_OK)
    {
        return rc;
    }
    if (strcmp(lobby.password, password != NULL ? password : "") != 0)
    {
        return RNET_LAN_LOBBY_ERR_PASSWORD;
    }
    if (lobby.joiner_name[0] != '\0')
    {
        return RNET_LAN_LOBBY_ERR_FULL;
    }
    snprintf(lobby.joiner_name, sizeof(lobby.joiner_name), "%s",
             player_name != NULL && player_name[0] != '\0' ? player_name : "Player");
    lobby.started = 0;
    rc = write_lobby(path, &lobby);
    if (rc == RNET_LAN_LOBBY_OK && out != NULL)
    {
        *out = lobby;
    }
    return rc;
}

int rnet_lan_lobby_leave(const char *path, int is_host)
{
    RNetLanLobby lobby;
    if (path == NULL || path[0] == '\0')
    {
        return RNET_LAN_LOBBY_ERR_IO;
    }
    if (is_host)
    {
        return remove(path) == 0 ? RNET_LAN_LOBBY_OK : RNET_LAN_LOBBY_ERR_IO;
    }
    if (rnet_lan_lobby_read(path, NULL, NULL, &lobby) != RNET_LAN_LOBBY_OK)
    {
        return RNET_LAN_LOBBY_ERR_IO;
    }
    lobby.joiner_name[0] = '\0';
    lobby.started = 0;
    return write_lobby(path, &lobby);
}

int rnet_lan_lobby_kick(const char *path)
{
    /* Same seat clear as guest leave; host keeps the published room. */
    return rnet_lan_lobby_leave(path, 0);
}

int rnet_lan_lobby_set_started(const char *path, int started)
{
    RNetLanLobby lobby;
    int rc = rnet_lan_lobby_read(path, NULL, NULL, &lobby);
    if (rc != RNET_LAN_LOBBY_OK)
    {
        return rc;
    }
    lobby.started = started ? 1 : 0;
    return write_lobby(path, &lobby);
}

int rnet_lan_lobby_set_host_slot(const char *path, int host_slot)
{
    RNetLanLobby lobby;
    int rc = rnet_lan_lobby_read(path, NULL, NULL, &lobby);
    if (rc != RNET_LAN_LOBBY_OK)
    {
        return rc;
    }
    lobby.host_slot = host_slot == 1 ? 1 : 0;
    lobby.started = 0;
    return write_lobby(path, &lobby);
}
