/* Input-only exhibition acceptance driver. Guest RAM is read-only. */
#include "issd_script.h"
#include <stdio.h>
#include <stdlib.h>
extern uint8_t g_ram[0x20000];
extern void issd_probe_finish(unsigned frame);
static FILE *controller_trace,*trace;
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
#define issd_script_load production_load
#define issd_script_active production_active
#define issd_script_mask production_mask
#define issd_script_mask_player production_mask_player
#define issd_script_players production_players
uint32_t production_mask_player(uint32_t frame,unsigned player);
#include "issd_script.c"
#undef issd_script_load
#undef issd_script_active
#undef issd_script_mask
#undef issd_script_mask_player
#undef issd_script_players
static unsigned last_mode=~0u,last_period=~0u,replay_start,terminal_frame;
static bool second;
int issd_script_load(const char *path) {
  trace=fopen("match-events.jsonl","w");controller_trace=fopen("controller.jsonl","w");
  if (!trace || !controller_trace) return 0;
  return production_load(path);
}
bool issd_script_active(void){return production_active();}
uint8_t issd_script_players(void){return 1;}
uint32_t issd_script_mask(uint32_t frame){return issd_script_mask_player(frame,0);}
uint32_t issd_script_mask_player(uint32_t frame,unsigned player){
  if(player)return 0;
  unsigned mode=word(0x70),period=word(0xa8);
  if(mode!=last_mode || period!=last_period || frame%1000==0){
    fprintf(trace,"{\"frame\":%u,\"mode\":%u,\"period\":%u,\"clock\":%u,\"display_clock\":%u,\"id\":%u,\"base\":%u,\"length\":%u,\"width\":%u,\"score\":[%u,%u]}\n",
      frame,mode,period,word(0x16d0),word(0x16d2),word(0x1fa2),word(0x86),word(0x12a2),word(0x12a4),word(0xda2),word(0xea2));
    fflush(trace);last_mode=mode;last_period=period;
  }
  if(frame<6000)return production_mask_player(frame,0);
  if(mode==8 && period==1)second=true;
  if(second && mode==0x12 && period==1){
    if(!terminal_frame)terminal_frame=frame;
    if(frame>=terminal_frame+120)issd_probe_finish(frame);
    return 0;
  }
  if(mode==8)return gameplay(frame);
  if(mode==0x13){
    if(!replay_start)replay_start=frame;
    return frame-replay_start>480 && frame%160<10 ? ISSD_BTN_START : 0;
  }
  replay_start=0;
  return frame%160<10 ? ISSD_BTN_A : 0;
}
