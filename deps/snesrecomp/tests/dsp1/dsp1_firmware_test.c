#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "dsp1.h"
#include "dsp1_hle.h"

enum {
  kRqm = 0x80,
  kDrc = 0x04,
};

static uint64_t master_clock;

static int check(int cond, const char *msg) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
  }
  return 0;
}

static int wait_rqm(Dsp1 *d, int wanted) {
  for (unsigned i = 0; i < 100000; i++) {
    uint8_t sr = dsp1_read(d, 1);
    if (!!(sr & kRqm) == wanted) return 1;
    master_clock += 128;
    dsp1_sync(d, master_clock);
  }
  return 0;
}

static int write_command(Dsp1 *d, uint8_t command) {
  if (!wait_rqm(d, 1)) return 0;
  if (!(dsp1_read(d, 1) & kDrc)) return 0;
  dsp1_write(d, 0, command);
  return wait_rqm(d, 1);
}

static int write_word(Dsp1 *d, uint16_t value) {
  if (!wait_rqm(d, 1)) return 0;
  if (dsp1_read(d, 1) & kDrc) return 0;
  dsp1_write(d, 0, (uint8_t)value);
  dsp1_write(d, 0, (uint8_t)(value >> 8));
  return 1;
}

static int read_word(Dsp1 *d, uint16_t *value) {
  if (!wait_rqm(d, 1)) return 0;
  if (dsp1_read(d, 1) & kDrc) return 0;
  uint8_t lo = dsp1_read(d, 0);
  uint8_t hi = dsp1_read(d, 0);
  *value = (uint16_t)(lo | ((uint16_t)hi << 8));
  return 1;
}

static int compare_hle_state(Dsp1 *d, Dsp1HleState *state, uint8_t command,
                             const int16_t *input, uint8_t input_words) {
  int16_t hle_output[4] = {0};
  uint16_t lle_output[4] = {0};
  uint8_t output_words = 0;
  if (!dsp1_hle_execute_state(state, command, input, input_words, hle_output,
                              4, &output_words) ||
      !write_command(d, command))
    return 0;
  for (uint8_t i = 0; i < input_words; i++) {
    if (!write_word(d, (uint16_t)input[i])) return 0;
  }
  int matches = 1;
  for (uint8_t i = 0; i < output_words; i++) {
    if (!read_word(d, &lle_output[i])) return 0;
    if (lle_output[i] != (uint16_t)hle_output[i]) {
      fprintf(stderr,
              "dsp1_hle mismatch: command=%02x input=%04x,%04x,%04x "
              "word=%u lle=%04x hle=%04x count=%llu\n",
              command, (uint16_t)input[0],
              input_words > 1 ? (uint16_t)input[1] : 0,
              input_words > 2 ? (uint16_t)input[2] : 0, i, lle_output[i],
              (uint16_t)hle_output[i],
              (unsigned long long)dsp1_command_count(d, command));
      matches = 0;
    }
  }
  return matches;
}

static int compare_hle(Dsp1 *d, uint8_t command, const int16_t *input,
                       uint8_t input_words) {
  return compare_hle_state(d, NULL, command, input, input_words);
}

static int compare_raster_continuation(Dsp1 *d, Dsp1HleState *state,
                                       int16_t raster) {
  int16_t hle_output[4] = {0};
  uint8_t output_words = 0;
  if (!dsp1_hle_execute_state(state, 0x0a, &raster, 1, hle_output, 4,
                              &output_words) ||
      output_words != 4)
    return 0;
  for (unsigned word = 0; word < 4; word++) {
    uint16_t lle_output;
    if (!read_word(d, &lle_output)) return 0;
    if (lle_output != (uint16_t)hle_output[word]) {
      fprintf(stderr,
              "dsp1_hle raster mismatch: raster=%04x word=%u "
              "lle=%04x hle=%04x\n",
              (uint16_t)raster, word, lle_output, (uint16_t)hle_output[word]);
      return 0;
    }
  }
  return 1;
}

