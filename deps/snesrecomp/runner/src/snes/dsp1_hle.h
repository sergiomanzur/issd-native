#ifndef SNES_DSP1_HLE_H
#define SNES_DSP1_HLE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct Dsp1HleState {
  int16_t sin_aas;
  int16_t cos_aas;
  int16_t sin_azs;
  int16_t cos_azs;
  int16_t nx;
  int16_t ny;
  int16_t nz;
  int16_t gx;
  int16_t gy;
  int16_t gz;
  int16_t les_coefficient;
  int16_t les_exponent;
  int16_t les;
  int16_t vplane_coefficient;
  int16_t vplane_exponent;
  int16_t v_offset;
  int16_t sec_azs_coefficient;
  int16_t sec_azs_exponent;
  bool projection_valid;
} Dsp1HleState;

/*
 * Firmware-free DSP-1 command model. This interface is deliberately separate
 * from the host DR/SR protocol until every command needed by a target has
 * passed differential tests against the instruction-level core.
 */
void dsp1_hle_state_reset(Dsp1HleState *state);
bool dsp1_hle_command_shape(uint8_t command, uint8_t *input_words,
                            uint8_t *output_words);
bool dsp1_hle_execute_state(Dsp1HleState *state, uint8_t command,
                            const int16_t *input, uint8_t input_words,
                            int16_t *output, uint8_t output_capacity,
                            uint8_t *output_words);
bool dsp1_hle_execute(uint8_t command, const int16_t *input,
                      uint8_t input_words, int16_t *output,
                      uint8_t output_capacity, uint8_t *output_words);

#endif /* SNES_DSP1_HLE_H */
