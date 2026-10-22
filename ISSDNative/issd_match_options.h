#ifndef ISSD_MATCH_OPTIONS_H
#define ISSD_MATCH_OPTIONS_H
#include <stdbool.h>
#include <stdint.h>
uint16_t issd_penalty_timing_input(const uint8_t *ram, uint16_t original, bool enabled);
bool issd_final_attack(const uint8_t *ram, bool enabled);
bool issd_short_goal_kick(const uint8_t *ram, uint16_t actor, bool enabled);
bool issd_duration_valid(int minutes);
uint16_t issd_duration_bcd(int minutes, unsigned period, uint16_t original);
#endif