int main(void) {
  const char *firmware = getenv("SNESRECOMP_DSP1_ROM");
  if (!firmware || !firmware[0]) {
    puts("dsp1_firmware_test: SKIP (SNESRECOMP_DSP1_ROM is unset)");
    return 0;
  }

  Dsp1 *d = dsp1_create();
  int fails = 0;
  uint16_t result = 0;
  uint16_t result2 = 0;

  fails += check(d != NULL, "dsp1_create");
  if (!d) return 1;
  fails += check(dsp1_load_firmware(d, NULL), "load DSP-1 firmware");
  fails += check(dsp1_firmware_loaded(d), "firmware reports loaded");
  fails += check(!dsp1_hle_active(d), "loaded firmware keeps LLE active");

  master_clock = 100000;
  dsp1_sync(d, master_clock);
  fails += check(wait_rqm(d, 1), "firmware reaches command-ready state");
  fails += check((dsp1_read(d, 1) & kDrc) != 0,
                 "command-ready state selects 8-bit DR");

  fails += check(write_command(d, 0x00), "submit multiply command");
  fails += check(write_word(d, 0x4000), "submit multiply operand 1");
  fails += check(wait_rqm(d, 1), "firmware requests multiply operand 2");
  fails += check(write_word(d, 0x4000), "submit multiply operand 2");
  fails += check(read_word(d, &result), "read multiply result");
  fails += check(result == 0x2000, "DSP-1 multiply result");

  fails += check(write_command(d, 0x20), "submit multiply-plus-one command");
  fails += check(write_word(d, 0x4000), "submit multiply-plus-one operand 1");
  fails += check(write_word(d, 0x4000), "submit multiply-plus-one operand 2");
  fails += check(read_word(d, &result), "read multiply-plus-one result");
  fails += check(result == 0x2001, "DSP-1 multiply-plus-one result");

  fails += check(write_command(d, 0x04), "submit sin/cos command");
  fails += check(write_word(d, 0x0000), "submit sin/cos angle");
  fails += check(write_word(d, 0x4000), "submit sin/cos radius");
  fails += check(read_word(d, &result), "read sine result");
  fails += check(read_word(d, &result2), "read cosine result");
  fails += check(result == 0x0000 && result2 == 0x3fff,
                 "DSP-1 sin/cos axis result");

  fails += check(write_command(d, 0x08), "submit vector-size command");
  fails += check(write_word(d, 0x1000), "submit vector-size X");
  fails += check(write_word(d, 0x0000), "submit vector-size Y");
  fails += check(write_word(d, 0x0000), "submit vector-size Z");
  fails += check(read_word(d, &result), "read vector-size low word");
  fails += check(read_word(d, &result2), "read vector-size high word");
  fails += check(result == 0x0000 && result2 == 0x0200,
                 "DSP-1 vector-size result");

  fails += check(write_command(d, 0x0c), "submit 2D rotate command");
  fails += check(write_word(d, 0x0000), "submit 2D rotate angle");
  fails += check(write_word(d, 0x4000), "submit 2D rotate X");
  fails += check(write_word(d, 0x2000), "submit 2D rotate Y");
  fails += check(read_word(d, &result), "read 2D rotate X");
  fails += check(read_word(d, &result2), "read 2D rotate Y");
  fails += check(result == 0x3fff && result2 == 0x1fff,
                 "DSP-1 2D identity rotation");

  fails += check(dsp1_command_count(d, 0x00) == 1 &&
                     dsp1_command_count(d, 0x20) == 1 &&
                     dsp1_command_count(d, 0x04) == 1 &&
                     dsp1_command_count(d, 0x08) == 1 &&
                     dsp1_command_count(d, 0x0c) == 1,
                 "command-ready writes are counted by command ID");
  fails += check(dsp1_command_count(d, 0x40) == 0,
                 "16-bit parameter writes are not counted as commands");
  const int16_t distance_regression[3] = {
      0x2a8e, (int16_t)0xfd93, (int16_t)0xf633};
  fails += check(compare_hle(d, 0x28, distance_regression, 3),
                 "original DSP-1 distance correction matches firmware");

  Dsp1HleState hle_state;
  dsp1_hle_state_reset(&hle_state);
  for (int16_t aas = 0; aas <= 0x0400; aas += 0x0100) {
    const int16_t projection[7] = {
        0x0880, 0x27a0, 0x0000, 0x0040, 0x0100, aas, 0x3400};
    fails += check(compare_hle_state(d, &hle_state, 0x02, projection, 7),
                   "SMK projection setup HLE matches firmware");
    for (int16_t offset = -0x0200; offset <= 0x0200; offset += 0x0100) {
      const int16_t point[3] = {
          (int16_t)(projection[0] + offset),
          (int16_t)(projection[1] - offset),
          (int16_t)(projection[2] + offset)};
      fails += check(compare_hle_state(d, &hle_state, 0x06, point, 3),
                     "SMK project HLE matches firmware");
    }
  }

  uint32_t random = 0x1badb002u;
  for (unsigned i = 0; i < 512 && !fails; i++) {
    int16_t input[3];
    for (unsigned word = 0; word < 3; word++) {
      random = random * 1664525u + 1013904223u;
      input[word] = (int16_t)(random >> 16);
    }
    fails += check(compare_hle(d, 0x20, input, 2),
                   "random multiply-plus-one HLE matches firmware");
    fails += check(compare_hle(d, 0x00, input, 2),
                   "random multiply HLE matches firmware");
    fails += check(compare_hle(d, 0x08, input, 3),
                   "random vector-size HLE matches firmware");
    fails += check(compare_hle(d, 0x04, input, 2),
                   "random sin/cos HLE matches firmware");
    fails += check(compare_hle(d, 0x0c, input, 3),
                   "random 2D rotate HLE matches firmware");
    fails += check(compare_hle_state(d, &hle_state, 0x06, input, 3),
                   "random project HLE matches firmware");
    fails += check(compare_hle(d, 0x10, input, 2),
                   "random inverse HLE matches firmware");
    int16_t range_input[4] = {input[0], input[1], input[2],
                              (int16_t)(input[0] ^ input[2])};
    fails += check(compare_hle(d, 0x18, range_input, 4),
                   "random range HLE matches firmware");
    int16_t distance_input[3] = {
        (int16_t)(input[0] >> 1),
        (int16_t)(input[1] >> 1),
        (int16_t)(input[2] >> 1)};
    fails += check(compare_hle(d, 0x28, distance_input, 3),
                   "random distance HLE matches firmware");
  }
  int16_t projection[7] = {
      0x0880, 0x27a0, 0x0000, 0x0040, 0x0100, 0x0400, 0x3400};
  int16_t raster_start = (int16_t)0xffb6;
  fails += check(compare_hle_state(d, &hle_state, 0x02, projection, 7),
                 "final SMK projection setup HLE matches firmware");
  fails += check(compare_hle_state(d, &hle_state, 0x0a, &raster_start, 1),
                 "SMK first raster HLE matches firmware");
  for (unsigned line = 1; line < 224 && !fails; line++) {
    int16_t raster = (int16_t)(raster_start + (int16_t)line);
    fails += check(compare_raster_continuation(d, &hle_state, raster),
                   "SMK continuous raster HLE matches firmware");
  }
  fails += check(dsp1_instructions_executed(d) > 0,
                 "firmware executed instructions");
  fails += check(!dsp1_hle_failed(d), "LLE run did not enter HLE failure");

  dsp1_destroy(d);
  if (fails) return 1;
  puts("dsp1_firmware_test: PASS");
  return 0;
}
