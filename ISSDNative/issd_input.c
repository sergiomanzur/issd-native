#include "issd_input.h"

static SDL_GameController *s_controllers[ISSD_LOCAL_PLAYERS];
static uint64_t s_blocked[ISSD_LOCAL_PLAYERS];
static bool s_focused = true;
static const IssdPlayerProfile *s_profiles;

void issd_input_configure(const IssdPlayerProfile *profiles) { s_profiles = profiles; }

enum { STICK_UP = 32, STICK_DOWN, STICK_LEFT, STICK_RIGHT, TRIGGER_LEFT, TRIGGER_RIGHT };

static uint64_t raw_controls(int player) {
    SDL_GameController *pad = s_controllers[player];
    if (!pad || !SDL_GameControllerGetAttached(pad)) return 0;
    int stick = s_profiles ? s_profiles[player].stick_deadzone : 12000;
    int trigger = s_profiles ? s_profiles[player].trigger_deadzone : 12000;
    stick = stick < 0 ? 0 : stick > 30000 ? 30000 : stick;
    trigger = trigger < 0 ? 0 : trigger > 30000 ? 30000 : trigger;
    uint64_t raw = 0;
    for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX && button <= 20; button++)
        if (button != SDL_CONTROLLER_BUTTON_GUIDE && SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)button))
            raw |= UINT64_C(1) << button;
    int x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
    int y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
    if (x < -stick) raw |= UINT64_C(1) << STICK_LEFT;
    if (x > stick) raw |= UINT64_C(1) << STICK_RIGHT;
    if (y < -stick) raw |= UINT64_C(1) << STICK_UP;
    if (y > stick) raw |= UINT64_C(1) << STICK_DOWN;
    if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > trigger)
        raw |= UINT64_C(1) << TRIGGER_LEFT;
    if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > trigger)
        raw |= UINT64_C(1) << TRIGGER_RIGHT;
    return raw;
}

int issd_input_player(SDL_JoystickID instance) {
    for (int i = 0; i < ISSD_LOCAL_PLAYERS; i++)
        if (s_controllers[i] &&
            SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(s_controllers[i])) == instance)
            return i;
    return -1;
}

SDL_GameController *issd_input_controller(int player) {
    return player >= 0 && player < ISSD_LOCAL_PLAYERS ? s_controllers[player] : NULL;
}

int issd_input_add(int device_index) {
    SDL_JoystickID instance = SDL_JoystickGetDeviceInstanceID(device_index);
    if (instance < 0 || !SDL_IsGameController(device_index)) return -1;
    int existing = issd_input_player(instance);
    if (existing >= 0) return existing;
    for (int i = 0; i < ISSD_LOCAL_PLAYERS; i++) {
        if (s_controllers[i]) continue;
        SDL_GameController *pad = SDL_GameControllerOpen(device_index);
        if (!pad) return -1;
        s_controllers[i] = pad;
        s_blocked[i] = raw_controls(i);
        SDL_GameControllerSetPlayerIndex(pad, i);
        return i;
    }
    return -1;
}

int issd_input_remove(SDL_JoystickID instance) {
    int player = issd_input_player(instance);
    if (player < 0) return -1;
    SDL_GameControllerClose(s_controllers[player]);
    s_controllers[player] = NULL;
    s_blocked[player] = 0;
    return player;
}

void issd_input_shutdown(void) {
    for (int i = 0; i < ISSD_LOCAL_PLAYERS; i++) {
        if (s_controllers[i]) SDL_GameControllerClose(s_controllers[i]);
        s_controllers[i] = NULL;
        s_blocked[i] = 0;
    }
}

void issd_input_init(void) {
    issd_input_shutdown();
    s_focused = true;
    for (int i = 0; i < SDL_NumJoysticks(); i++) issd_input_add(i);
}

uint8_t issd_input_connected(void) {
    uint8_t mask = 0;
    for (int i = 0; i < ISSD_LOCAL_PLAYERS; i++)
        if (s_controllers[i] && SDL_GameControllerGetAttached(s_controllers[i])) mask |= 1u << i;
    return mask;
}

void issd_input_block_held(void) {
    for (int i = 0; i < ISSD_LOCAL_PLAYERS; i++) s_blocked[i] = raw_controls(i);
}

void issd_input_set_focus(bool focused) {
    s_focused = focused;
    issd_input_block_held();
}

uint16_t issd_input_read(int player, IssdControlSchema schema) {
    if (player < 0 || player >= ISSD_LOCAL_PLAYERS) return 0;
    uint64_t raw = raw_controls(player);
    s_blocked[player] &= raw;
    if (!s_focused) return 0;
    raw &= ~s_blocked[player];
    uint16_t mask = 0;
    if (s_profiles) {
        for (int bit = 0; bit < ISSD_PROFILE_BINDINGS; bit++)
            if (raw & s_profiles[player].bindings[bit]) mask |= 1u << bit;
    } else {
#define MAP(source, bit) do { if (raw & (UINT64_C(1) << (source))) mask |= 1u << (bit); } while (0)
    MAP(SDL_CONTROLLER_BUTTON_DPAD_UP, 4); MAP(STICK_UP, 4);
    MAP(SDL_CONTROLLER_BUTTON_DPAD_DOWN, 5); MAP(STICK_DOWN, 5);
    MAP(SDL_CONTROLLER_BUTTON_DPAD_LEFT, 6); MAP(STICK_LEFT, 6);
    MAP(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 7); MAP(STICK_RIGHT, 7);
    MAP(SDL_CONTROLLER_BUTTON_BACK, 2); MAP(SDL_CONTROLLER_BUTTON_START, 3);
    MAP(SDL_CONTROLLER_BUTTON_A, 0); MAP(SDL_CONTROLLER_BUTTON_Y, 9);
    MAP(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 10); MAP(TRIGGER_LEFT, 11);
    if (schema == ISSD_SCHEMA_PES) {
        MAP(SDL_CONTROLLER_BUTTON_X, 8); MAP(SDL_CONTROLLER_BUTTON_B, 1);
    } else {
        MAP(SDL_CONTROLLER_BUTTON_B, 8); MAP(SDL_CONTROLLER_BUTTON_X, 1);
    }
    MAP(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, schema == ISSD_SCHEMA_CLASSIC ? 11 : 1);
    MAP(TRIGGER_RIGHT, 1);
#undef MAP
    }
    if ((mask & 48u) == 48u) mask &= ~48u;
    if ((mask & 192u) == 192u) mask &= ~192u;
    return mask;
}

uint64_t issd_input_raw(int player) {
    return s_focused && player >= 0 && player < ISSD_LOCAL_PLAYERS ? raw_controls(player) : 0;
}
