#include "issd_match_menu.h"
#include "issd_match.h"
#include "issd_menu.h"
#include "issd_save.h"
#include <stdio.h>
#include <string.h>

#define ROWS 14
#define VISIBLE 13
static bool favorite_available;
static const char *presets[] = {"ORIGINAL", "CLASSIC", "CASUAL", "CUSTOM"};
static void save_config(void) {
    if (!issd_config_save(&g_issd_config, NULL)) issd_menu_notify("Settings could not be saved", 240);
}
static void refresh_favorite(void) {
    char info[128]; favorite_available = issd_save_match_favorite_info(info, sizeof info);
}
void issd_match_menu_open(void) {
    g_overlay_menu.page = ISSD_MENU_PAGE_MATCH; g_overlay_menu.current_item = g_overlay_menu.scroll = 0;
    refresh_favorite();
}
void issd_match_menu_step(int direction) {
    int row = (g_overlay_menu.current_item + direction + ROWS) % ROWS;
    g_overlay_menu.current_item = row;
    if (row < g_overlay_menu.scroll) g_overlay_menu.scroll = row;
    if (row >= g_overlay_menu.scroll + VISIBLE) g_overlay_menu.scroll = row - VISIBLE + 1;
}
void issd_match_menu_adjust(int direction) {
    int row = g_overlay_menu.current_item;
    if (row == 5) {
        g_issd_config.match_preset = (g_issd_config.match_preset + direction + 4) % 4;
        save_config();
    } else if (row >= 7 && row <= 12) {
        IssdMatchRules rules; issd_match_selected_rules(&g_issd_config, &rules);
        int *fields[] = {&rules.duration,&rules.difficulty,&rules.offside,&rules.fouls,&rules.cards,&rules.extra_time};
        int counts[] = {3,5,2,2,2,2}; int i = row - 7;
        *fields[i] = (*fields[i] + direction + counts[i]) % counts[i];
        g_issd_config.match_custom = rules; g_issd_config.match_preset = ISSD_MATCH_CUSTOM;
        save_config();
    }
}
void issd_match_menu_cancel(void) {
    g_overlay_menu.page = ISSD_MENU_PAGE_GAMEPLAY; g_overlay_menu.current_item = 2; g_overlay_menu.scroll = 0;
}
void issd_match_menu_confirm(void) {
    int row = g_overlay_menu.current_item;
    if (row == 13) { issd_match_menu_cancel(); return; }
    if (row == 5 || (row >= 7 && row <= 12)) { issd_match_menu_adjust(1); return; }
    bool ok = false, resume = true; const char *message = "Exhibition restored";
    switch (row) {
        case 0: ok = issd_match_rematch(); message = "Exhibition kickoff restored"; break;
        case 1: ok = issd_match_mark_drill(issd_match_frame_healthy()); resume = false; message = "Drill checkpoint marked"; break;
        case 2: ok = issd_match_restart_drill(); message = "Drill checkpoint restored"; break;
        case 3: ok = issd_match_save_favorite(); resume = false; message = "Favorite exhibition saved"; if (ok) refresh_favorite(); break;
        case 4: ok = issd_match_play_favorite(); break;
        case 6: ok = issd_match_start_rules(); message = "Starting with selected rules"; break;
        default: return;
    }
    if (!ok) { issd_menu_notify(issd_match_error(), 240); return; }
    issd_menu_notify(message, 150);
    if (resume) issd_menu_close();
}
void issd_match_menu_click(int px, int py, int x, int y) {
    int row = g_overlay_menu.scroll + (py - y - 22)/12;
    if (py >= y + 22 && py < y + 178 && row < ROWS) {
        g_overlay_menu.current_item = row;
        if (px < x + 30) issd_match_menu_adjust(-1); else issd_match_menu_confirm();
    } else if (py >= y + 190) issd_match_menu_cancel();
}
void issd_match_menu_render(uint32_t *fb, int w, int h, int x, int y,
    void (*text)(uint32_t *, int, int, int, int, const char *, uint32_t)) {
    text(fb,w,h,x+48,y+4,"MATCH SHORTCUTS",0xffffd700);
    IssdMatchRules rules; issd_match_selected_rules(&g_issd_config, &rules);
    const char *labels[] = {"Instant rematch", "Mark drill checkpoint", "Restart drill", "Save favorite setup",
        "Play favorite", "", "Start with selected rules"};
    for (int row = g_overlay_menu.scroll; row < ROWS && row < g_overlay_menu.scroll + VISIBLE; ++row) {
        char line[29]; bool available = true;
        if (row < 5) {
            available = row == 0 ? issd_match_has_kickoff() : row == 1 ? issd_match_is_live() :
                row == 2 ? issd_match_has_drill() : row == 3 ? issd_match_has_setup() : favorite_available;
            snprintf(line,sizeof line,"%s%s",labels[row],available?"":" (---)");
        } else if (row == 5) snprintf(line,sizeof line,"Rules: <%s>",presets[g_issd_config.match_preset]);
        else if (row == 6) { snprintf(line,sizeof line,"%s",labels[row]); available = issd_match_has_setup(); }
        else if (row == 7) snprintf(line,sizeof line,"Time: %d minutes",3 + 2*rules.duration);
        else if (row == 8) snprintf(line,sizeof line,"Difficulty: level %d",1 + rules.difficulty);
        else if (row == 9) snprintf(line,sizeof line,"Offside: %s",rules.offside?"OFF":"ON");
        else if (row == 10) snprintf(line,sizeof line,"Fouls: %s",rules.fouls?"OFF":"ON");
        else if (row == 11) snprintf(line,sizeof line,"Cards: %s",rules.cards?"OFF":"ON");
        else if (row == 12) snprintf(line,sizeof line,"Extra time: %s",rules.extra_time?"OFF":"ON");
        else snprintf(line,sizeof line,"Back");
        if (row >= 7 && row <= 12 && g_issd_config.match_preset == ISSD_MATCH_ORIGINAL)
            snprintf(line,sizeof line,"%s: original setup",row==7?"Time":row==8?"Difficulty":row==9?"Offside":row==10?"Fouls":row==11?"Cards":"Extra time");
        bool selected = row == g_overlay_menu.current_item;
        uint32_t color = selected ? 0xff00ff66 : available ? 0xffe0e0e0 : 0xff888888;
        if (selected) text(fb,w,h,x+4,y+22+(row-g_overlay_menu.scroll)*12,">",color);
        text(fb,w,h,x+14,y+22+(row-g_overlay_menu.scroll)*12,line,color);
    }
    if (g_overlay_menu.status_timer > 0 && g_overlay_menu.status_message[0]) {
        char first[29], second[29];
        const char *message = g_overlay_menu.status_message;
        snprintf(first,sizeof first,"%.28s",message);
        snprintf(second,sizeof second,"%.28s",strlen(message) > 28 ? message+28 : "");
        text(fb,w,h,x+8,y+190,first,0xffffaa00);
        text(fb,w,h,x+8,y+205,second,0xffffaa00);
    } else {
        text(fb,w,h,x+8,y+190,"Rules: next exhibition start",0xffb0b0b0);
        text(fb,w,h,x+8,y+205,"A:Set  < >:Edit  ESC:Back",0xff888888);
    }
}
