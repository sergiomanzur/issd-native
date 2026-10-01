#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include "issd_input.h"

static void update(void) { SDL_PumpEvents(); SDL_GameControllerUpdate(); }

int main(void) {
    SDL_SetMainReady();
    assert(SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) == 0);
    int devices[5];
    SDL_Joystick *sticks[5];
    for (int i = 0; i < 5; i++) {
        devices[i] = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                               SDL_CONTROLLER_AXIS_MAX,
                                               SDL_CONTROLLER_BUTTON_MAX, 0);
        assert(devices[i] >= 0);
        sticks[i] = SDL_JoystickOpen(devices[i]);
        assert(sticks[i]);
        SDL_JoystickSetVirtualAxis(sticks[i], SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768);
        SDL_JoystickSetVirtualAxis(sticks[i], SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768);
    }
    issd_input_init();
    update();
    /* Startup enumeration also works when the developer has real pads attached.
     * Then isolate the virtual devices for the routing assertions below. */
    int assigned = 0;
    for (int device = 0; device < SDL_NumJoysticks() && assigned < 4; device++) {
        if (!SDL_IsGameController(device)) continue;
        assert(issd_input_player(SDL_JoystickGetDeviceInstanceID(device)) == assigned++);
    }
    issd_input_shutdown();
    for (int i = 0; i < 4; i++) assert(issd_input_add(devices[i]) == i);
    assert(issd_input_connected() == 15);
    for (int i = 0; i < 4; i++) {
        assert(issd_input_player(SDL_JoystickInstanceID(sticks[i])) == i);
        assert(issd_input_add(devices[i]) == i); /* duplicate startup event */
        SDL_JoystickSetVirtualButton(sticks[i], SDL_CONTROLLER_BUTTON_A, 1);
    }
    assert(issd_input_player(SDL_JoystickInstanceID(sticks[4])) == -1);
    assert(issd_input_add(devices[4]) == -1);
    update();
    for (int i = 0; i < 4; i++) assert(issd_input_read(i, ISSD_SCHEMA_CLASSIC) == 1);
    SDL_JoystickSetVirtualButton(sticks[1], SDL_CONTROLLER_BUTTON_A, 0);
    SDL_JoystickSetVirtualButton(sticks[1], SDL_CONTROLLER_BUTTON_B, 1);
    update();
    assert(issd_input_read(1, ISSD_SCHEMA_CLASSIC) == 256);
    assert(issd_input_read(0, ISSD_SCHEMA_CLASSIC) == 1);

    /* Releasing one alias must not cancel another held source. */
    SDL_JoystickSetVirtualButton(sticks[0], SDL_CONTROLLER_BUTTON_X, 1);
    SDL_JoystickSetVirtualButton(sticks[0], SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 1);
    update();
    assert(issd_input_read(0, ISSD_SCHEMA_FIFA) & 2);
    SDL_JoystickSetVirtualButton(sticks[0], SDL_CONTROLLER_BUTTON_X, 0);
    update();
    assert(issd_input_read(0, ISSD_SCHEMA_FIFA) & 2);
    SDL_JoystickSetVirtualButton(sticks[0], SDL_CONTROLLER_BUTTON_DPAD_LEFT, 1);
    SDL_JoystickSetVirtualAxis(sticks[0], SDL_CONTROLLER_AXIS_LEFTX, 20000);
    update();
    assert(!(issd_input_read(0, ISSD_SCHEMA_CLASSIC) & 192)); /* opposing directions */
    SDL_JoystickSetVirtualAxis(sticks[0], SDL_CONTROLLER_AXIS_LEFTX, 0);
    update();
    assert(issd_input_read(0, ISSD_SCHEMA_CLASSIC) & 64); /* dpad survives stick release */

    /* Overlay/focus transitions consume held controls until they are released. */
    issd_input_block_held();
    assert(issd_input_read(0, ISSD_SCHEMA_CLASSIC) == 0);
    SDL_JoystickSetVirtualButton(sticks[0], SDL_CONTROLLER_BUTTON_A, 0);
    update();
    assert(issd_input_read(0, ISSD_SCHEMA_CLASSIC) == 0);
    SDL_JoystickSetVirtualButton(sticks[0], SDL_CONTROLLER_BUTTON_A, 1);
    update();
    assert(issd_input_read(0, ISSD_SCHEMA_CLASSIC) == 1);
    issd_input_set_focus(false);
    assert(issd_input_read(0, ISSD_SCHEMA_CLASSIC) == 0);
    issd_input_set_focus(true);
    assert(issd_input_read(0, ISSD_SCHEMA_CLASSIC) == 0);

    SDL_JoystickID removed = SDL_JoystickInstanceID(sticks[1]);
    assert(issd_input_remove(removed) == 1);
    assert(issd_input_remove(removed) == -1);
    assert(issd_input_read(1, ISSD_SCHEMA_CLASSIC) == 0);
    assert(issd_input_player(SDL_JoystickInstanceID(sticks[2])) == 2);
    assert(issd_input_player(SDL_JoystickInstanceID(sticks[3])) == 3);
    assert(issd_input_add(devices[1]) == 1); /* vacated slot, others retain identity */
    issd_input_shutdown();
    assert(issd_input_connected() == 0);
    for (int i = 4; i >= 0; i--) {
        SDL_JoystickClose(sticks[i]);
        assert(SDL_JoystickDetachVirtual(devices[i]) == 0);
    }
    SDL_Quit();
    puts("four local gamepads: PASS");
    return 0;
}
