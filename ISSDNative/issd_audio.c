#include "issd_audio.h"
#include "snes/snes.h"
#include "snes/apu.h"
#include <stdio.h>
#include <string.h>

extern Snes *g_snes;
extern uint8_t g_ram[0x20000];
void RtlApuLock(void);
void RtlApuUnlock(void);

static uint32_t s_sfx_dispatched = 0;
static uint32_t s_voice_dispatched = 0;
static const char *s_last_voice_desc = "None";

static const char *k_voice_names[75] = {
    "None",                     /* 0x00 */
    "Corner Kick",              /* 0x01 */
    "Goal Kick",                /* 0x02 */
    "Throw In",                 /* 0x03 */
    "Free Kick",                /* 0x04 */
    "Penalty Kick",             /* 0x05 */
    "Offside",                  /* 0x06 */
    "Replay",                   /* 0x07 */
    "Half Time",                /* 0x08 */
    "Injury Time",              /* 0x09 */
    "Great Save!",              /* 0x0A */
    "Good Ball!",               /* 0x0B */
    "Long Ball",                /* 0x0C */
    "Great Cross!",             /* 0x0D */
    "Good Header!",             /* 0x0E */
    "Great Tackle!",            /* 0x0F */
    "On The Volley!",           /* 0x10 */
    "Amazing Kick!",            /* 0x11 */
    "Good Touch!",              /* 0x12 */
    "You Lose",                 /* 0x13 */
    "You Win",                  /* 0x14 */
    "Match Draw",               /* 0x15 */
    "Player Change",            /* 0x16 */
    "Good Clearance!",          /* 0x17 */
    "Keeper Fumble",            /* 0x18 */
    "Over The Bar!",            /* 0x19 */
    "Past The Post!",           /* 0x1A */
    "Through Ball",             /* 0x1B */
    "Interception!",            /* 0x1C */
    "Nice Dummy!",              /* 0x1D */
    "Great Punch!",             /* 0x1E */
    "Blocked Shot!",            /* 0x1F */
    "No Foul",                  /* 0x20 */
    "It's One On One!",         /* 0x21 */
    "One-Two!",                 /* 0x22 */
    "Dirty Play Ref!",          /* 0x23 */
    "Down The Wing!",           /* 0x24 */
    "Semi Final",               /* 0x25 */
    "Final",                    /* 0x26 */
    "Kickoff",                  /* 0x27 */
    "Foul!",                    /* 0x28 */
    "Yellow Card!",             /* 0x29 */
    "Red Card!",                /* 0x2A */
    "He's Off!",                /* 0x2B */
    "He Shoots!",               /* 0x2C */
    "It's A Big Kick!",         /* 0x2D */
    "Incredible Control!",      /* 0x2E */
    "GOAL!",                    /* 0x2F */
    "Go Go Go!",                /* 0x30 */
    "Oh No!",                   /* 0x31 */
    "What A Goal!",             /* 0x32 */
    "Dangerous Play!",          /* 0x33 */
    "Golden Goal!",             /* 0x34 */
    "Extra Time",               /* 0x35 */
    "What A Miss!",             /* 0x36 */
    "Heading For Goal!",        /* 0x37 */
    "Saved by the Keeper!",     /* 0x38 */
    "Cleared Off The Line!",    /* 0x39 */
    "Hit The Crossbar!",        /* 0x3A */
    "Hit The Post!",            /* 0x3B */
    "Free Kick In Box!",        /* 0x3C */
    "Direct Free Kick!",        /* 0x3D */
    "Indirect Free Kick!",      /* 0x3E */
    "Wall Is Set",              /* 0x3F */
    "Advantage Played",         /* 0x40 */
    "Substitute Player",        /* 0x41 */
    "Full Time Whistle",        /* 0x42 */
    "Penalty Shootout",         /* 0x43 */
    "Sudden Death",             /* 0x44 */
    "World Champions!",         /* 0x45 */
    "Victory Celebration",      /* 0x46 */
    "Cup Winners!",             /* 0x47 */
    "International Superstar Soccer DELUXE!", /* 0x48 */
    "Konami Intro Jingle",      /* 0x49 */
    "Stadium Crowd Roar"        /* 0x4A */
};

const char *Issd_GetVoiceName(uint16_t voice_id) {
    if (voice_id < sizeof(k_voice_names) / sizeof(k_voice_names[0])) {
        return k_voice_names[voice_id];
    }
    return "Unknown Voice Line";
}

