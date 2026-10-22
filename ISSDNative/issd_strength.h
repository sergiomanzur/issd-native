#ifndef ISSD_STRENGTH_H
#define ISSD_STRENGTH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { int attack,midfield,defence,overall,stars; } IssdTeamStrength;
bool issd_strength_calculate(const uint8_t *rom,size_t size,int team,IssdTeamStrength *out);
void issd_strength_draw(uint32_t *fb,int width,int height,int margin,const uint8_t *ram,
                        const uint8_t *rom,size_t size,const uint8_t font[96][8]);
#endif
