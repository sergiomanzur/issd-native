#ifndef ISSD_INPUT_H
#define ISSD_INPUT_H

#include <SDL.h>
#include "issd_menu.h"

#define ISSD_LOCAL_PLAYERS 4

/* Slots are assigned in connection order. Removing a device never renumbers
 * another player; the next connection fills the first vacant slot. Keyboard
 * and touch remain additional sources for P1. SDL events carry instance IDs,
 * whereas DEVICEADDED carries a device index. */
void issd_input_init(void);
void issd_input_shutdown(void);
int issd_input_add(int device_index);
int issd_input_remove(SDL_JoystickID instance);
int issd_input_player(SDL_JoystickID instance);
SDL_GameController *issd_input_controller(int player);
uint8_t issd_input_connected(void);
uint16_t issd_input_read(int player, IssdControlSchema schema);
void issd_input_block_held(void);
void issd_input_set_focus(bool focused);

#endif
