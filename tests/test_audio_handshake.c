#include <assert.h>
#include "cpu_state.h"
#include "snes/snes.h"
#include "snes/spc.h"
#include "snes/snes_regs.h"

static Snes snes;
static Snes *g_snes = &snes;
static uint64_t guest_cycle;
static void RtlApuLock(void) {}
static void RtlApuUnlock(void) {}
void rtl_sync_apu_to_cpu_locked(void) {
  assert(apu_runToGuestCycle(g_snes->apu, guest_cycle, 1u << 20));
}

/* PRODUCTION HANDSHAKE */

int main(int argc, char **argv) {
  assert(argc == 2);
  Apu *apu = snes.apu = apu_init();
  apu_reset(apu);
  apu->spc->pc = 0x200;
  apu->spc->p = false;
  CpuState cpu = {0};

  if (!strcmp(argv[1], "cleared-command")) {
    /* ISSD's SFX acknowledgement ($0E3F) clears BOTH input ports via
     * CONTROL=$11 while the CPU is trying to send voice command $F0. */
    const uint8_t program[] = {
      0xE4,0xF4, 0xC4,0xF4, 0x8F,0x11,0xF1,
      0xE4,0xF5, 0x68,0xF0, 0xD0,0xFA,
      0x8F,0x01,0x20, 0xC4,0xF5, 0x2F,0xFE,
    };
    memcpy(apu->ram + 0x200, program, sizeof(program));
    apu_writePortNow(apu, 0, 0x18);
    assert(RtlApuWriteWaitEcho(&cpu, APUI01, 0xF0, false));
    assert(apu->ram[0x20] == 1 && "SPC must actually receive the voice command");
    assert(apu->portClock < 1024 && "normal handshake must not hit timeout");
    assert(cpu.open_bus == 0xF0);
  } else if (!strcmp(argv[1], "queued-release")) {
    /* The voice driver ($0AA8) must observe zero between $F0 and $CC.
     * A pending CPU write is a timed bus event, not just its final value. */
    const uint8_t program[] = {
      0xEB,0xF5, 0xD0,0xFC, 0xCB,0xF5, 0x8F,0x01,0x20,
      0xE4,0xF5, 0x68,0xCC, 0xD0,0xFA,
      0x8F,0x01,0x21, 0xC4,0xF5, 0x2F,0xFE,
    };
    memcpy(apu->ram + 0x200, program, sizeof(program));
    apu_writePortNow(apu, 1, 0xF0);
    apu->outPorts[1] = 0xF0;
    assert(apu_schedulePortWrite(apu, 1, 0, 100));
    guest_cycle = 200;
    assert(RtlApuWriteWaitEcho(&cpu, APUI01, 0xCC, false));
    assert(apu->ram[0x20] == 1 && "SPC must see the queued release");
    assert(apu->ram[0x21] == 1 && "SPC must actually receive the next command");
    assert(apu->portClock < 1024);
  } else if (!strcmp(argv[1], "post-echo-clear")) {
    const uint8_t program[] = {
      0xE4,0xF5, 0xC4,0xF5, 0x8F,0x11,0xF1, 0x2F,0xFE,
    };
    memcpy(apu->ram + 0x200, program, sizeof(program));
    assert(RtlApuWriteWaitEcho(&cpu, APUI01, 0xF0, false));
    assert(apu->outPorts[1] == 0xF0);
    assert(apu->inPorts[1] == 0 && "do not resend an acknowledged command");
    assert(apu->portClock < 1024);
  } else {
    assert(!strcmp(argv[1], "word-echo") || !strcmp(argv[1], "cleared-word"));
    /* Preserve word acknowledgements and high-byte open bus semantics. */
    const uint8_t program[] = {
      0x8F,0x11,0xF1, /* Clear both bytes before polling them. */
      0xE4,0xF4, 0x68,0x34, 0xD0,0xFA,
      0xE4,0xF5, 0x68,0x12, 0xD0,0xFA,
      0xE4,0xF4, 0xC4,0xF4, 0xE4,0xF5, 0xC4,0xF5, 0x2F,0xFE,
    };
    memcpy(apu->ram + 0x200, program, sizeof(program));
    if (!strcmp(argv[1], "word-echo")) apu->spc->pc += 3;
    assert(RtlApuWriteWaitEcho(&cpu, APUI00, 0x1234, true));
    assert(apu->outPorts[0] == 0x34 && apu->outPorts[1] == 0x12);
    assert(cpu.open_bus == 0x12 && apu->portClock < 1024);
  }
  apu_free(apu);
  return 0;
}
