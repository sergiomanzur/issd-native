#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include "issd_input.h"
#include "issd_config.h"

static void update(void) { SDL_PumpEvents(); SDL_GameControllerUpdate(); }

int main(void) {
    IssdConfig cfg;
    issd_config_init_defaults(&cfg);
    assert(cfg.player_profiles[0].schema == ISSD_SCHEMA_CLASSIC);
    assert(cfg.player_profiles[3].stick_deadzone == 12000);
    cfg.player_profiles[0].stick_deadzone = 5000;
    cfg.player_profiles[1].stick_deadzone = 20000;
    cfg.player_profiles[0].trigger_deadzone = 5000;
    cfg.player_profiles[1].trigger_deadzone = 20000;
    issd_config_player_preset(&cfg, 1, ISSD_SCHEMA_PES);
    assert(cfg.player_profiles[1].stick_deadzone == 20000);
    SDL_SetMainReady();
    assert(SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) == 0);
    SDL_Joystick *sticks[4];
    int devices[4];
    for (int i = 0; i < 4; i++) {
        devices[i] = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                      SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        assert(devices[i] >= 0);
        sticks[i] = SDL_JoystickOpen(devices[i]);
        assert(sticks[i]);
        SDL_JoystickSetVirtualAxis(sticks[i], SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768);
        SDL_JoystickSetVirtualAxis(sticks[i], SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768);
    }
    update();
    issd_input_configure(cfg.player_profiles);
    for (int i = 0; i < 4; i++) assert(issd_input_add(devices[i]) == i);
    for (int i = 0; i < 2; i++)
        SDL_JoystickSetVirtualAxis(sticks[i], SDL_CONTROLLER_AXIS_LEFTX, 10000);
    update();
    assert(issd_input_read(0, ISSD_SCHEMA_CLASSIC) == 128);
    assert(issd_input_read(1, ISSD_SCHEMA_CLASSIC) == 0);
    SDL_JoystickSetVirtualAxis(sticks[0], SDL_CONTROLLER_AXIS_LEFTX, 5000);
    update();
    assert(issd_input_read(0, ISSD_SCHEMA_CLASSIC) == 0);
    for (int i = 0; i < 2; i++) {
        SDL_JoystickSetVirtualAxis(sticks[i], SDL_CONTROLLER_AXIS_LEFTX, 0);
        /* SDL maps signed joystick trigger axes to unsigned controller axes. */
        SDL_JoystickSetVirtualAxis(sticks[i], SDL_CONTROLLER_AXIS_TRIGGERLEFT, -12768);
    }
    update();
    assert(issd_input_read(0, ISSD_SCHEMA_CLASSIC) == 2048);
    assert(issd_input_read(1, ISSD_SCHEMA_CLASSIC) == 0);
    cfg.player_profiles[2].schema = 3;
    for (int i = 0; i < 12; i++) cfg.player_profiles[2].bindings[i] = 0;
    cfg.player_profiles[2].bindings[8] = (UINT64_C(1) << SDL_CONTROLLER_BUTTON_A) |
                                       (UINT64_C(1) << SDL_CONTROLLER_BUTTON_X);
    SDL_JoystickSetVirtualButton(sticks[2], SDL_CONTROLLER_BUTTON_A, 1);
    SDL_JoystickSetVirtualButton(sticks[3], SDL_CONTROLLER_BUTTON_A, 1);
    update();
    assert(issd_input_read(2, ISSD_SCHEMA_CLASSIC) == 256);
    assert(issd_input_read(3, ISSD_SCHEMA_PES) == 1);
    issd_input_block_held();
    assert(issd_input_read(2, ISSD_SCHEMA_CLASSIC) == 0);
    assert(issd_input_raw(2) & 1);
    SDL_JoystickSetVirtualButton(sticks[2], SDL_CONTROLLER_BUTTON_A, 0);
    update();
    assert(issd_input_read(2, ISSD_SCHEMA_CLASSIC) == 0);
    SDL_JoystickSetVirtualButton(sticks[2], SDL_CONTROLLER_BUTTON_X, 1);
    update();
    assert(issd_input_read(2, ISSD_SCHEMA_CLASSIC) == 256);
    SDL_JoystickSetVirtualButton(sticks[2], SDL_CONTROLLER_BUTTON_GUIDE, 1);
    cfg.player_profiles[2].bindings[0] = UINT64_C(1) << SDL_CONTROLLER_BUTTON_GUIDE;
    update();
    assert(!(issd_input_raw(2) & (UINT64_C(1) << SDL_CONTROLLER_BUTTON_GUIDE)));
    assert(!(issd_input_read(2, ISSD_SCHEMA_CLASSIC) & 1));
    issd_input_shutdown();
    issd_input_configure(NULL);
    for (int i = 3; i >= 0; i--) {
        SDL_JoystickClose(sticks[i]);
        assert(SDL_JoystickDetachVirtual(devices[i]) == 0);
    }
    SDL_Quit();
    puts("per-player control profiles: PASS");
    return 0;
}
