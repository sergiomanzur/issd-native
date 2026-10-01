/* Reference calls execute the original cartridge instructions in an isolated
 * interpreter and bus. This harness never contains copied ROM bytes. */
#include "snes/interp816.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ISSD_PASSWORD_NATIVE_TEST
#include "issd_password.h"
#endif

typedef struct { unsigned char *rom; size_t size; unsigned char ram[0x20000]; unsigned char io[0x20]; } Bus;
int interp816_opcode_hook(uint32_t address) { (void)address; return 0; }
static uint8_t read_bus(void *opaque, uint32_t address) {
  Bus *bus = opaque;
  unsigned bank = address >> 16, low = address & 0xffff;
  if (bank == 0x7e || bank == 0x7f) return bus->ram[((bank - 0x7e) << 16) | low];
  if (low < 0x2000 && (bank < 0x40 || (bank >= 0x80 && bank < 0xc0))) return bus->ram[low];
  if (low >= 0x4200 && low <= 0x421f) return bus->io[low - 0x4200];
  unsigned offset = ((bank & 0x7f) << 15) | (low & 0x7fff);
  assert(low >= 0x8000 && offset < bus->size);
  return bus->rom[offset];
}
static void write_bus(void *opaque, uint32_t address, uint8_t value) {
  Bus *bus = opaque;
  unsigned bank = address >> 16, low = address & 0xffff;
  if (bank == 0x7e || bank == 0x7f) { bus->ram[((bank - 0x7e) << 16) | low] = value; return; }
  if (low < 0x2000 && (bank < 0x40 || (bank >= 0x80 && bank < 0xc0))) { bus->ram[low] = value; return; }
  assert(low >= 0x4200 && low <= 0x421f);
  bus->io[low - 0x4200] = value;
  if (low == 0x4206) {
    unsigned dividend = bus->io[4] | (unsigned)bus->io[5] << 8;
    unsigned quotient = value ? dividend / value : 0xffff;
    unsigned remainder = value ? dividend % value : dividend;
    bus->io[0x14] = (unsigned char)quotient; bus->io[0x15] = (unsigned char)(quotient >> 8);
    bus->io[0x16] = (unsigned char)remainder; bus->io[0x17] = (unsigned char)(remainder >> 8);
  }
}
static void call(Bus *bus, Interp816 *cpu, unsigned address) {
  cpu->k = address >> 16; cpu->pc = address & 0xffff;
  cpu->sp = 0x1ffb; cpu->dp = 0; cpu->db = 0x81;
  bus->ram[0x1ffc] = 0xfe; bus->ram[0x1ffd] = 0xff; bus->ram[0x1ffe] = 0x7f;
  for (unsigned i = 0; i < 100000; ++i) {
    if (cpu->k == 0x7f && cpu->pc == 0xffff) return;
    interp816_runOpcode(cpu);
  }
  assert(!"original cartridge routine exceeded instruction budget");
}
static void load(const char *path, unsigned char *out, size_t size) {
  FILE *f = fopen(path,"rb"); assert(f);
  assert(fread(out,1,size,f) == size); assert(fclose(f) == 0);
}
static void store(const char *path, const unsigned char *data, size_t size) {
  FILE *f = fopen(path,"wb"); assert(f);
  assert(fwrite(data,1,size,f) == size); assert(fclose(f) == 0);
}
int main(int argc, char **argv) {
  assert(argc >= 5);
  Bus *bus = calloc(1,sizeof(*bus)); assert(bus);
  bus->size = 0x200000; bus->rom = malloc(bus->size); assert(bus->rom);
  load(argv[1],bus->rom,bus->size); load(argv[2],bus->ram,sizeof(bus->ram));
  Interp816 *cpu = interp816_init(bus,read_bus,write_bus); assert(cpu);
  cpu->e = cpu->mf = cpu->xf = false;
#ifdef ISSD_PASSWORD_NATIVE_TEST
  assert(issd_password_set_context(bus->rom,bus->size,bus->rom,bus->size,0));
  if (!strcmp(argv[3],"native-context")) {
    for (unsigned i = 0; i < 64; ++i) {
      assert(issd_password_glyph(i) == read_bus(bus,0x87dd45 + i));
      const char *label = issd_password_symbol_label(i);
      if (!label[1]) assert(issd_password_symbol_from_char(label[0]) == (int)i);
    }
    assert(issd_password_symbol_from_char('/') == 42);
    assert(issd_password_symbol_from_char('A') == -1);
    assert(issd_password_symbol_from_char('B') != issd_password_symbol_from_char('b'));
    assert(!issd_password_set_context(bus->rom,bus->size,bus->rom,bus->size,1));
    assert(!issd_password_available());
    unsigned char *changed = malloc(bus->size); assert(changed); memcpy(changed,bus->rom,bus->size); changed[123] ^= 1;
    assert(!issd_password_set_context(bus->rom,bus->size,changed,bus->size,0));
    assert(!issd_password_set_context(changed,bus->size,changed,bus->size,0));
    assert(!issd_password_available());
    assert(issd_password_set_context(bus->rom,bus->size,bus->rom,bus->size,0));
    assert(issd_password_available()); free(changed);
    uint8_t symbols[60]; memset(symbols,0xaa,sizeof(symbols)); size_t count = 123;
    assert(!issd_password_encode(bus->ram,symbols,1,&count));
    assert(count == 123 && symbols[0] == 0xaa);
    printf("accepted=1\n");
  } else if (!strcmp(argv[3],"native-encode")) {
    uint8_t symbols[60]; size_t count = 0;
    bool ok = issd_password_encode(bus->ram,symbols,sizeof(symbols),&count);
    printf("accepted=%d count=%zu error=%s\n",ok,count,issd_password_error());
    if (ok) store(argv[4],symbols,count);
  } else if (!strcmp(argv[3],"native-decode") || !strcmp(argv[3],"native-submit")) {
    assert(argc == 6); size_t count = (size_t)atoi(argv[5]); assert(count <= 60);
    uint8_t symbols[60]; load(argv[4],symbols,count);
    uint8_t before[0x20000], decoded[0x20000]; memcpy(before,bus->ram,sizeof(before)); memset(decoded,0xaa,sizeof(decoded));
    bool ok = !strcmp(argv[3],"native-submit") ? issd_password_submit(bus->ram,symbols,count) : issd_password_decode(symbols,count,bus->ram,decoded);
    printf("accepted=%d error=%s\n",ok,issd_password_error());
    if (!strcmp(argv[3],"native-decode")) { assert(!memcmp(before,bus->ram,sizeof(before))); store("decoded.wram",decoded,sizeof(decoded)); }
    else { if (!ok) assert(!memcmp(before,bus->ram,sizeof(before))); store("submitted.wram",bus->ram,sizeof(bus->ram)); }
  } else
#endif
  if (!strcmp(argv[3],"encode")) {
    cpu->a = bus->ram[0x1648] | (unsigned)bus->ram[0x1649] << 8;
    call(bus,cpu,0xa4a9a3);
    assert(cpu->x < 24);
    size_t count = read_bus(bus,0x81e44d + cpu->x);
    call(bus,cpu,0x83f46d);
    cpu->y = (uint16_t)count;
    call(bus,cpu,0x86cb64);
    store(argv[4],bus->ram + 0xe2d0,count);
    printf("count=%zu flags=%04x\n",count,bus->ram[0x1648] | (unsigned)bus->ram[0x1649] << 8);
  } else {
    assert(argc == 6); size_t count = (size_t)atoi(argv[5]); assert(count <= 60);
    memset(bus->ram + 0xe2d0,255,60); load(argv[4],bus->ram + 0xe2d0,count);
    call(bus,cpu,0x86cb27); cpu->a = (uint16_t)count;
    call(bus,cpu,0x83f674);
    printf("accepted=%d flags=%04x team=%u\n",!cpu->c,bus->ram[0x1648] | (unsigned)bus->ram[0x1649] << 8,bus->ram[0xda0]);
    store("decoded.wram",bus->ram,sizeof(bus->ram));
  }
  interp816_free(cpu); free(bus->rom); free(bus); return 0;
}
