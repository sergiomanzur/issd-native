/* Original Matchday broadcast presentation for ISSD Native.
 * Pre-game behavior informed by Mabec-games/iss-deluxed (54344cb, GPL-3.0).
 * All graphics below are drawn in code; no fork artwork is used. */
#include "issd_launcher.h"
#include "issd_menu.h"
#include "issd_android.h"
#include "issd_mod.h"
#include "issd_hd.h"
#include "issd_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

enum { VW=960,VH=600,MAX_FILES=1024 };
typedef struct { char name[256];bool directory; } Entry;
typedef struct {
    IssdConfig *cfg;char *rom;size_t capacity;
    int page,focus,home,scroll,count;bool valid,done,play,picking,suspended;
    unsigned import_generation;
    char status[160],folder[1024];Entry entries[MAX_FILES];
    SDL_Renderer *renderer;SDL_Rect viewport;float scale;
} Launcher;
static const SDL_Color navy={12,25,43,255},cream={243,232,204,255},amber={255,188,84,255},
    muted={146,170,181,255},green={24,83,65,255};
static void color(SDL_Renderer*r,SDL_Color c){SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);}
static void box(SDL_Renderer*r,int x,int y,int w,int h,SDL_Color c){SDL_Rect a={x,y,w,h};color(r,c);SDL_RenderFillRect(r,&a);}
static void line(SDL_Renderer*r,int x,int y,int xx,int yy,SDL_Color c){color(r,c);SDL_RenderDrawLine(r,x,y,xx,yy);}
static void text(SDL_Renderer*r,int x,int y,const char*s,int scale,SDL_Color c,int max){
    color(r,c);
    for(int n=0;*s&&n<max;s++,n++,x+=8*scale){unsigned ch=(unsigned char)*s;if(ch<32||ch>127)ch='?';
        for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)if(g_issd_font8x8[ch-32][yy]&(0x80>>xx)){
            SDL_Rect p={x+xx*scale,y+yy*scale,scale,scale};SDL_RenderFillRect(r,&p);}
    }
}
static const char*base_name(const char*p){const char*b=p;for(;*p;p++)if(*p=='/'||*p=='\\')b=p+1;return b;}
static bool join(char*out,size_t cap,const char*folder,const char*name){
    int n=snprintf(out,cap,"%s%s%s",folder,folder[0]&&folder[strlen(folder)-1]!='/'?"/":"",name);
    return n>=0&&(size_t)n<cap;
}
static bool rom_name(const char*s){size_t n=strlen(s);if(n<4)return false;
    char ext[5];for(int i=0;i<4;i++)ext[i]=(char)tolower((unsigned char)s[n-4+i]);ext[4]=0;
    return !strcmp(ext,".sfc")||!strcmp(ext,".smc");}
static int compare(const void*aa,const void*bb){const Entry*a=aa,*b=bb;
    if(a->directory!=b->directory)return a->directory?-1:1;return strcmp(a->name,b->name);}
