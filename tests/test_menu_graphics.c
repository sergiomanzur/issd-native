/* Share the real menu link harness, retaining its existing native tests. */
#define main existing_menu_test_main
#include "test_menu_internal_res_native.c"
#undef main
#include <stdlib.h>
#include <limits.h>

static void preview_ppm(const char *path, const uint32_t *pixels, int width, int height) {
    FILE *file=fopen(path,"wb"); assert(file);
    fprintf(file,"P6\n%d %d\n255\n",width,height);
    for(int i=0;i<width*height;i++) {
        fputc((pixels[i]>>16)&255,file); fputc((pixels[i]>>8)&255,file); fputc(pixels[i]&255,file);
    }
    assert(fclose(file)==0);
}

int main(void) {
    issd_config_init_defaults(&g_issd_config);
    issd_config_set_default_path("graphics-test.ini");
    issd_menu_init(); issd_menu_open();
    issd_menu_set_save_context_callback(prepare_context);
    g_overlay_menu.current_item = 20;
    issd_menu_confirm();
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_GRAPHICS);
    assert(prepared_contexts == 0);
    issd_menu_navigate_up(); assert(g_overlay_menu.current_item == 19);
    issd_menu_navigate_down(); assert(g_overlay_menu.current_item == 0);
    g_issd_config.scaling_filter = ISSD_FILTER_SHARP;
    g_issd_config.internal_res = ISSD_RES_1X;
    for (int row=0; row<11; row++) {
        g_overlay_menu.current_item = row;
        issd_menu_navigate_right();
        IssdConfig loaded; issd_config_init_defaults(&loaded);
        assert(issd_config_load(&loaded,"graphics-test.ini"));
        switch(row) {
        case 0: assert(loaded.output_resolution == g_issd_config.output_resolution); break;
        case 1: assert(loaded.aspect_ratio == g_issd_config.aspect_ratio); break;
        case 2: assert(loaded.integer_scaling == g_issd_config.integer_scaling); break;
        case 3: assert(loaded.scaling_filter == g_issd_config.scaling_filter); g_issd_config.scaling_filter = ISSD_FILTER_SHARP; break;
        case 4: assert(loaded.internal_res == g_issd_config.internal_res && loaded.internal_res == ISSD_RES_2X); break;
        case 5: assert(loaded.overlay_scale == g_issd_config.overlay_scale); break;
        case 6: assert(loaded.color_boost == g_issd_config.color_boost); break;
        case 7: assert(loaded.ball_shadow == g_issd_config.ball_shadow); break;
        case 8: assert(loaded.player_markers == g_issd_config.player_markers); break;
        case 9: assert(loaded.player_names == g_issd_config.player_names); break;
        case 10: assert(loaded.radar_scale == g_issd_config.radar_scale); break;
        }
    }
    assert(prepared_contexts == 0);
    /* The animation row switches and saves without touching game context. */
    g_overlay_menu.current_item = 17;
    assert(!g_issd_config.enhanced_running_animation);
    issd_menu_navigate_right();
    assert(g_issd_config.enhanced_running_animation);
    IssdConfig animation_loaded;
    assert(issd_config_load(&animation_loaded,"graphics-test.ini"));
    assert(animation_loaded.enhanced_running_animation);
    issd_menu_navigate_left();
    assert(!g_issd_config.enhanced_running_animation);
    issd_menu_confirm();
    assert(g_issd_config.enhanced_running_animation && prepared_contexts == 0);
    assert(issd_config_load(&animation_loaded,"graphics-test.ini"));
    assert(animation_loaded.enhanced_running_animation);
    g_overlay_menu.current_item=18;
    g_issd_config.crt_strength=100;
    for(int i=0;i<5;i++) {
        issd_menu_navigate_right();
        assert(g_issd_config.crt_strength==i*25);
        assert(issd_config_load(&animation_loaded,"graphics-test.ini"));
        assert(animation_loaded.crt_strength==i*25 && !animation_loaded.ball_outline);
    }
    g_overlay_menu.current_item = 0;
    /* New presentation rows persist without preparing emulation context. */
    for (int row=11; row<=13; row++) {
        g_overlay_menu.current_item = row;
        issd_menu_navigate_right();
        IssdConfig loaded; issd_config_init_defaults(&loaded);
        assert(issd_config_load(&loaded,"graphics-test.ini"));
        if(row==11) assert(loaded.hud_scale==2);
        if(row==12) assert(loaded.radar_position==1);
        if(row==13) assert(loaded.radar_opacity==100);
    }
    g_overlay_menu.current_item=14;
    issd_menu_confirm();
    assert(issd_config_visual_preset_id(&g_issd_config)==ISSD_VISUAL_ORIGINAL);
    assert(!g_issd_config.enhanced_running_animation);
    issd_menu_confirm();
    assert(issd_config_visual_preset_id(&g_issd_config)==ISSD_VISUAL_SHARP);
    assert(!g_issd_config.enhanced_running_animation);
    issd_menu_confirm();
    assert(issd_config_visual_preset_id(&g_issd_config)==ISSD_VISUAL_ENHANCED);
    assert(g_issd_config.enhanced_running_animation);
    g_overlay_menu.current_item=15; issd_menu_confirm();
    assert(g_overlay_menu.page==ISSD_MENU_PAGE_GRAPHICS_PREVIEW);
    uint32_t preview[400*224], original[400*224];
    g_issd_config.overlay_scale=1;
    issd_menu_render_display(preview,400,224);
    /* Optional artifacts let reviewers inspect the real native renderer. */
    if(getenv("ISSD_MENU_PREVIEW_ARTIFACT")) {
        preview_ppm("preview-sample.ppm",preview,400,224);
        uint32_t *display=malloc(1920*1080*sizeof *display); assert(display);
        g_issd_config.overlay_scale=0;
        issd_menu_render_display(display,1920,1080);
        preview_ppm("preview-display.ppm",display,1920,1080);
        free(display); g_issd_config.overlay_scale=1;
    }
    issd_menu_navigate_up(); issd_menu_navigate_down();
    issd_menu_navigate_left(); issd_menu_navigate_right();
    assert(g_overlay_menu.page==ISSD_MENU_PAGE_GRAPHICS_PREVIEW && g_overlay_menu.current_item==0);
    int pitch_pixels=0;
    for(int i=0;i<400*224;i++) if(preview[i]==0xFF207438) pitch_pixels++;
    assert(pitch_pixels>1000);
    issd_config_visual_preset(&g_issd_config,ISSD_VISUAL_ORIGINAL);
    issd_menu_render_display(original,400,224);
    assert(memcmp(preview,original,sizeof preview)!=0);
    issd_config_visual_preset(&g_issd_config,ISSD_VISUAL_SHARP);
    issd_menu_render_display(preview,400,224);
    assert(memcmp(preview,original,sizeof preview)!=0);
    g_issd_config.color_boost=true;
    issd_menu_render_display(preview,400,224);
    g_issd_config.color_boost=false;
    issd_menu_render_display(original,400,224);
    assert(memcmp(preview,original,sizeof preview)!=0);
    g_issd_config.scaling_filter=ISSD_FILTER_CRT;
    g_issd_config.crt_strength=0;
    issd_menu_render_display(original,400,224);
    g_issd_config.crt_strength=100;
    issd_menu_render_display(preview,400,224);
    assert(memcmp(preview,original,sizeof preview)!=0);
    uint32_t tiny_preview[6]={0x12345678,0,0,0,0,0x87654321};
    issd_menu_render_display(tiny_preview+1,2,2);
    assert(tiny_preview[0]==0x12345678 && tiny_preview[5]==0x87654321);
    issd_menu_render_display(preview,400,120);
    issd_menu_cancel();
    assert(g_overlay_menu.page==ISSD_MENU_PAGE_GRAPHICS && g_overlay_menu.current_item==15);
    g_issd_config.output_resolution=3; g_issd_config.internal_res=ISSD_RES_4X;
    g_issd_config.overlay_scale=4; g_issd_config.integer_scaling=true; g_issd_config.scanlines=true;
    g_issd_config.window_width=1234; g_issd_config.window_height=777;
    g_issd_config.fullscreen=true; g_issd_config.vsync=false;
    g_issd_config.enhanced_running_animation=true;
    g_overlay_menu.current_item=16; issd_menu_confirm();
    assert(issd_config_visual_preset_id(&g_issd_config)==ISSD_VISUAL_ORIGINAL);
    assert(g_issd_config.output_resolution==0 && g_issd_config.internal_res==ISSD_RES_1X);
    assert(g_issd_config.overlay_scale==0 && !g_issd_config.integer_scaling && !g_issd_config.scanlines);
    assert(!g_issd_config.enhanced_running_animation);
    assert(g_issd_config.window_width==1234 && g_issd_config.window_height==777);
    assert(g_issd_config.fullscreen && !g_issd_config.vsync && prepared_contexts==0);
    IssdConfig reset; issd_config_init_defaults(&reset);
    assert(issd_config_load(&reset,"graphics-test.ini"));
    assert(issd_config_visual_preset_id(&reset)==ISSD_VISUAL_ORIGINAL);
    g_issd_config.fullscreen=false;
    g_overlay_menu.current_item=0; g_overlay_menu.scroll=0;
    issd_menu_render_display(preview,400,224);
    issd_menu_notify("Timer test", 120);
    /* Native-output overlay is unaffected by any game filter. Every canvas
       pixel becomes an exact 3x3 block, and mouse coordinates select it. */
    uint32_t *large = malloc(1600*900*sizeof *large);
    uint32_t *other = malloc(1600*900*sizeof *other);
    assert(large && other);
    g_issd_config.overlay_scale = 3;
    g_issd_config.scaling_filter = ISSD_FILTER_NEAREST;
    issd_menu_render_display(large,1600,900);
    g_issd_config.scaling_filter = ISSD_FILTER_LINEAR;
    issd_menu_render_display(other,1600,900);
    assert(g_overlay_menu.status_timer == 120);
    /* A panel border pixel and its complete scaled block stay identical. */
    int ox=200,oy=114;
    for(int dy=0;dy<3;dy++) for(int dx=0;dx<3;dx++) {
        assert(large[(oy+2*3+dy)*1600+ox+8*3+dx] == 0xFF00E5FF);
        assert(other[(oy+2*3+dy)*1600+ox+8*3+dx] == 0xFF00E5FF);
    }
    bool outline = g_issd_config.color_boost;
    assert(issd_menu_handle_display_click(ox+40*3,oy+(2+18+6*14)*3,1600,900));
    assert(g_overlay_menu.current_item == 6 && g_issd_config.color_boost != outline);
    /* Asymmetric physical safe insets shift the visual center and clicks
       by exactly the same amount. */
    issd_menu_set_safe_insets(100,0,0,0);
    issd_menu_render_display(large,1600,900);
    assert(large[(oy+2*3)*1600+ox+50+8*3] == 0xFF00E5FF);
    outline = g_issd_config.color_boost;
    issd_menu_handle_display_click(ox+50+40*3,oy+(2+18+6*14)*3,1600,900);
    assert(g_overlay_menu.current_item == 6 && g_issd_config.color_boost != outline);
    issd_menu_set_safe_insets(0,0,0,0);
    /* Back keeps its appended main row visible, and wrap clears scroll. */
    issd_menu_cancel(); assert(g_overlay_menu.current_item == 20);
    assert(g_overlay_menu.scroll == 6);
    issd_menu_navigate_down(); assert(g_overlay_menu.current_item == 0 && g_overlay_menu.scroll == 0);
    issd_menu_navigate_up(); assert(g_overlay_menu.current_item == 20 && g_overlay_menu.scroll == 6);
    g_issd_config.overlay_scale = 1;
    issd_menu_render_display(large,400,224);
    issd_menu_handle_display_click(40,2+18+(20-6)*12,400,224);
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_GRAPHICS);
    /* Short displays scroll graphics rows too; Back remains clickable. */
    issd_menu_render_display(large,400,120);
    issd_menu_navigate_up();
    assert(g_overlay_menu.current_item == 19 && g_overlay_menu.scroll == 15);
    issd_menu_render_display(large,400,120);
    issd_menu_handle_display_click(40,2+18+4*14,400,120);
    assert(g_overlay_menu.page == ISSD_MENU_PAGE_MAIN);
    issd_menu_close(); issd_menu_render_display(large,400,224);
    for(int i=0;i<400*224;i++) assert(large[i] == 0);
    issd_menu_notify("Sharp notice", 2);
    assert(issd_menu_has_notification());
    issd_menu_render_notification_display(large,400,224);
    int ink = 0; for(int i=0;i<400*224;i++) if (large[i]) ink++;
    assert(ink > 0 && ink < 400*224);
    issd_menu_render_display(large,400,224);
    issd_menu_render_notification_display(large,400,224);
    assert(issd_menu_has_notification()); /* Presentation does not consume time. */
    issd_menu_tick_notification();
    assert(issd_menu_has_notification());
    issd_menu_tick_notification();
    assert(!issd_menu_has_notification());
    issd_menu_render_display(large,400,224);
    issd_menu_render_notification_display(large,400,224);
    for(int i=0;i<400*224;i++) assert(large[i] == 0);
    /* Notifications stay above the bottom inset and inside horizontal
       safe bounds. Rendering leaves their simulation timer unchanged. */
    issd_menu_set_safe_insets(100,0,0,100);
    issd_menu_notify("Safe notice",2);
    issd_menu_render_display(large,1600,900);
    issd_menu_render_notification_display(large,1600,900);
    int safe_ink=0;
    for(int y=0;y<900;y++) for(int x=0;x<1600;x++) if(large[y*1600+x]) {
        assert(x>=100 && y<800); safe_ink++;
    }
    assert(safe_ink>0 && issd_menu_has_notification());
    /* Pathological insets still leave one safe pixel, without writing
       outside either output buffer. Negative inset values reset to zero. */
    issd_menu_open();
    issd_menu_notify("Protected status",19);
    issd_menu_set_safe_insets(INT_MAX,INT_MAX,INT_MAX,INT_MAX);
    uint32_t guard[6] = {0x12345678,0,0,0,0,0x87654321};
    issd_menu_render_display(guard+1,2,2);
    issd_menu_render_notification_display(guard+1,2,2);
    assert(guard[0]==0x12345678 && guard[5]==0x87654321);
    assert(g_overlay_menu.status_timer==19);
    issd_menu_handle_display_click(1,1,2,2);
    issd_menu_set_safe_insets(-1,-1,-1,-1);
    issd_menu_render_display(guard+1,2,2);
    issd_menu_render_notification_display(guard+1,2,2);
    assert(guard[0]==0x12345678 && guard[5]==0x87654321);
    issd_menu_set_safe_insets(0,0,0,0);
    free(large); free(other);
    return 0;
}
