#include "issd_match_options.h"
#include "interp816.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char ram[0x20000],rom[0x200000];
static unsigned w(unsigned a){return ram[a]|(unsigned)ram[a+1]<<8;}
static void p(unsigned a,unsigned v){ram[a]=v;ram[a+1]=v>>8;}
static uint8_t rd(void *x,uint32_t a){(void)x;unsigned b=a>>16,o=a&65535;
 if((b&127)<64&&o>=32768)return rom[(b&127)*32768+(o&32767)];return ram[a&0x1ffff];}
static void wr(void*x,uint32_t a,uint8_t v){(void)x;ram[a&0x1ffff]=v;}
static unsigned penalty(Interp816*c,unsigned shooter,unsigned stale,unsigned routed,bool on){
 memset(ram,0,sizeof ram);p(0x32,6);p(0x70,12);p(0x1440,shooter);
 p(0x140e,stale);p(0x140c,routed);p(0x141a,0x8080);p(0x174c,51);
 interp816_reset(c);c->e=c->mf=c->xf=false;c->k=0x8c;c->db=0;c->dp=0xc00;c->pc=0x9708;
 for(unsigned i=0;i<10;i++){
  if(c->pc==0x970e)c->a=issd_penalty_timing_input(ram,c->a,on);
  if(c->pc==0x971f||c->pc==0x971e)return c->pc;
  interp816_runOpcode(c);
 }assert(!"penalty boundary");return 0;
}
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);assert(fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 Interp816*c=interp816_init(NULL,rd,wr);assert(c);
 for(unsigned side=0;side<=2;side+=2){
  assert(penalty(c,side,0x8080,0,false)==0x971f); /* original interference */
  assert(penalty(c,side,0x8080,0,true)==0x971e);
  assert(w(0x141a)==0x8080); /* actual keeper input untouched */
  assert(penalty(c,side,0,0x8080,true)==0x971f);
 }
 assert(issd_duration_bcd(0,0,0x300)==0x300);
 assert(issd_duration_bcd(1,0,0x300)==0x100);
 assert(issd_duration_bcd(20,0,0x300)==0x2000);
 assert(issd_duration_bcd(20,2,0x100)==0x700);
 assert(issd_duration_bcd(4,0,0x300)==0x300);
 memset(ram,0,sizeof ram);p(0x32,6);p(0x70,8);p(0x12a2,1792);p(0x12a4,576);
 p(0x45c,0x600);p(0x69a,0xd00);p(0xda4,0);p(0x42a,1600);p(0x42c,288);p(0x630,1);
 assert(issd_final_attack(ram,true));assert(!issd_final_attack(ram,false));
 p(0x16d6,599);assert(issd_final_attack(ram,true));p(0x16d6,600);assert(!issd_final_attack(ram,true));
 p(0x16d6,1);p(0x42a,600);assert(!issd_final_attack(ram,true));
 p(0xda4,0x8000);p(0x42a,100);assert(issd_final_attack(ram,true));
 p(0xbc,1);assert(!issd_final_attack(ram,true));
 interp816_free(c);puts("Original penalty interference reproduced and corrected; match policies passed");return 0;
}