static void add_entry(Launcher*l,const char*name,bool directory){
    if(l->count>=MAX_FILES||strlen(name)>=sizeof l->entries[0].name||!strcmp(name,".")||!strcmp(name,".."))return;
    if(!directory&&!rom_name(name))return;
    Entry*e=&l->entries[l->count++];snprintf(e->name,sizeof e->name,"%s",name);e->directory=directory;
}
static void browse(Launcher*l){
    l->count=0;l->focus=l->scroll=0;l->page=ISSD_LAUNCHER_BROWSER;
    snprintf(l->entries[0].name,sizeof l->entries[0].name,"..");l->entries[0].directory=true;l->count=1;
#ifdef _WIN32
    if(!l->folder[0]) {
        DWORD drives=GetLogicalDrives();
        for(int i=0;i<26;i++)if(drives&(1u<<i)){char drive[4]={(char)('A'+i),':','/',0};add_entry(l,drive,true);}
    }
    char pattern[1200];WIN32_FIND_DATAA data;
    if(l->folder[0]&&join(pattern,sizeof pattern,l->folder,"*")){HANDLE h=FindFirstFileA(pattern,&data);
        if(h!=INVALID_HANDLE_VALUE){do{add_entry(l,data.cFileName,(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0);}while(FindNextFileA(h,&data));FindClose(h);}}
#else
    DIR*d=opendir(l->folder);if(d){struct dirent*e;while((e=readdir(d))){char path[1400];struct stat info;
        if(join(path,sizeof path,l->folder,e->d_name)&&!stat(path,&info))add_entry(l,e->d_name,S_ISDIR(info.st_mode));}closedir(d);}
#endif
    qsort(l->entries+1,(size_t)(l->count-1),sizeof(Entry),compare);
    snprintf(l->status,sizeof l->status,"Browse folders with A / Enter. B / Escape returns.");
}
static void choose(Launcher*l,const char*path){
    char why[160];if(!issd_launcher_rom_ok(path,why,sizeof why)){
        l->valid=issd_launcher_rom_ok(l->rom,NULL,0);
        snprintf(l->status,sizeof l->status,"%s",why);return;}
    if(strlen(path)>=l->capacity){snprintf(l->status,sizeof l->status,"That file path is too long.");return;}
    /* Candidate may alias the current path. */
    char copy[1024];snprintf(copy,sizeof copy,"%s",path);snprintf(l->rom,l->capacity,"%s",copy);
    snprintf(l->cfg->rom_path,sizeof l->cfg->rom_path,"%s",copy);l->valid=true;
    l->page=ISSD_LAUNCHER_HOME;l->home=l->focus=0;
    snprintf(l->status,sizeof l->status,"Cartridge verified. Ready for kickoff.");
}
static void select_rom(Launcher*l){
#ifdef ISSD_ANDROID
    l->import_generation=issd_android_rom_generation();
    issd_android_pick_rom();l->picking=true;
    snprintf(l->status,sizeof l->status,"Choose a cartridge in Android's document picker.");
#else
    /* Built-in browser also works in Deck Gaming Mode without zenity. */
    browse(l);
#endif
}
static int rows(const Launcher*l){
    switch(l->page){case ISSD_LAUNCHER_HOME:return 7;case ISSD_LAUNCHER_GRAPHICS:return 8;
        case ISSD_LAUNCHER_CONTROLS:return 5;case ISSD_LAUNCHER_GAMEPLAY:return 8;
        case ISSD_LAUNCHER_MODS:return issd_mod_get_pack_count()+issd_hd_available_count()+1;
        case ISSD_LAUNCHER_BROWSER:return l->count+1;default:return 1;}
}
static void back(Launcher*l){if(l->page==ISSD_LAUNCHER_HOME){l->done=true;return;}
    l->page=ISSD_LAUNCHER_HOME;l->focus=l->home;l->scroll=0;}
static void modify(Launcher*l,int direction){
    if(l->page==ISSD_LAUNCHER_MODS){int packs=issd_mod_get_pack_count();
        if(l->focus<packs)issd_mod_set_pack_enabled(l->focus,!issd_mod_is_pack_enabled(l->focus));
        else if(l->focus<packs+issd_hd_available_count())issd_hd_set_enabled(l->focus-packs,!issd_hd_is_enabled(l->focus-packs));
        issd_mod_enabled_list(l->cfg->active_mod_packs,sizeof l->cfg->active_mod_packs);
        issd_hd_enabled_list(l->cfg->hd_texture_packs,sizeof l->cfg->hd_texture_packs);
    }else issd_launcher_adjust(l->cfg,l->page,l->focus,direction);
}
static void activate(Launcher*l){
    if(l->page==ISSD_LAUNCHER_HOME){l->home=l->focus;
        switch(l->focus){case 0:if(l->picking){snprintf(l->status,sizeof l->status,"Wait for cartridge import to finish.");}
            else if(l->valid){l->play=l->done=true;}else select_rom(l);break;
            case 1:select_rom(l);break;case 2:l->page=ISSD_LAUNCHER_GRAPHICS;break;
            case 3:l->page=ISSD_LAUNCHER_CONTROLS;break;case 4:l->page=ISSD_LAUNCHER_GAMEPLAY;break;
            case 5:l->page=ISSD_LAUNCHER_MODS;break;case 6:l->done=true;break;}
        if(l->page!=ISSD_LAUNCHER_HOME)l->focus=l->scroll=0;return;
    }
    if(l->focus==rows(l)-1){back(l);return;}
    if(l->page==ISSD_LAUNCHER_BROWSER){Entry e=l->entries[l->focus];char path[1024];
        if(!strcmp(e.name,"..")){
#ifdef _WIN32
            if(strlen(l->folder)==3&&l->folder[1]==':'){l->folder[0]=0;browse(l);return;}
#endif
            size_t n=strlen(l->folder);while(n>1&&l->folder[n-1]=='/')l->folder[--n]=0;
            char*slash=strrchr(l->folder,'/');if(slash&&slash!=l->folder)*slash=0;else if(slash)slash[1]=0;
#ifdef _WIN32
            if(strlen(l->folder)==2&&l->folder[1]==':')strcat(l->folder,"/");
#endif
            browse(l);
        }else if(join(path,sizeof path,l->folder,e.name)){
            if(e.directory){snprintf(l->folder,sizeof l->folder,"%s",path);browse(l);}else choose(l,path);
        }return;
    }modify(l,1);
}
static void action(Launcher*l,const char*command){
    if(!strcmp(command,"UP"))l->focus=issd_launcher_move(l->focus,rows(l),-1);
    else if(!strcmp(command,"DOWN"))l->focus=issd_launcher_move(l->focus,rows(l),1);
    else if(!strcmp(command,"LEFT"))modify(l,-1);
    else if(!strcmp(command,"RIGHT"))modify(l,1);
    else if(!strcmp(command,"CONFIRM"))activate(l);
    else if(!strcmp(command,"BACK"))back(l);
    else if(!strcmp(command,"QUIT"))l->done=true;
    if(l->focus<l->scroll)l->scroll=l->focus;
    if(l->focus>=l->scroll+8)l->scroll=l->focus-7;
}
static void label(Launcher*l,int row,char*out,size_t cap){
    IssdConfig*c=l->cfg;
    if(row==rows(l)-1){snprintf(out,cap,l->page==ISSD_LAUNCHER_HOME?"QUIT":"BACK");return;}
    if(l->page==ISSD_LAUNCHER_HOME){static const char*names[]={"KICK OFF","CHOOSE ROM","PRESENTATION","CONTROLS","GAMEPLAY","MODS"};snprintf(out,cap,"%s",names[row]);}
    else if(l->page==ISSD_LAUNCHER_GRAPHICS){static const char*aspects[]={"4:3","SQUARE PIXELS","16:9","16:10","21:9","INTEGER","AUTHENTIC","FIT PIXEL PERFECT"};
        static const char*filters[]={"NEAREST","LINEAR","CRT","SHARP"};static const char*cameras[]={"CLASSIC","TACTICAL","TACTICAL WIDE"};
        switch(row){case 0:snprintf(out,cap,"FULLSCREEN     %s",c->fullscreen?"ON":"OFF");break;
            case 1:snprintf(out,cap,"VIEW   %s",aspects[c->aspect_ratio%ISSD_ASPECT_COUNT]);break;
            case 2:snprintf(out,cap,"FILTER         %s",filters[c->scaling_filter%4]);break;
            case 3:snprintf(out,cap,"WIDE MENUS     %s",c->widescreen_menus?"ON":"OFF");break;
            case 4:snprintf(out,cap,"ENHANCED POSES %s",c->enhanced_running_animation?"ON":"OFF");break;
            case 5:snprintf(out,cap,"VOLUME         %d%%",c->master_volume);break;
            case 6:snprintf(out,cap,"CAMERA   %s",cameras[c->camera_mode%3]);break;}}
    else if(l->page==ISSD_LAUNCHER_CONTROLS){static const char*p[]={"CLASSIC","FIFA","PES","CUSTOM"};
        snprintf(out,cap,"PLAYER %d      %s",row+1,p[c->player_profiles[row].schema%4]);}
    else if(l->page==ISSD_LAUNCHER_GAMEPLAY){
        static const char*names[]={"ORIGINAL BUG FIXES","GOALKEEPER AI","PLAYER AI","SHORT GOAL KICKS","FINAL ATTACK","MATCH TIME","TEAM RATINGS"};
        bool enabled[]={c->gameplay_bug_fixes,c->gameplay_goalkeeper_ai,c->gameplay_player_ai,c->gameplay_short_goal_kicks,c->gameplay_final_attack,false,c->team_strength};
        if(row==5){if(c->match_minutes)snprintf(out,cap,"MATCH TIME     %d MIN",c->match_minutes);else snprintf(out,cap,"MATCH TIME     ORIGINAL");}
        else snprintf(out,cap,"%s  %s",names[row],enabled[row]?"ON":"OFF");
    }else if(l->page==ISSD_LAUNCHER_MODS){int packs=issd_mod_get_pack_count();
        if(row<packs){IssdModPack*p=issd_mod_get_pack(row);snprintf(out,cap,"[%c] %.26s",issd_mod_is_pack_enabled(row)?'X':' ',p?p->name:"?");}
        else snprintf(out,cap,"[%c] %.26s",issd_hd_is_enabled(row-packs)?'X':' ',issd_hd_available_name(row-packs));
    }else if(l->page==ISSD_LAUNCHER_BROWSER)snprintf(out,cap,"%s%.28s",l->entries[row].directory?"+ ":"  ",l->entries[row].name);
}
static void draw(Launcher*l,SDL_Window*window){
    SDL_Renderer*r=l->renderer;int ow,oh,ww,wh,left=0,top=0,right=0,bottom=0;
    SDL_GetRendererOutputSize(r,&ow,&oh);SDL_GetWindowSize(window,&ww,&wh);
    issd_android_safe_insets(&left,&top,&right,&bottom);
    if(ww>0&&wh>0){left=left*ow/ww;right=right*ow/ww;top=top*oh/wh;bottom=bottom*oh/wh;}
    int available_w=ow-left-right,available_h=oh-top-bottom;
    l->scale=(float)available_w/VW;if((float)available_h/VH<l->scale)l->scale=(float)available_h/VH;
    if(l->scale<=0)l->scale=1;
    l->viewport=(SDL_Rect){left+(available_w-(int)(VW*l->scale))/2,top+(available_h-(int)(VH*l->scale))/2,(int)(VW*l->scale),(int)(VH*l->scale)};
    SDL_RenderSetViewport(r,NULL);SDL_RenderSetScale(r,1,1);color(r,navy);SDL_RenderClear(r);
    SDL_RenderSetViewport(r,&l->viewport);SDL_RenderSetScale(r,l->scale,l->scale);
    box(r,0,0,VW,8,amber);text(r,36,31,"MATCHDAY / 16-BIT FOOTBALL",2,muted,50);
    text(r,34,66,"ISSD NATIVE",4,cream,30);
    box(r,789,37,135,30,amber);text(r,807,44,"ON AIR",2,navy,8);
    line(r,36,125,924,125,muted);
    text(r,36,150,"THE TOUCHLINE",2,amber,18);
    int original_page=l->page;
    for(int i=0;i<7;i++){char caption[64];l->page=ISSD_LAUNCHER_HOME;label(l,i,caption,sizeof caption);l->page=original_page;
        bool selected=(original_page==ISSD_LAUNCHER_HOME?l->focus:l->home)==i;
        if(selected)box(r,30,184+i*43,280,35,amber);
        text(r,42,193+i*43,caption,2,selected?navy:cream,17);}
    line(r,326,148,326,505,muted);
    if(l->page==ISSD_LAUNCHER_HOME){
        text(r,352,150,"YOUR NEXT MATCH STARTS HERE",2,cream,35);
        box(r,354,192,570,229,green);
        for(int x=354;x<924;x+=48)box(r,x,192,24,229,(SDL_Color){27,90,70,255});
        SDL_Rect pitch={378,208,522,196};color(r,cream);SDL_RenderDrawRect(r,&pitch);
        line(r,639,208,639,404,cream);SDL_Rect area={378,256,70,100};SDL_RenderDrawRect(r,&area);area.x=830;SDL_RenderDrawRect(r,&area);
        for(int d=-32;d<32;d++){int yy=(int)SDL_sqrt(32*32-d*d);line(r,639+d,306-yy,639+d,306-yy,cream);line(r,639+d,306+yy,639+d,306+yy,cream);}
        box(r,452,289,13,13,amber);box(r,567,258,13,13,amber);box(r,567,343,13,13,amber);
        box(r,750,289,13,13,cream);box(r,689,254,13,13,cream);box(r,689,343,13,13,cream);
        text(r,352,446,l->valid?"CARTRIDGE READY":"CARTRIDGE NEEDED",2,l->valid?amber:cream,35);
        text(r,352,479,l->valid?base_name(l->rom):"Choose your original USA ROM to play.",1,muted,65);
    }else{
        const char*title=l->page==ISSD_LAUNCHER_GRAPHICS?"PRESENTATION":l->page==ISSD_LAUNCHER_CONTROLS?"CONTROLS":
            l->page==ISSD_LAUNCHER_GAMEPLAY?"MATCH OPTIONS":l->page==ISSD_LAUNCHER_MODS?"MOD PACKS":"CARTRIDGE LIBRARY";
        text(r,354,150,title,2,amber,35);
        for(int i=l->scroll;i<rows(l)&&i<l->scroll+8;i++){char caption[96];label(l,i,caption,sizeof caption);int y=190+(i-l->scroll)*36;
            if(i==l->focus)box(r,348,y-5,576,30,(SDL_Color){38,61,72,255});
            text(r,360,y,caption,2,i==l->focus?amber:cream,34);}
        if(l->page==ISSD_LAUNCHER_CONTROLS)text(r,354,494,"Full button remapping is in the in-game overlay.",1,muted,65);
        else if(l->page==ISSD_LAUNCHER_BROWSER)text(r,354,494,l->folder,1,muted,65);
        else text(r,354,494,"LEFT / RIGHT TO CHANGE    A TO SELECT",1,muted,65);
    }
    line(r,36,517,924,517,muted);
    text(r,36,533,l->status,1,amber,110);
    text(r,36,565,"A / ENTER SELECT     B / ESC BACK",2,cream,55);
}
static void click(Launcher*l,SDL_Window*w,int x,int y){int ww,wh,ow,oh;SDL_GetWindowSize(w,&ww,&wh);SDL_GetRendererOutputSize(l->renderer,&ow,&oh);
    if(!ww||!wh)return;x=(int)(((float)x*ow/ww-l->viewport.x)/l->scale);y=(int)(((float)y*oh/wh-l->viewport.y)/l->scale);
    if(x>=30&&x<310&&y>=184&&y<485){l->page=ISSD_LAUNCHER_HOME;l->focus=(y-184)/43;activate(l);return;}
    if(l->page!=ISSD_LAUNCHER_HOME&&x>=348&&x<924&&y>=185&&y<478){int row=l->scroll+(y-185)/36;
        if(row<rows(l)){l->focus=row;activate(l);}}
}
int issd_launcher_run(IssdConfig*cfg,char*rom,size_t cap,SDL_Window**window,SDL_Renderer**renderer,const char*capture,const char*actions){
    if(SDL_InitSubSystem(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER|SDL_INIT_TIMER))return -1;
    if(!*window)*window=SDL_CreateWindow("ISSD Native - Matchday",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        cfg->window_width>0?cfg->window_width:1280,cfg->window_height>0?cfg->window_height:800,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if(!*window)return -1;
    if(!*renderer)*renderer=SDL_CreateRenderer(*window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!*renderer)*renderer=SDL_CreateRenderer(*window,-1,SDL_RENDERER_SOFTWARE);
    if(!*renderer)return -1;
    Launcher*l=calloc(1,sizeof *l);if(!l)return -1;l->cfg=cfg;l->rom=rom;l->capacity=cap;l->renderer=*renderer;
    l->valid=issd_launcher_rom_ok(rom,l->status,sizeof l->status);
    if(l->valid)snprintf(l->status,sizeof l->status,"Cartridge verified. Ready for kickoff.");
    const char*home=getenv("HOME");
#ifdef _WIN32
    home=getenv("USERPROFILE");
#endif
    snprintf(l->folder,sizeof l->folder,"%s",home?home:".");
    for(char*p=l->folder;*p;p++)if(*p=='\\')*p='/';
    issd_input_init();SDL_EventState(SDL_DROPFILE,SDL_ENABLE);
    FILE*script=actions?fopen(actions,"r"):NULL;if(actions&&!script){free(l);return -1;}
    Uint32 repeat_at=0;int held=0;bool was_suspended=false;
    while(!l->done){SDL_Event e;
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT){l->done=true;break;}
            if(e.type==SDL_APP_WILLENTERBACKGROUND){l->suspended=true;issd_input_block_held();}
            if(e.type==SDL_APP_DIDENTERFOREGROUND)l->suspended=false;
            if(e.type==SDL_CONTROLLERDEVICEADDED)issd_input_add(e.cdevice.which);
            if(e.type==SDL_CONTROLLERDEVICEREMOVED)issd_input_remove(e.cdevice.which);
            if(e.type==SDL_KEYDOWN&&!e.key.repeat){switch(e.key.keysym.sym){
                case SDLK_UP:action(l,"UP");break;case SDLK_DOWN:action(l,"DOWN");break;
                case SDLK_LEFT:action(l,"LEFT");break;case SDLK_RIGHT:action(l,"RIGHT");break;
                case SDLK_RETURN:case SDLK_SPACE:action(l,"CONFIRM");break;case SDLK_ESCAPE:case SDLK_AC_BACK:action(l,"BACK");break;}}
            if(e.type==SDL_CONTROLLERBUTTONDOWN){switch(e.cbutton.button){case SDL_CONTROLLER_BUTTON_A:case SDL_CONTROLLER_BUTTON_START:action(l,"CONFIRM");break;
                case SDL_CONTROLLER_BUTTON_B:case SDL_CONTROLLER_BUTTON_BACK:action(l,"BACK");break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:action(l,"UP");break;case SDL_CONTROLLER_BUTTON_DPAD_DOWN:action(l,"DOWN");break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:action(l,"LEFT");break;case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:action(l,"RIGHT");break;}}
            if(e.type==SDL_MOUSEBUTTONDOWN&&e.button.which!=SDL_TOUCH_MOUSEID&&e.button.button==SDL_BUTTON_LEFT)click(l,*window,e.button.x,e.button.y);
            if(e.type==SDL_FINGERDOWN){int ww,wh;SDL_GetWindowSize(*window,&ww,&wh);click(l,*window,(int)(e.tfinger.x*ww),(int)(e.tfinger.y*wh));}
            if(e.type==SDL_MOUSEWHEEL)action(l,e.wheel.y>0?"UP":"DOWN");
            if(e.type==SDL_DROPFILE){choose(l,e.drop.file);SDL_free(e.drop.file);}
        }
        if(issd_android_is_finishing()){l->done=true;break;}
        if(l->picking){char path[1024];if(issd_android_is_picker_cancelled()){l->picking=false;snprintf(l->status,sizeof l->status,"Cartridge selection cancelled.");}
            else if(issd_android_rom_generation()!=l->import_generation){
                l->picking=false;
                if(join(path,sizeof path,issd_android_external_dir(),"isss_deluxe.sfc"))choose(l,path);
                else snprintf(l->status,sizeof l->status,"Imported cartridge path is too long.");
            }}
        if(l->suspended){was_suspended=true;SDL_Delay(16);continue;}
        if(was_suspended){held=0;repeat_at=SDL_GetTicks()+300;was_suspended=false;}
        int direction=0;for(int p=0;p<4;p++){SDL_GameController*c=issd_input_controller(p);if(!c)continue;
            int axis=SDL_GameControllerGetAxis(c,SDL_CONTROLLER_AXIS_LEFTY);if(axis<-16000)direction=-1;else if(axis>16000)direction=1;}
        Uint32 now=SDL_GetTicks();if(direction&&(direction!=held||(Sint32)(now-repeat_at)>=0)){
            action(l,direction<0?"UP":"DOWN");repeat_at=now+(direction!=held?330:120);}held=direction;
        if(script){char command[80];if(fgets(command,sizeof command,script)){command[strcspn(command,"\r\n")]=0;action(l,command);}else if(!capture)l->done=true;}
        if(!capture&&((SDL_GetWindowFlags(*window)&SDL_WINDOW_FULLSCREEN_DESKTOP)!=0)!=cfg->fullscreen)
            SDL_SetWindowFullscreen(*window,cfg->fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);
        draw(l,*window);
        if(capture&&(!script||feof(script))){int w,h;SDL_GetRendererOutputSize(*renderer,&w,&h);
            SDL_Surface*surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);
            if(!surface||SDL_RenderReadPixels(*renderer,NULL,SDL_PIXELFORMAT_ARGB8888,surface->pixels,surface->pitch)||SDL_SaveBMP(surface,capture)){
                if(surface)SDL_FreeSurface(surface);if(script)fclose(script);free(l);return -1;}
            SDL_FreeSurface(surface);l->done=true;}
        SDL_RenderPresent(*renderer);SDL_Delay(8);
    }
    if(script)fclose(script);
    int result=l->play?1:0;
    if(!capture)issd_config_save(cfg,NULL);
    printf("[Launcher] %s; page=%d; focus=%d; cartridge=%s\n",result?"play":"quit",l->page,l->focus,l->valid?"verified":"missing");
    SDL_RenderSetViewport(*renderer,NULL);SDL_RenderSetScale(*renderer,1,1);
    issd_input_block_held();free(l);return result;
}
