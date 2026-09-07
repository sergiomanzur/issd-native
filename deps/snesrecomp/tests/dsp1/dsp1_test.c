#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "dsp1.h"

static int check(int cond, const char *msg) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
  }
  return 0;
}

int main(void) {
  int fails = 0;
  Dsp1 *d = dsp1_create();
  fails += check(d != NULL, "dsp1_create");
  if (!d) return 1;

  fails += check(!dsp1_firmware_loaded(d), "firmware starts unloaded");

  dsp1_write(d, 0x8000, 0x34);
  fails += check((dsp1_read(d, 0x8001) & 0x10) != 0,
                 "first 16-bit DR write sets DRS");
  dsp1_write(d, 0x8000, 0x12);
  fails += check((dsp1_read(d, 0x8001) & 0x10) == 0,
                 "second 16-bit DR write clears DRS");
  fails += check((dsp1_read(d, 0x8001) & 0x80) == 0,
                 "external DR write clears RQM");
  fails += check(dsp1_host_reads(d) == 3,
                 "host reads are counted");
  fails += check(dsp1_host_writes(d) == 2,
                 "host writes are counted");

  dsp1_write_data_ram(d, 0x0000, 0x78);
  dsp1_write_data_ram(d, 0x0001, 0x56);
  fails += check(dsp1_read_data_ram(d, 0x0000) == 0x78,
                 "data RAM low byte");
  fails += check(dsp1_read_data_ram(d, 0x0001) == 0x56,
                 "data RAM high byte");
  fails += check(dsp1_read_data_ram(d, 0x0800) == 0x78,
                 "data RAM mask mirrors low byte");

  dsp1_sync(d, 1000000);
  fails += check(dsp1_instructions_executed(d) == 0,
                 "missing firmware does not execute garbage");

  dsp1_destroy(d);
  if (fails) return 1;
  puts("dsp1_test: PASS");
  return 0;
}
