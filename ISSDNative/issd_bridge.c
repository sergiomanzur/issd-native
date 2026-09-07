#include "issd_bridge.h"
#include <stdio.h>
#include "cpu_state.h"
#include <string.h>

/* Access global WRAM from the SNESRecomp runtime */
extern uint8_t g_ram[0x20000];

/* Direct access to RAM variables based on RAM_Map_ISSD.asm */
#define RAM_BYTE(addr) (g_ram[(addr) & 0x1FFFF])
#define RAM_WORD(addr) ((uint16_t)(g_ram[(addr) & 0x1FFFF] | (g_ram[((addr) + 1) & 0x1FFFF] << 8)))

void issd_bridge_init(void) {
    /* $84:8000 is a large stateful gameplay update rooted in several runtime
     * jump tables. Its current AOT CFG can reach a deadline through a phantom
     * low-bank node, returning NORMAL before its JSL frame is consumed and
     * before D is restored to zero. The following player-update dispatch then
     * jumps through $17xx WRAM and encounters a stray RTI, leaving NMI busy and
     * the displayed frame permanently black/frozen. Keep this one routine on
     * the byte-exact tier until its indirect CFG can be represented soundly;
     * its callees can still bounce to compiled bodies. */
    static const uint32 k_exact_lle_roots[] = { 0x848000u };
    interp_bridge_set_lle_bounce_exclusions(
        k_exact_lle_roots,
        sizeof k_exact_lle_roots / sizeof k_exact_lle_roots[0]);
    printf("[ISSD Bridge] Initialized memory bridge\n");
}

void issd_bridge_update_state(IssdMatchState* out_state) {
    if (!out_state) return;

    out_state->game_mode1 = RAM_BYTE(0x0032);
    out_state->game_mode2 = RAM_BYTE(0x0070);
    out_state->p1_score = RAM_BYTE(0x0DA2);
    out_state->p2_score = RAM_BYTE(0x0EA2);
    out_state->p1_team = RAM_BYTE(0x0DA0);
    out_state->p2_team = RAM_BYTE(0x0EA0);
    out_state->timer_seconds = RAM_BYTE(0x16D0);
    out_state->timer_minutes = RAM_BYTE(0x16D1);
    out_state->weather = RAM_BYTE(0x1E4C);
    out_state->referee = RAM_BYTE(0x1E58);
    out_state->game_level = RAM_BYTE(0x1E54);
    out_state->foul_setting = RAM_BYTE(0x1E50);
    out_state->yellow_card_setting = RAM_BYTE(0x1E52);
    out_state->offside_setting = RAM_BYTE(0x1E4A);
    out_state->camera_x = RAM_WORD(0x13A0);
    out_state->camera_y = RAM_WORD(0x13B0);
    out_state->dog_referee_unlocked = (RAM_BYTE(0x7ED854) != 0);
    out_state->all_stars_unlocked = (RAM_BYTE(0x7ED856) != 0);
}

void issd_bridge_log_state(const IssdMatchState* state) {
    if (!state) return;
    printf("[ISSD State] Mode1: 0x%02X, Mode2: 0x%02X | P1 (%02X): %d - P2 (%02X): %d | Clock: %02d:%02d | Cam: (%d, %d)\n",
        state->game_mode1, state->game_mode2,
        state->p1_team, state->p1_score,
        state->p2_team, state->p2_score,
        state->timer_minutes, state->timer_seconds,
        state->camera_x, state->camera_y);
}