const char *Issd_GetSfxName(uint8_t sfx_id) {
    switch (sfx_id) {
        case 0x01: return "Cursor Move";
        case 0x02: return "Menu Select";
        case 0x03: return "Menu Cancel";
        case 0x04: return "Menu Error";
        case 0x05: return "Short Whistle";
        case 0x06: return "Long Whistle";
        case 0x07: return "Double Whistle";
        case 0x08: return "Ball Kick (Ground)";
        case 0x09: return "Ball Kick (Hard)";
        case 0x0A: return "Ball Bounce";
        case 0x0B: return "Ball Catch";
        case 0x0C: return "Player Slide";
        case 0x0D: return "Player Tackle";
        case 0x0E: return "Player Fall";
        case 0x0F: return "Net Ripple (Goal)";
        case 0x10: return "Crowd Cheer (Soft)";
        case 0x11: return "Crowd Cheer (Loud)";
        case 0x12: return "Crowd Chant";
        case 0x13: return "Crowd Ooh/Aah";
        case 0x14: return "Post Rebound (Thud)";
        case 0x15: return "Goal Horn / Siren";
        default:   return "Sound Effect";
    }
}

void Issd_AudioGetTelemetry(uint32_t *out_sfx_count, uint32_t *out_voice_count, const char **out_last_voice) {
    if (out_sfx_count) *out_sfx_count = s_sfx_dispatched;
    if (out_voice_count) *out_voice_count = s_voice_dispatched;
    if (out_last_voice) *out_last_voice = s_last_voice_desc;
}

bool Issd_HlePumpAudio(CpuState *cpu) {
    if (!cpu || !g_snes || !g_snes->apu) return false;

    RtlApuLock();

    /* 1. Check if previous command in $4C is acknowledged */
    uint8_t in_flight = g_ram[0x4C];
    if (in_flight != 0) {
        if (g_snes->apu->outPorts[0] == in_flight) {
            g_ram[0x4C] = 0;
            apu_writePortNow(g_snes->apu, 0, 0);
        } else {
            apu_writePortNow(g_snes->apu, 0, in_flight);
        }
    }

    /* 2. Drain next pending sound effect from FIFO $7EE680 */
    if (g_ram[0x4C] == 0) {
        uint8_t rd = g_ram[0x4E];
        uint8_t wr = g_ram[0x4F];
        if (rd != wr) {
            uint8_t sfx = g_ram[0xE680 + rd];
            g_ram[0x4E] = (uint8_t)(rd + 1);
            g_ram[0x4C] = sfx;
            s_sfx_dispatched++;

            apu_writePortNow(g_snes->apu, 0, sfx);
            /* Fast-step SPC to acknowledge without CPU spin-waiting */
            for (int i = 0; i < 256; i++) {
                if (g_snes->apu->outPorts[0] == sfx) break;
                apu_cycle(g_snes->apu);
            }
        }
    }

    RtlApuUnlock();

    /* Restore CPU registers to expected post-RTL state: REP #$30 */
    cpu->m_flag = 0;
    cpu->x_flag = 0;
    cpu->P &= ~0x30;

    return true;
}

bool Issd_HlePlayVoice(CpuState *cpu) {
    if (!cpu || !g_snes || !g_snes->apu) return false;

    uint16_t voice_id = cpu->A;
    s_last_voice_desc = Issd_GetVoiceName(voice_id);
    s_voice_dispatched++;

    RtlApuLock();

    uint8_t port2_val = (uint8_t)(voice_id & 0xFF);
    uint8_t port1_cmd = (uint8_t)(0xF0 | (((voice_id >> 8) & 0x03) << 1));

    /* Apply port writes to SPC */
    apu_writePortNow(g_snes->apu, 2, port2_val);
    apu_writePortNow(g_snes->apu, 1, port1_cmd);

    /* Fast-step SPC until it echoes Port 1 */
    for (int i = 0; i < 512; i++) {
        if (g_snes->apu->outPorts[1] == port1_cmd) break;
        apu_cycle(g_snes->apu);
    }
    g_snes->apu->outPorts[1] = port1_cmd;

    /* Write acknowledge back to Port 1 */
    apu_writePortNow(g_snes->apu, 1, 0);

    RtlApuUnlock();

    /* Reset pending voice request */
    g_ram[0x1D2E] = 0;
    g_ram[0x1D2F] = 0;
    g_ram[0x1D30] = 1;
    g_ram[0x1D31] = 0;

    /* Restore CPU registers to expected post-RTL state: REP #$30 */
    cpu->m_flag = 0;
    cpu->x_flag = 0;
    cpu->P &= ~0x30;

    return true;
}

bool Issd_HleStopSound(CpuState *cpu) {
    if (!cpu || !g_snes || !g_snes->apu) return false;

    RtlApuLock();

    /* Command $EF is Konami's SPC mute/silence command */
    apu_writePortNow(g_snes->apu, 0, 0xEF);
    for (int i = 0; i < 256; i++) {
        if (g_snes->apu->outPorts[0] == 0xEF) break;
        apu_cycle(g_snes->apu);
    }
    g_snes->apu->outPorts[0] = 0xEF;

    RtlApuUnlock();

    /* Clean WRAM sound queue state */
    g_ram[0x4C] = 0;
    g_ram[0x4E] = 0;
    g_ram[0x4F] = 0;
    g_ram[0x1D00] = 0;
    g_ram[0x1D01] = 0;

    /* Restore CPU registers: REP #$20 (M=0) */
    cpu->m_flag = 0;
    cpu->P &= ~0x20;

    return true;
}
