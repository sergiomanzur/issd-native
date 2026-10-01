#include <assert.h>
#include <stddef.h>
#include "common_rtl.h"
#include "common_cpu_infra.h"
#include "cpu_state.h"
#include "snes/snes.h"
#include "snes/dsp.h"
#include "snes/spc.h"

static Snes machine;
static Cpu cpu;
static Apu apu;
static Dsp dsp;
static Spc spc;
static Dma dma;
static Ppu ppu;
static Cart cart;
static unsigned char ram[0x20000];
static unsigned char sram[32];
Snes *g_snes = &machine;
CpuState g_cpu;
int snes_frame_counter;
static unsigned host_extra;
static unsigned reconciled;
static unsigned presentation_resets;
static unsigned locks;
static bool overread_extra;
const RtlGameInfo *g_rtl_game_info;

void RtlApuLock(void) { ++locks; }
void RtlApuUnlock(void) { assert(locks); --locks; }
void PpuResetWidescreenOamHistory(Ppu *p) { (void)p; ++presentation_resets; }
void cart_saveload(Cart *c, SaveLoadInfo *s) { s->func(s, c->ram, c->ramSize); }
static void save_extra(SaveLoadInfo *s) { s->func(s, &host_extra, sizeof(host_extra)); }
static void load_extra(SaveLoadInfo *s, uint32_t version) {
    (void)version;
    s->func(s, &host_extra, sizeof(host_extra));
    if (overread_extra) {
        uint8_t unexpected = 0;
        s->func(s, &unexpected, sizeof(unexpected));
    }
}
static void reconcile(uint32_t version) { (void)version; ++reconciled; }
static const RtlGameInfo game = {
    .state_save_extra = save_extra, .state_load_extra = load_extra,
    .on_state_loaded = reconcile,
};
