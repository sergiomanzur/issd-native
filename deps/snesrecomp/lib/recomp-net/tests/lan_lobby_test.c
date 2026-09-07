#include "recomp_net/lan_lobby.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

int main(void)
{
    const char *path = "rnet_lan_lobby_test.tmp";
    RNetLanLobby room;
    RNetLanLobby got;
    remove(path);
    memset(&room, 0, sizeof(room));
    snprintf(room.name, sizeof(room.name), "Test Room");
    snprintf(room.game, sizeof(room.game), "Metal Warriors");
    snprintf(room.game_version, sizeof(room.game_version), "0.1.0");
    snprintf(room.endpoint, sizeof(room.endpoint), "192.168.1.20:7777");
    snprintf(room.host_name, sizeof(room.host_name), "Host");
    snprintf(room.password, sizeof(room.password), "secret");
    room.input_delay = 5;
    room.rollback = 1;
    room.input_prediction = 7;

    expect(rnet_lan_lobby_publish(path, &room) == RNET_LAN_LOBBY_OK,
           "publish room");
    expect(rnet_lan_lobby_read(path, "Other Game", "0.1.0", &got) ==
               RNET_LAN_LOBBY_ERR_IDENTITY,
           "reject wrong game");
    expect(rnet_lan_lobby_join(path, "Metal Warriors", "0.1.0", "wrong",
                               "Guest", &got) == RNET_LAN_LOBBY_ERR_PASSWORD,
           "reject wrong password");
    expect(rnet_lan_lobby_join(path, "Metal Warriors", "0.1.0", "secret",
                               "Guest", &got) == RNET_LAN_LOBBY_OK,
           "guest joins");
    expect(strcmp(got.joiner_name, "Guest") == 0 && !got.started,
           "joined room remains waiting");
    expect(rnet_lan_lobby_join(path, "Metal Warriors", "0.1.0", "secret",
                               "Third", NULL) == RNET_LAN_LOBBY_ERR_FULL,
           "reject full room");
    expect(rnet_lan_lobby_set_started(path, 1) == RNET_LAN_LOBBY_OK,
           "host starts room");
    expect(rnet_lan_lobby_read(path, "Metal Warriors", "0.1.0", &got) ==
               RNET_LAN_LOBBY_OK && got.started,
           "start visible to guest");
    expect(got.input_delay == 5, "input_delay round-trips in V3 file");
    expect(got.rollback == 1 && got.input_prediction == 7,
           "rollback/prediction round-trip in V3 file");
    expect(rnet_lan_lobby_set_host_slot(path, 1) == RNET_LAN_LOBBY_OK,
           "move host slot");
    expect(rnet_lan_lobby_read(path, "Metal Warriors", "0.1.0", &got) ==
               RNET_LAN_LOBBY_OK && got.host_slot == 1 && !got.started,
           "slot move resets start");
    expect(rnet_lan_lobby_leave(path, 0) == RNET_LAN_LOBBY_OK,
           "guest leaves");
    expect(rnet_lan_lobby_read(path, "Metal Warriors", "0.1.0", &got) ==
               RNET_LAN_LOBBY_OK && got.joiner_name[0] == '\0',
           "guest slot cleared");
    expect(rnet_lan_lobby_join(path, "Metal Warriors", "0.1.0", "secret",
                               "Guest2", &got) == RNET_LAN_LOBBY_OK,
           "guest rejoins");
    expect(rnet_lan_lobby_kick(path) == RNET_LAN_LOBBY_OK, "host kicks guest");
    expect(rnet_lan_lobby_read(path, "Metal Warriors", "0.1.0", &got) ==
               RNET_LAN_LOBBY_OK && got.joiner_name[0] == '\0' &&
               strcmp(got.host_name, "Host") == 0,
           "kick clears guest and keeps room");
    expect(rnet_lan_lobby_leave(path, 1) == RNET_LAN_LOBBY_OK,
           "host removes room");
    expect(rnet_lan_lobby_read(path, NULL, NULL, &got) == RNET_LAN_LOBBY_ERR_IO,
           "removed room unavailable");

    if (failures == 0)
    {
        puts("lan_lobby_test: ok");
        return 0;
    }
    return 1;
}
