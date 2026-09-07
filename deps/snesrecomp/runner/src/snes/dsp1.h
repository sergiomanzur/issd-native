#ifndef SNES_DSP1_H
#define SNES_DSP1_H

#include <stdint.h>

struct SaveLoadInfo;

typedef struct Dsp1 Dsp1;

Dsp1 *dsp1_create(void);
void dsp1_destroy(Dsp1 *d);
void dsp1_reset(Dsp1 *d);
void dsp1_sync(Dsp1 *d, uint64_t master_clock);

uint8_t dsp1_read(Dsp1 *d, uint16_t addr);
void dsp1_write(Dsp1 *d, uint16_t addr, uint8_t value);

uint8_t dsp1_read_data_ram(Dsp1 *d, uint16_t addr);
void dsp1_write_data_ram(Dsp1 *d, uint16_t addr, uint8_t value);

int dsp1_load_firmware(Dsp1 *d, const char *rom_path);
int dsp1_firmware_loaded(const Dsp1 *d);
int dsp1_hle_active(const Dsp1 *d);
int dsp1_hle_failed(const Dsp1 *d);
uint8_t dsp1_hle_failed_command(const Dsp1 *d);
uint64_t dsp1_instructions_executed(const Dsp1 *d);
uint64_t dsp1_host_reads(const Dsp1 *d);
uint64_t dsp1_host_writes(const Dsp1 *d);
uint64_t dsp1_command_count(const Dsp1 *d, uint8_t command);

void dsp1_saveload(Dsp1 *d, struct SaveLoadInfo *sli);

#endif /* SNES_DSP1_H */
