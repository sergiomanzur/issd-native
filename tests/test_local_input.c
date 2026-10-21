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
    /* A detached old connection may still occupy P1 when SDL reports the
     * replacement first. Reap it, keep P2 stable, and ignore late removal. */
    int first = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                         SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    int second = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                          SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    SDL_JoystickID old_id = SDL_JoystickGetDeviceInstanceID(first);
    SDL_JoystickID second_id = SDL_JoystickGetDeviceInstanceID(second);
    assert(issd_input_add(first) == 0 && issd_input_add(second) == 1);
    assert(SDL_JoystickDetachVirtual(first) == 0);
    int replacement = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                               SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    SDL_Joystick *fresh = SDL_JoystickOpen(replacement); assert(fresh);
    SDL_JoystickSetVirtualButton(fresh, SDL_CONTROLLER_BUTTON_A, 1); update();
    assert(issd_input_add(replacement) == 0);
    assert(issd_input_player(second_id) == 1);
    assert(issd_input_remove(old_id) == -1);
    assert(issd_input_read(0, ISSD_SCHEMA_CLASSIC) == 0); /* held input guard */
    SDL_JoystickSetVirtualButton(fresh, SDL_CONTROLLER_BUTTON_A, 0); update();
    assert(issd_input_read(0, ISSD_SCHEMA_CLASSIC) == 0);
    SDL_JoystickSetVirtualButton(fresh, SDL_CONTROLLER_BUTTON_A, 1); update();
    assert(issd_input_read(0, ISSD_SCHEMA_CLASSIC) == 1);
    assert(issd_input_player(second_id) == 1);
    issd_input_shutdown(); SDL_JoystickClose(fresh);
    assert(SDL_JoystickDetachVirtual(replacement) == 0);
    /* Removing first shifted the second device's index down by one. */
    assert(SDL_JoystickDetachVirtual(second - 1) == 0);
    /* An independently connected identical pad must not be mistaken for a
     * reconnect simply because it appeared just before another pad left. */
    first = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                      SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    second = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                       SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    old_id=SDL_JoystickGetDeviceInstanceID(first);
    second_id=SDL_JoystickGetDeviceInstanceID(second);
    assert(issd_input_add(first)==0 && issd_input_add(second)==1);
    SDL_Delay(1100);
    int newcomer=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                          SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    SDL_JoystickID newcomer_id=SDL_JoystickGetDeviceInstanceID(newcomer);
    assert(issd_input_add(newcomer)==2);
    assert(SDL_JoystickDetachVirtual(first)==0);
    assert(issd_input_remove(old_id)==0);
    assert(issd_input_player(second_id)==1 && issd_input_player(newcomer_id)==2);
    issd_input_shutdown();
    assert(SDL_JoystickDetachVirtual(newcomer-1)==0);
    assert(SDL_JoystickDetachVirtual(second-1)==0);
    SDL_Quit();
    puts("four local gamepads: PASS");
    return 0;
}
