"""A checked load must not reset the animation state it just restored."""
from pathlib import Path
import subprocess
from test_config_persistence import compile_c
from test_snapshot_transactional import function


def test_load_selects_local_hd_context_without_discarding_restored_animation(tmp_path):
    root=Path(__file__).resolve().parents[1]
    source=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
static struct { void *cart,*ppu; } snes,*g_snes=&snes;
static uint8_t g_ram[0x20000],*g_rom_data;
static size_t g_rom_size;
static int g_render_stadium_id=-1,hd_id=-1;
static unsigned g_render_stadium_generation,hd_generation,resets;
static uint16_t g_pad1_state;
static bool g_touch_release_guard,g_legacy_quick_confirm,g_frame_healthy;
static bool issd_stadium_scene_restore(void *cart,const uint8_t *ram,const uint8_t *rom,size_t size) {
 (void)cart;(void)ram;(void)rom;(void)size;return true;
}
const void *issd_stadium_profile(unsigned id) { return id==8?&snes:NULL; }
unsigned issd_stadium_generation(void) { return 42; }
static void Die(const char *text) { (void)text;abort(); }
static void issd_hd_set_stadium_context(int id,unsigned generation) { hd_id=id;hd_generation=generation; }
static void issd_widescreen_reset(void) { resets++; }
static void issd_bugfix_keeper_begin_loop(void) {}
static void issd_input_block_held(void) {}
static void issd_campaign_reset(void) {}
''' + function((root/'ISSDNative/main.c').read_text(),'static void IssdSaveLoaded(')+r'''
int main(void) {
 g_ram[0x1fa2]=8;g_ram[0x70]=19;
 IssdSaveLoaded();assert(!resets);
 assert(g_render_stadium_id==8&&hd_id==8&&g_render_stadium_generation==42&&hd_generation==42);
 g_ram[0x1fa2]=0;IssdSaveLoaded();assert(!resets);
 assert(g_render_stadium_id==-1&&hd_id==-1&&!g_render_stadium_generation&&!hd_generation);
 return 0;
}
'''
    # Once reset is removed from the real callback the stub itself is unused.
    source=source.replace('static void issd_widescreen_reset','void issd_widescreen_reset')
    path=tmp_path/'load.c';path.write_text(source)
    executable=tmp_path/'load.exe';compile_c(executable,root,[path],[])
    result=subprocess.run([str(executable)],capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
