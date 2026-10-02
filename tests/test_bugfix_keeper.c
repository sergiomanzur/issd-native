#include "issd_bugfix_keeper.h"
#include <assert.h>
#include <string.h>
static uint8_t ram[0x20000], original[0x20000];
#ifdef KEEPER_OPCODE_TEST
#include <stdio.h>
#include "interp816.h"
static uint8_t rom[0x200000];
static uint8_t read_bus(void *memory, uint32_t address) {
    (void)memory;
    unsigned bank = address >> 16, offset = address & 0xffff;
    if ((bank & 0x7f) < 0x40 && offset >= 0x8000)
        return rom[(bank & 0x7f) * 0x8000 + (offset & 0x7fff)];
    return ram[address & 0x1ffff];
}
#endif

static unsigned word(unsigned a) { return ram[a] | (unsigned)ram[a + 1] << 8; }
static void put(unsigned a, unsigned v) { ram[a] = (uint8_t)v; ram[a + 1] = (uint8_t)(v >> 8); }

/* Recreate $8485CF-$8485FE's movement branch and $80D2B3's exact 16.16
 * integration. Other input handling runs after this branch either way. */
static void human_pass(unsigned controller, unsigned actor, bool enabled) {
    put(0x88, controller);
    if (!(word(actor + 0x60) & 0x8000) ||
        issd_bugfix_keeper_skip_movement(ram, (uint16_t)actor, enabled)) return;
    for (unsigned axis = 0; axis < 2; ++axis) {
        unsigned fraction = actor + 0x06 + axis * 4;
        unsigned velocity = actor + 0x22 + axis * 4;
        unsigned sum = word(fraction) + word(velocity);
        put(fraction, sum);
        put(fraction + 2, word(fraction + 2) + word(velocity + 2) + (sum >> 16));
    }
}
static void fixture(unsigned actor, unsigned count) {
    issd_bugfix_keeper_begin_loop();
    memset(ram, 0, sizeof ram);
    put(actor + 0x60, 0x8000);
    put(actor + 0x08, 100); put(actor + 0x0c, 200);
    put(actor + 0x22, 0xc000); put(actor + 0x24, 2);
    put(actor + 0x26, 0x4000); put(actor + 0x28, 0xffff);
    for (unsigned i = 0; i < count; ++i) put(0x1aa0 + i * 0x30 + 0x2c, actor);
}
#ifdef KEEPER_OPCODE_TEST
static void write_bus(void *memory, uint32_t address, uint8_t value) {
    (void)memory; ram[address & 0x1ffff] = value;
}
static void actual_opcode_pass(Interp816 *cpu, unsigned controller, unsigned actor, bool enabled) {
    put(0x88, controller);
    cpu->pc = 0x85cf; cpu->k = 0x84; cpu->db = 0; cpu->dp = (uint16_t)actor;
    cpu->sp = 0x1af; cpu->e = cpu->mf = cpu->xf = cpu->d = false;
    for (unsigned instructions = 0; instructions < 32; ++instructions) {
        uint32_t pc = (uint32_t)cpu->k << 16 | cpu->pc;
        if (pc == 0x8485d1 && cpu->n &&
            issd_bugfix_keeper_skip_movement(ram, (uint16_t)actor, enabled)) cpu->n = false;
        /* Stop after the cartridge integrates both axes, before collision
         * checks; or immediately at its real skip-movement destination. */
        if (pc == 0x80d2cd || pc == 0x8485fe) return;
        interp816_runOpcode(cpu);
    }
    assert(!"original movement opcodes did not reach boundary");
}
static void verify_original_opcodes(const char *path) {
    FILE *file = fopen(path, "rb"); assert(file);
    assert(fread(rom, 1, sizeof rom, file) == sizeof rom); fclose(file);
    const uint8_t branch[] = {0xa6, 0x60, 0x10, 0x2b, 0x22, 0xb3, 0xd2, 0x80};
    assert(!memcmp(rom + 4 * 0x8000 + 0x5cf, branch, sizeof branch));
    Interp816 *cpu = interp816_init(NULL, read_bus, write_bus); assert(cpu);
    for (unsigned actor = 0x500; actor <= 0x1000; actor += 0xb00) {
        for (unsigned count = 1; count <= 5; ++count) {
            fixture(actor, count);
            for (unsigned i = 0; i < count; ++i) actual_opcode_pass(cpu, 0x1aa0 + i * 0x30, actor, false);
            assert(word(actor + 8) == 100 + (11 * count) / 4);
            fixture(actor, count);
            for (unsigned i = 0; i < count; ++i) actual_opcode_pass(cpu, 0x1aa0 + i * 0x30, actor, true);
            assert(word(actor + 6) == 0xc000 && word(actor + 8) == 102);
            assert(word(actor + 10) == 0x4000 && word(actor + 12) == 199);
        }
    }
    interp816_free(cpu);
}
int main(int argc, char **argv) {
    assert(argc == 2); verify_original_opcodes(argv[1]);
#else
int main(void) {
#endif
    /* Five controller records exist even though the host exposes four pads. */
    for (unsigned actor = 0x500; actor <= 0x1000; actor += 0xb00) {
        for (unsigned count = 1; count <= 5; ++count) {
            fixture(actor, count);
            for (unsigned i = 0; i < count; ++i) human_pass(0x1aa0 + i * 0x30, actor, false);
            assert(word(actor + 8) == 100 + (11 * count) / 4);
            fixture(actor, count);
            for (unsigned i = 0; i < count; ++i) human_pass(0x1aa0 + i * 0x30, actor, true);
            assert(word(actor + 6) == 0xc000 && word(actor + 8) == 102);
            assert(word(actor + 10) == 0x4000 && word(actor + 12) == 199);
        }
    }
    /* Selecting the keeper during an earlier outfield pass must not consume
     * its one movement. Only an actual negative-status movement branch does. */
    fixture(0x500, 4); put(0x88, 0x1b00);
    memcpy(original, ram, sizeof ram);
    assert(!issd_bugfix_keeper_skip_movement(ram, 0x500, true));
    assert(issd_bugfix_keeper_skip_movement(ram, 0x500, true));
    assert(!memcmp(original, ram, sizeof ram));
    assert(!issd_bugfix_keeper_skip_movement(ram, 0x500, false));
    assert(!issd_bugfix_keeper_skip_movement(NULL, 0x500, true));
    /* Earlier CPU ownership does not count as a human movement pass. */
    put(0x1ace, 0x8000); put(0x1afe, 0x8000);
    issd_bugfix_keeper_begin_loop();
    assert(!issd_bugfix_keeper_skip_movement(ram, 0x500, true));
    put(0x1afe, 0); assert(issd_bugfix_keeper_skip_movement(ram, 0x500, true));
    put(0x1b2e, 0x8000); assert(!issd_bugfix_keeper_skip_movement(ram, 0x500, true));
    /* Distinct actors, outfield players, invalid loop cursors never qualify. */
    fixture(0x500, 3); put(0x88, 0x1ad0); put(0x1afc, 0x1000);
    assert(!issd_bugfix_keeper_skip_movement(ram, 0x1000, true));
    assert(!issd_bugfix_keeper_skip_movement(ram, 0x600, true));
    for (unsigned cursor = 0; cursor < 0x2000; ++cursor) {
        if (cursor >= 0x1aa0 && cursor <= 0x1b60 && (cursor - 0x1aa0) % 0x30 == 0) continue;
        put(0x88, cursor); assert(!issd_bugfix_keeper_skip_movement(ram, 0x500, true));
    }
    /* Gameplay mode is intentionally not gated: this exact cartridge branch
     * also governs goalkeeper ownership in training and other match modes. */
    fixture(0x500, 2); put(0x88, 0x1ad0);
    for (unsigned mode = 0; mode <= 12; ++mode) {
        issd_bugfix_keeper_begin_loop(); put(0xc0, mode);
        assert(!issd_bugfix_keeper_skip_movement(ram, 0x500, true));
        assert(issd_bugfix_keeper_skip_movement(ram, 0x500, true));
    }
    return 0;
}
