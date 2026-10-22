/* ROM validation and pre-game configuration behavior adapted from
 * Mabec-games/iss-deluxed, commit 54344cb (GPL-3.0). Original UI lives in
 * issd_launcher.c; no fork artwork, online code or branding is included. */
#include "issd_launcher_model.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
bool issd_launcher_rom_ok(const char*path,char*why,size_t cap){
    static const char expected[]="d2fe66c1ce66c65ce14e478c94be2e616f9e2cad374b5783a6a64d3c1a99cfa9";
    const char*error="Select your original ISS Deluxe USA cartridge.";
    FILE*f=path&&path[0]?fopen(path,"rb"):NULL;
    if(f){
        if(fseek(f,0,SEEK_END)==0){long bytes=ftell(f);long header=bytes==2097664?512:0;
            if(bytes-header==2097152){
                uint8_t*image=malloc(2097152);uint8_t digest[32];char hex[65];
                if(image&&fseek(f,header,SEEK_SET)==0&&fread(image,1,2097152,f)==2097152){
                    sha256_compute(image,2097152,digest);
                    for(int i=0;i<32;i++)snprintf(hex+i*2,3,"%02x",digest[i]);
                    free(image);fclose(f);
                    if(!strcmp(hex,expected)){if(why&&cap)why[0]=0;return true;}
                    if(why&&cap)snprintf(why,cap,"Different revision or modified ROM. Original USA ROM required.");return false;
                }free(image);
            }
        }fclose(f);error="Unreadable or unsupported cartridge. Choose the original USA ROM.";
    }
    if(why&&cap)snprintf(why,cap,"%s",error);return false;
}
int issd_launcher_move(int f,int count,int d){if(count<=0)return 0;int n=(f+d)%count;return n<0?n+count:n;}
void issd_launcher_adjust(IssdConfig*c,int page,int row,int dir){
    if(!c)return;
    if(page==ISSD_LAUNCHER_GRAPHICS){switch(row){
        case 0:c->fullscreen=!c->fullscreen;break;
        case 1:c->aspect_ratio=(IssdAspectRatio)issd_launcher_move(c->aspect_ratio,ISSD_ASPECT_COUNT,dir);break;
        case 2:c->scaling_filter=(IssdScalingFilter)issd_launcher_move(c->scaling_filter,4,dir);break;
        case 3:c->widescreen_menus=!c->widescreen_menus;break;
        case 4:c->enhanced_running_animation=!c->enhanced_running_animation;break;
        case 5:c->master_volume+=dir*5;if(c->master_volume<0)c->master_volume=0;if(c->master_volume>100)c->master_volume=100;break;
        case 6:c->camera_mode=(IssdCameraMode)issd_launcher_move(c->camera_mode,3,dir);break;
    }}else if(page==ISSD_LAUNCHER_CONTROLS){
        if(row>=0&&row<4)issd_config_player_preset(c,row,issd_launcher_move(c->player_profiles[row].schema,3,dir));
    }else if(page==ISSD_LAUNCHER_GAMEPLAY){switch(row){
        case 0:c->gameplay_bug_fixes=!c->gameplay_bug_fixes;break;
        case 1:c->gameplay_goalkeeper_ai=!c->gameplay_goalkeeper_ai;break;
        case 2:c->gameplay_player_ai=!c->gameplay_player_ai;break;
        case 3:c->gameplay_short_goal_kicks=!c->gameplay_short_goal_kicks;break;
        case 4:c->gameplay_final_attack=!c->gameplay_final_attack;break;
        case 5:{static const int times[]={0,1,2,3,5,7,10,15,20};int i=0;while(i<8&&times[i]!=c->match_minutes)i++;
            c->match_minutes=times[issd_launcher_move(i,9,dir)];break;}
        case 6:c->team_strength=!c->team_strength;break;
    }}
}
