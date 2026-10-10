/* Diagnostic-only controller driver. Reads WRAM; never writes game state.
 * Linked into a separate acceptance executable, never the released game. */
#include "issd_script.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
extern uint8_t g_ram[0x20000];
extern void issd_probe_finish(unsigned frame);
static FILE *trace;
static FILE *controller_trace;
static int world;
static unsigned last_mode = ~0u, last_period = ~0u, last_cb = ~0u;
static unsigned terminal_frame;
static bool seen_live;
static bool seen_elimination;
static unsigned word(unsigned a) { return g_ram[a] | (unsigned)g_ram[a+1] << 8; }
static uint32_t gameplay(unsigned frame) {
  unsigned actor=word(0x1acc);
  if (actor<0x500 || actor>0x1a00 || (actor&255))
    return frame%160<20 ? ISSD_BTN_B : 0;
  int x=(int16_t)word(actor+0x2a), y=(int16_t)word(actor+0x2c);
  /* Original $849497/$8494A5 ownership tests, not $D8's actor cache. */
  bool possession=word(0xa4)==actor || word(0xa6)==actor;
  unsigned keeper=word(actor+0x9a)==0xd00 ? 0x1000 : 0x500;
  int target_x=possession ? (int16_t)word(keeper+0x8c) : (int16_t)word(0x42a);
  int target_y=possession ? (int16_t)word(keeper+0x8e)+32 : (int16_t)word(0x42c);
  if (frame%300==0) {
    fprintf(controller_trace,"{\"controller_frame\":%u,\"actor\":%u,\"owner\":[%u,%u],\"actor_xy\":[%d,%d],\"ball_xy\":[%d,%d],\"goal_xy\":[%d,%d],\"team\":%u,\"state\":%u,\"actor_word_96\":%u}\n",
      frame,actor,word(0xa4),word(0xa6),x,y,(int16_t)word(0x42a),(int16_t)word(0x42c),
      (int16_t)word(keeper+0x8c),(int16_t)word(keeper+0x8e),word(actor+0x9a),word(actor+0x32),word(actor+0x96));
    fflush(controller_trace);
  }
  /* The keeper cannot dribble out like an outfield player. Release a caught
   * ball using the original pass/high-ball input before chasing it again. */
  if (possession && (actor==0x500 || actor==0x1000))
    return frame%90<10 ? ISSD_BTN_A | (target_x>x ? ISSD_BTN_RIGHT : ISSD_BTN_LEFT) : 0;
  /* Short sprints conserve the original player's endurance across halves. */
  uint32_t mask=(possession || (abs(target_x-x)<240 && abs(target_y-y)<100)) && frame%180<20 ? ISSD_BTN_Y : 0;
  if (target_x>x+8) mask|=ISSD_BTN_RIGHT;
  else if (target_x<x-8) mask|=ISSD_BTN_LEFT;
  if (target_y>y+12) mask|=ISSD_BTN_DOWN;
  else if (target_y<y-12) mask|=ISSD_BTN_UP;
  /* Default logical X is Shoot; A is High ball (original $838000/$83F12B). */
  if (possession && abs(target_x-x)<220 && abs(target_y-y)<12 && frame%30<3) {
    mask &= ~(ISSD_BTN_UP | ISSD_BTN_DOWN | ISSD_BTN_Y);
    mask |= ISSD_BTN_X;
  }
  /* B is a slide tackle without the ball. Only tackle within reach, so
   * periodic slides do not stop a distant defender chasing the attack. */
  if (!possession && abs(target_x-x)<60 && abs(target_y-y)<36 && frame%30<3)
    mask|=ISSD_BTN_B;
  /* Original shoulder-button cursor selection can pick a nearer defender. */
  if (!possession && abs(target_x-x)>160 && frame%60<3)
    mask|=ISSD_BTN_R;
  /* No generic B mash: $BC is used by original dead-ball dispatch, not a
   * documented instruction to tackle or pass periodically during live play. */
  return mask;
}
int issd_script_load(const char *path) {
  world = path && path[0] == 'w';
  trace = fopen("progression.jsonl", "w");
  controller_trace = fopen("controller.jsonl", "w");
  if (!trace || !controller_trace) abort();
  return 1;
}
bool issd_script_active(void) { return true; }
uint8_t issd_script_players(void) { return 1; }
uint32_t issd_script_mask(unsigned frame) { return issd_script_mask_player(frame, 0); }
uint32_t issd_script_mask_player(unsigned frame, unsigned player) {
  if (player) return 0;
  unsigned mode=word(0x70), period=word(0xa8);
  unsigned cb=word(0x1446) | (unsigned)g_ram[0x1448]<<16;
  if (mode!=last_mode || period!=last_period || (mode==12 && cb!=last_cb) || frame%5000==0) {
    fprintf(trace,"{\"frame\":%u,\"mode\":%u,\"period\":%u,\"callback\":\"%06x\",\"stage\":%u,\"round\":%u,\"live_stage\":%u,\"live_round\":%u,\"clock\":%u,\"display_clock\":%u,\"score\":[%u,%u],\"opponent\":%u,\"game_mode\":%u,\"campaign_flags\":%u,\"duration_setting\":%u,\"difficulty_setting\":%u}\n",
            frame,mode,period,cb,word(0x1640),word(0x1652),word(0xddff),word(0xde11),word(0x16d0),word(0x16d2),word(0xda2),word(0xea2),word(0xea0),word(0x32),word(0x1648),word(0x1e5a),word(0x1e54));
    fflush(trace);
    last_mode=mode; last_period=period; last_cb=cb;
  }
  if (frame<4201) {
    if (frame>=60 && frame<371 && frame%60<10) return ISSD_BTN_START;
    /* Select Level1/three minutes in original Options. Campaign constructors
     * can override these choices; trace actual live settings, never assume
     * the selected exhibition options determine tournament rules. */
    if ((frame>=500 && frame<510) || (frame>=530 && frame<540) ||
        (frame>=560 && frame<570) || (frame>=980 && frame<990)) return ISSD_BTN_DOWN;
    if (frame>=600 && frame<610) return ISSD_BTN_RIGHT;
    if ((frame>=650 && frame<660) || (frame>=1100 && frame<1110)) return ISSD_BTN_A;
    if ((frame>=900 && frame<910) || (frame>=940 && frame<950) ||
        (frame>=1020 && frame<1030) || (frame>=1400 && frame<1410)) return ISSD_BTN_LEFT;
    if (frame>=1060 && frame<1070) return ISSD_BTN_B;
    if ((frame>=1440 && frame<1450) || (!world && frame>=1480 && frame<1490)) return ISSD_BTN_UP;
    unsigned choose=world ? 1480 : 1520;
    if (frame>=choose && frame<choose+10) return ISSD_BTN_A;
    if (frame>=1800 && frame<2600 && frame%100<10) return ISSD_BTN_A;
    if (frame>=2800 && frame<4100 && frame%200<10) return ISSD_BTN_A;
    return 0;
  }
  if (mode==8) seen_live=true;
  if (mode==12 && cb==0x8bc1f6) seen_elimination=true;
  /* World terminal standings and Cup return-to-menu/ceremony are original
   * transitions. Leave them settled long enough for native autosave. */
  bool main_menu = word(0x32)==6 && mode==12 && word(0x1538)==0x9d72 && word(0x153a)==0xa4;
  unsigned penalty_p1=g_ram[0xd442], penalty_p2=g_ram[0xd443];
  bool shootout_complete = period==3 && (word(0x1648)&8) &&
      penalty_p1!=penalty_p2 && word(0x1700)==(penalty_p1>penalty_p2 ? 0 : 2) &&
      word(0xddce)==word(penalty_p1>penalty_p2 ? 0xda0 : 0xea0);
  bool terminal = world ? (mode==12 && cb==0x8b94ee && word(0x1652)==35)
                        : (main_menu || (seen_elimination && mode==0) ||
                           (seen_live && mode==0x1a && (word(0x1648)&0x24)!=4) ||
                           (mode==12 && (cb==0x85d32e || (cb==0x8bc8c0 && shootout_complete)) && word(0x1640)==9));
  if (terminal && !terminal_frame) terminal_frame=frame;
  if (terminal_frame) {
    if (frame>=terminal_frame+120) issd_probe_finish(frame);
    return 0;
  }
  /* Original penalty kicks run through bank $8C's menu callback, not the
   * ordinary outfield actor state. Ordinary B pulses are the kick input. */
  if (!world && period==3 && mode==12 && (cb>>16)==0x8c)
    return ISSD_BTN_RIGHT | (frame%120<20 ? ISSD_BTN_B : 0);
  if (mode==8) {
    /* Controller-only player steers toward ball/goal and shoots normally. */
    return gameplay(frame);
  }
  if (mode==0x13) return frame%160<10 ? ISSD_BTN_START : 0;
  /* Original elimination input dispatch $8BC212 admits only START. */
  if (mode==12 && cb==0x8bc1f6) return frame%160<10 ? ISSD_BTN_START : 0;
  return frame%160<10 ? ISSD_BTN_A : 0;
}
